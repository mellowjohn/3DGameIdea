#include "engine/ui/world_forge_editor.h"

#include "engine/automation/world_forge_commands.h"
#include "engine/core/error.h"
#include "engine/core/id_slug.h"
#include "engine/ui/world_forge_graph_camera.h"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <unordered_set>
#include <vector>

namespace engine {
namespace {

using ListKind = WorldForgeEditorSession::ListKind;

EngineError editor_error(std::string code, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Validation, "world_forge_editor",
        std::move(message), ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

// --- reload/save helpers ---------------------------------------------------

template <typename AssetT>
Result<void> reload_kind(const std::filesystem::path& project_root, const std::filesystem::path& path,
    const char* kind, const char* source_name, AssetT& out) {
    if (!std::filesystem::exists(path)) {
        out = AssetT{};
        return Result<void>::success();
    }
    nlohmann::json params{{"action", "get"}, {"kind", kind}};
    const auto response = apply_world_forge_operation(project_root, params);
    if (response.exit_code != ExitCode::Success) {
        if (!response.diagnostics.empty()) return Result<void>::failure(response.diagnostics.front());
        return Result<void>::failure(
            editor_error("WORLD-FORGE-EDITOR-GET", response.summary, "Check the World Forge asset file."));
    }
    const auto found = response.metadata.find("content");
    if (found == response.metadata.end()) {
        return Result<void>::failure(editor_error(
            "WORLD-FORGE-EDITOR-CONTENT", "World Forge get response missing content", "Retry Reload."));
    }
    auto parsed = AssetT::parse(found->second, source_name);
    if (!parsed) return Result<void>::failure(parsed.error());
    out = std::move(parsed.value());
    return Result<void>::success();
}

template <typename AssetT>
Result<void> apply_kind(const std::filesystem::path& project_root, const char* kind, const AssetT& asset) {
    nlohmann::json params{{"action", "apply"}, {"kind", kind}, {"source", asset.to_json()}};
    const auto response = apply_world_forge_operation(project_root, params);
    if (response.exit_code == ExitCode::Success) return Result<void>::success();
    if (!response.diagnostics.empty()) return Result<void>::failure(response.diagnostics.front());
    return Result<void>::failure(
        editor_error("WORLD-FORGE-EDITOR-APPLY", response.summary, "Fix the validation error and retry Save."));
}

// --- small text helpers -----------------------------------------------------

std::string trim(const std::string& text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return text.substr(begin, end - begin);
}

std::string join_csv(const std::vector<std::string>& values) {
    std::string out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out += ", ";
        out += values[i];
    }
    return out;
}

std::vector<std::string> split_csv(const std::string& text) {
    std::vector<std::string> out;
    std::string current;
    for (char c : text) {
        if (c == ',') {
            auto piece = trim(current);
            if (!piece.empty()) out.push_back(std::move(piece));
            current.clear();
        } else {
            current += c;
        }
    }
    auto piece = trim(current);
    if (!piece.empty()) out.push_back(std::move(piece));
    return out;
}

std::string join_lines(const std::vector<std::string>& values) {
    std::string out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out += "\n";
        out += values[i];
    }
    return out;
}

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> out;
    std::string current;
    for (char c : text) {
        if (c == '\n') {
            auto piece = trim(current);
            if (!piece.empty()) out.push_back(std::move(piece));
            current.clear();
        } else if (c != '\r') {
            current += c;
        }
    }
    auto piece = trim(current);
    if (!piece.empty()) out.push_back(std::move(piece));
    return out;
}

/// Humanize slug ids (Twine imports) and keep a short title for graph cards.
std::string dialogue_node_title(const WorldForgeDialogueNode& node) {
    std::string title = node.id;
    for (char& c : title) {
        if (c == '_') c = ' ';
    }
    title = trim(title);
    if (title.empty()) title = node.id;
    // Prefer a short first phrase from the line when the id is a long choice-slug.
    if (title.size() > 28 && !node.line.empty()) {
        std::string line = node.line;
        const auto nl = line.find('\n');
        if (nl != std::string::npos) line = line.substr(0, nl);
        line = trim(line);
        if (line.size() > 4) {
            // Strip leading stage directions in quotes/parentheses lightly.
            title = std::move(line);
        }
    }
    if (title.size() > 32) title = title.substr(0, 29) + "...";
    return title;
}

std::string ellipsize_fit(const std::string& text, float max_width) {
    if (text.empty()) return text;
    if (ImGui::CalcTextSize(text.c_str()).x <= max_width) return text;
    std::string out = text;
    while (out.size() > 1) {
        out.pop_back();
        const std::string candidate = out + "...";
        if (ImGui::CalcTextSize(candidate.c_str()).x <= max_width) return candidate;
    }
    return "...";
}

void draw_clipped_text_lines(ImDrawList* draw, const ImVec2& pad_min, const ImVec2& pad_max, const std::string& line1,
    const std::string& line2, ImU32 color1, ImU32 color2) {
    draw->PushClipRect(pad_min, pad_max, true);
    const float max_w = (std::max)(8.0f, pad_max.x - pad_min.x);
    const float line_h = ImGui::GetTextLineHeight();
    const std::string t1 = ellipsize_fit(line1, max_w);
    draw->AddText(ImVec2(pad_min.x, pad_min.y), color1, t1.c_str());
    if (!line2.empty() && pad_min.y + line_h + 2.0f < pad_max.y) {
        const std::string t2 = ellipsize_fit(line2, max_w);
        draw->AddText(ImVec2(pad_min.x, pad_min.y + line_h + 1.0f), color2, t2.c_str());
    }
    draw->PopClipRect();
}

// --- ImGui field widgets bound directly to model strings --------------------

bool draw_input_text(const char* label, std::string& value, std::size_t capacity = 256) {
    std::vector<char> buffer((std::max)(capacity, value.size() + 1), '\0');
    std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
    if (ImGui::InputText(label, buffer.data(), buffer.size())) {
        value = buffer.data();
        return true;
    }
    return false;
}

bool draw_input_text_multiline(const char* label, std::string& value, std::size_t capacity, const ImVec2& size) {
    std::vector<char> buffer((std::max)(capacity, value.size() + 1), '\0');
    std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
    if (ImGui::InputTextMultiline(label, buffer.data(), buffer.size(), size,
            ImGuiInputTextFlags_AllowTabInput)) {
        value = buffer.data();
        return true;
    }
    return false;
}

/// Labeled multiline editor for long World Forge prose (summary, dialogue line, notes, …).
bool draw_text_area(const char* label, std::string& value, float height = 96.0f, std::size_t capacity = 8192) {
    ImGui::TextUnformatted(label);
    const std::string widget_id = std::string("##ta_") + label;
    return draw_input_text_multiline(widget_id.c_str(), value, capacity, ImVec2(-1.0f, height));
}

bool draw_csv_field(const char* label, std::vector<std::string>& values, std::size_t capacity = 512) {
    std::string csv = join_csv(values);
    if (draw_input_text(label, csv, capacity)) {
        values = split_csv(csv);
        return true;
    }
    return false;
}

bool draw_double_field(const char* label, double& value) {
    float as_float = static_cast<float>(value);
    ImGui::InputFloat(label, &as_float, 0.0f, 0.0f, "%.3f");
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        value = static_cast<double>(as_float);
        return true;
    }
    return false;
}

bool draw_open_questions_field(std::vector<std::string>& values) {
    std::string joined = join_lines(values);
    if (draw_text_area("openQuestions (one per line)", joined, 100.0f, 4096)) {
        values = split_lines(joined);
        return true;
    }
    return false;
}

template <typename EnumT>
bool draw_enum_combo(const char* label, EnumT& value, std::initializer_list<EnumT> options) {
    bool changed = false;
    if (ImGui::BeginCombo(label, to_string(value))) {
        for (const EnumT option : options) {
            const bool selected = option == value;
            if (ImGui::Selectable(to_string(option), selected)) {
                if (value != option) {
                    value = option;
                    changed = true;
                }
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool draw_political_role_combo(const char* label, std::optional<WorldForgePoliticalRole>& value) {
    bool changed = false;
    const char* current = value ? to_string(*value) : "(none)";
    if (ImGui::BeginCombo(label, current)) {
        if (ImGui::Selectable("(none)", !value)) {
            if (value) {
                value.reset();
                changed = true;
            }
        }
        for (const auto role :
            {WorldForgePoliticalRole::Arena, WorldForgePoliticalRole::Faction, WorldForgePoliticalRole::Unknown}) {
            const bool selected = value && *value == role;
            if (ImGui::Selectable(to_string(role), selected)) {
                if (!value || *value != role) {
                    value = role;
                    changed = true;
                }
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

/// Dropdown for ID lookups (optional empty choice). Keeps orphan/stale values selectable.
bool draw_id_combo(const char* label, std::string& value, const std::vector<std::string>& options,
    bool allow_empty = true, const char* empty_label = "(none)") {
    bool changed = false;
    std::vector<std::string> items;
    items.reserve(options.size() + 2);
    if (allow_empty) items.emplace_back();
    bool has_current = value.empty();
    for (const auto& opt : options) {
        if (opt.empty()) continue;
        items.push_back(opt);
        if (opt == value) has_current = true;
    }
    if (!value.empty() && !has_current) items.push_back(value);

    const char* preview = value.empty() ? empty_label : value.c_str();
    if (ImGui::BeginCombo(label, preview)) {
        for (const auto& opt : items) {
            const bool is_empty = opt.empty();
            const char* text = is_empty ? empty_label : opt.c_str();
            const bool selected = value == opt;
            if (ImGui::Selectable(text, selected)) {
                if (value != opt) {
                    value = opt;
                    changed = true;
                }
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

std::vector<std::string> collect_faction_ids(const WorldForgeFactionsAsset& asset) {
    std::vector<std::string> out;
    out.reserve(asset.entities.size());
    for (const auto& e : asset.entities) out.push_back(e.id);
    return out;
}

std::vector<std::string> collect_relationship_node_ids(const WorldForgeRelationshipsAsset& asset) {
    std::vector<std::string> out;
    out.reserve(asset.nodes.size());
    for (const auto& n : asset.nodes) out.push_back(n.id);
    return out;
}

std::vector<std::string> collect_region_ids(const WorldForgeMapAsset& asset) {
    std::vector<std::string> out;
    out.reserve(asset.regions.size());
    for (const auto& r : asset.regions) out.push_back(r.id);
    return out;
}

std::vector<std::string> collect_map_endpoint_ids(const WorldForgeMapAsset& asset) {
    std::vector<std::string> out;
    out.reserve(asset.regions.size() + asset.pois.size());
    for (const auto& r : asset.regions) out.push_back(r.id);
    for (const auto& p : asset.pois) out.push_back(p.id);
    return out;
}

std::vector<std::string> collect_quest_ids(const WorldForgeQuestsAsset& asset) {
    std::vector<std::string> out;
    out.reserve(asset.quests.size());
    for (const auto& q : asset.quests) out.push_back(q.id);
    return out;
}

std::vector<std::string> collect_dialogue_tree_ids(const WorldForgeDialoguesAsset& asset) {
    std::vector<std::string> out;
    out.reserve(asset.trees.size());
    for (const auto& t : asset.trees) out.push_back(t.id);
    return out;
}

std::vector<std::string> collect_dialogue_node_ids(const WorldForgeDialogueTree& tree) {
    std::vector<std::string> out;
    out.reserve(tree.nodes.size());
    for (const auto& n : tree.nodes) out.push_back(n.id);
    return out;
}

std::vector<std::string> collect_speaker_ids(const WorldForgeRelationshipsAsset& relationships) {
    std::vector<std::string> out = collect_relationship_node_ids(relationships);
    bool has_narrator = false;
    for (const auto& id : out) {
        if (id == "narrator") {
            has_narrator = true;
            break;
        }
    }
    if (!has_narrator) out.insert(out.begin(), "narrator");
    return out;
}

std::vector<std::string> collect_endpoint_ids(const WorldForgeEditorSession& session,
    WorldForgeRelationshipEndpointTarget target) {
    if (target == WorldForgeRelationshipEndpointTarget::Faction) return collect_faction_ids(session.factions);
    return collect_relationship_node_ids(session.relationships);
}

// --- lookups -----------------------------------------------------------------

WorldForgeFactionEntity* find_faction(WorldForgeFactionsAsset& asset, const std::string& id) {
    for (auto& entity : asset.entities) {
        if (entity.id == id) return &entity;
    }
    return nullptr;
}

const WorldForgeFactionEntity* find_faction(const WorldForgeFactionsAsset& asset, const std::string& id) {
    for (const auto& entity : asset.entities) {
        if (entity.id == id) return &entity;
    }
    return nullptr;
}

WorldForgeRelationshipNode* find_node(WorldForgeRelationshipsAsset& asset, const std::string& id) {
    for (auto& node : asset.nodes) {
        if (node.id == id) return &node;
    }
    return nullptr;
}

const WorldForgeRelationshipNode* find_node(const WorldForgeRelationshipsAsset& asset, const std::string& id) {
    for (const auto& node : asset.nodes) {
        if (node.id == id) return &node;
    }
    return nullptr;
}

WorldForgeRelationshipEdge* find_edge(WorldForgeRelationshipsAsset& asset, const std::string& id) {
    for (auto& edge : asset.edges) {
        if (edge.id == id) return &edge;
    }
    return nullptr;
}

const WorldForgeRelationshipEdge* find_edge(const WorldForgeRelationshipsAsset& asset, const std::string& id) {
    for (const auto& edge : asset.edges) {
        if (edge.id == id) return &edge;
    }
    return nullptr;
}

WorldForgeRegion* find_region(WorldForgeMapAsset& asset, const std::string& id) {
    for (auto& region : asset.regions) {
        if (region.id == id) return &region;
    }
    return nullptr;
}

const WorldForgeRegion* find_region(const WorldForgeMapAsset& asset, const std::string& id) {
    for (const auto& region : asset.regions) {
        if (region.id == id) return &region;
    }
    return nullptr;
}

WorldForgePoi* find_poi(WorldForgeMapAsset& asset, const std::string& id) {
    for (auto& poi : asset.pois) {
        if (poi.id == id) return &poi;
    }
    return nullptr;
}

const WorldForgePoi* find_poi(const WorldForgeMapAsset& asset, const std::string& id) {
    for (const auto& poi : asset.pois) {
        if (poi.id == id) return &poi;
    }
    return nullptr;
}

WorldForgeMapLink* find_link(WorldForgeMapAsset& asset, const std::string& id) {
    for (auto& link : asset.links) {
        if (link.id == id) return &link;
    }
    return nullptr;
}

const WorldForgeMapLink* find_link(const WorldForgeMapAsset& asset, const std::string& id) {
    for (const auto& link : asset.links) {
        if (link.id == id) return &link;
    }
    return nullptr;
}

WorldForgeQuest* find_quest(WorldForgeQuestsAsset& asset, const std::string& id) {
    for (auto& quest : asset.quests) {
        if (quest.id == id) return &quest;
    }
    return nullptr;
}

const WorldForgeQuest* find_quest(const WorldForgeQuestsAsset& asset, const std::string& id) {
    for (const auto& quest : asset.quests) {
        if (quest.id == id) return &quest;
    }
    return nullptr;
}

WorldForgeDialogueTree* find_dialogue_tree(WorldForgeDialoguesAsset& asset, const std::string& id) {
    return asset.find_tree(id);
}

const WorldForgeDialogueTree* find_dialogue_tree(const WorldForgeDialoguesAsset& asset, const std::string& id) {
    return asset.find_tree(id);
}

bool is_valid_id_token(const std::string& id) {
    if (id.empty()) return false;
    for (unsigned char c : id) {
        if (!(std::isalnum(c) || c == '_' || c == '-')) return false;
    }
    return true;
}

std::string sanitize_id_token(std::string value) {
    return slugify_id(value);
}

std::string unique_quest_id(const WorldForgeQuestsAsset& asset, const std::string& preferred) {
    return unique_slugify_id(preferred, [&](const std::string& candidate) { return find_quest(asset, candidate) != nullptr; },
        "quest");
}

bool add_quest(WorldForgeEditorSession& session, std::string id, std::string display_name) {
    id = sanitize_id_token(std::move(id));
    if (!is_valid_id_token(id) || find_quest(session.quests, id)) return false;
    WorldForgeQuest quest;
    quest.id = id;
    quest.display_name = std::move(display_name);
    quest.kind = WorldForgeQuestKind::Side;
    quest.canon_status = WorldForgeQuestCanonStatus::Draft;
    session.quests.quests.push_back(std::move(quest));
    session.selected_id = id;
    session.dirty = true;
    session.status = "Added quest " + id;
    return true;
}

bool remove_quest(WorldForgeEditorSession& session, const std::string& id) {
    auto& quests = session.quests.quests;
    const auto it = std::find_if(quests.begin(), quests.end(), [&](const auto& q) { return q.id == id; });
    if (it == quests.end()) return false;
    quests.erase(it);
    if (session.selected_id == id) session.selected_id.clear();
    session.dirty = true;
    session.status = "Removed quest " + id;
    return true;
}

bool add_quest_objective(WorldForgeQuest& quest, std::string id, std::string summary = {}) {
    id = sanitize_id_token(std::move(id));
    if (!is_valid_id_token(id)) return false;
    for (const auto& objective : quest.objectives) {
        if (objective.id == id) return false;
    }
    WorldForgeQuestObjective objective;
    objective.id = id;
    objective.summary = std::move(summary);
    quest.objectives.push_back(std::move(objective));
    return true;
}

bool remove_quest_objective(WorldForgeQuest& quest, const std::string& id) {
    auto& objectives = quest.objectives;
    const auto it = std::find_if(objectives.begin(), objectives.end(), [&](const auto& o) { return o.id == id; });
    if (it == objectives.end()) return false;
    objectives.erase(it);
    return true;
}

bool add_quest_fork(WorldForgeQuest& quest, std::string id, std::string summary = {}) {
    id = sanitize_id_token(std::move(id));
    if (!is_valid_id_token(id)) return false;
    for (const auto& fork : quest.forks) {
        if (fork.id == id) return false;
    }
    WorldForgeQuestFork fork;
    fork.id = id;
    fork.summary = std::move(summary);
    quest.forks.push_back(std::move(fork));
    return true;
}

bool remove_quest_fork(WorldForgeQuest& quest, const std::string& id) {
    auto& forks = quest.forks;
    const auto it = std::find_if(forks.begin(), forks.end(), [&](const auto& f) { return f.id == id; });
    if (it == forks.end()) return false;
    forks.erase(it);
    return true;
}

std::string unique_faction_id(const WorldForgeFactionsAsset& asset, const std::string& preferred) {
    return unique_slugify_id(
        preferred, [&](const std::string& candidate) { return find_faction(asset, candidate) != nullptr; }, "faction");
}

bool add_faction(WorldForgeEditorSession& session, std::string id, std::string display_name, WorldForgeFactionKind kind) {
    id = sanitize_id_token(std::move(id));
    if (!is_valid_id_token(id) || find_faction(session.factions, id)) return false;
    WorldForgeFactionEntity entity;
    entity.id = id;
    entity.display_name = std::move(display_name);
    entity.kind = kind;
    entity.canon_status = WorldForgeCanonStatus::Draft;
    session.factions.entities.push_back(std::move(entity));
    session.selected_id = id;
    session.dirty = true;
    session.status = "Added faction " + id;
    return true;
}

std::string unique_region_id(const WorldForgeMapAsset& asset, const std::string& preferred) {
    return unique_slugify_id(
        preferred, [&](const std::string& candidate) { return find_region(asset, candidate) != nullptr; }, "region");
}

bool add_region(WorldForgeEditorSession& session, std::string id, std::string display_name, WorldForgeRegionKind kind) {
    id = sanitize_id_token(std::move(id));
    if (!is_valid_id_token(id) || find_region(session.map, id)) return false;
    WorldForgeRegion region;
    region.id = id;
    region.display_name = std::move(display_name);
    region.kind = kind;
    region.canon_status = WorldForgeMapCanonStatus::Draft;
    session.map.regions.push_back(std::move(region));
    session.selected_id = id;
    session.list_kind = WorldForgeEditorSession::ListKind::Regions;
    session.dirty = true;
    session.status = "Added region " + id;
    return true;
}

std::string unique_poi_id(const WorldForgeMapAsset& asset, const std::string& preferred) {
    return unique_slugify_id(
        preferred, [&](const std::string& candidate) { return find_poi(asset, candidate) != nullptr; }, "poi");
}

bool add_poi(WorldForgeEditorSession& session, std::string id, std::string display_name, WorldForgePoiKind kind,
    std::string region_id) {
    id = sanitize_id_token(std::move(id));
    if (!is_valid_id_token(id) || find_poi(session.map, id)) return false;
    if (region_id.empty() || !find_region(session.map, region_id)) return false;
    WorldForgePoi poi;
    poi.id = id;
    poi.display_name = std::move(display_name);
    poi.kind = kind;
    poi.region_id = std::move(region_id);
    poi.canon_status = WorldForgeMapCanonStatus::Draft;
    session.map.pois.push_back(std::move(poi));
    session.selected_id = id;
    session.list_kind = WorldForgeEditorSession::ListKind::Pois;
    session.dirty = true;
    session.status = "Added POI " + id;
    return true;
}

std::string unique_map_link_id(const WorldForgeMapAsset& asset, const std::string& preferred) {
    return unique_slugify_id(
        preferred, [&](const std::string& candidate) { return find_link(asset, candidate) != nullptr; }, "link");
}

bool add_map_link(WorldForgeEditorSession& session, std::string id, WorldForgeMapLinkKind kind,
    WorldForgeMapEndpointKind from_kind, std::string from_id, WorldForgeMapEndpointKind to_kind, std::string to_id) {
    id = sanitize_id_token(std::move(id));
    if (!is_valid_id_token(id) || find_link(session.map, id)) return false;
    if (from_id.empty() || to_id.empty()) return false;
    if (from_kind == WorldForgeMapEndpointKind::Region && !find_region(session.map, from_id)) return false;
    if (from_kind == WorldForgeMapEndpointKind::Poi && !find_poi(session.map, from_id)) return false;
    if (to_kind == WorldForgeMapEndpointKind::Region && !find_region(session.map, to_id)) return false;
    if (to_kind == WorldForgeMapEndpointKind::Poi && !find_poi(session.map, to_id)) return false;
    WorldForgeMapLink link;
    link.id = id;
    link.kind = kind;
    link.from_kind = from_kind;
    link.from_id = std::move(from_id);
    link.to_kind = to_kind;
    link.to_id = std::move(to_id);
    link.canon_status = WorldForgeMapCanonStatus::Draft;
    session.map.links.push_back(std::move(link));
    session.selected_id = id;
    session.list_kind = WorldForgeEditorSession::ListKind::Links;
    session.dirty = true;
    session.status = "Added map link " + id;
    return true;
}

std::string unique_dialogue_tree_id(const WorldForgeDialoguesAsset& asset, const std::string& preferred) {
    return unique_slugify_id(
        preferred, [&](const std::string& candidate) { return find_dialogue_tree(asset, candidate) != nullptr; },
        "dialogue");
}

bool add_dialogue_tree(WorldForgeEditorSession& session, std::string id, std::string display_name,
    std::string parent_quest_id = {}) {
    id = sanitize_id_token(std::move(id));
    if (!is_valid_id_token(id) || find_dialogue_tree(session.dialogues, id)) return false;
    WorldForgeDialogueTree tree;
    tree.id = id;
    tree.display_name = std::move(display_name);
    tree.parent_quest_id = std::move(parent_quest_id);
    tree.canon_status = WorldForgeDialogueCanonStatus::Draft;
    tree.entry_node_id = "start";
    WorldForgeDialogueNode entry;
    entry.id = "start";
    entry.speaker_id = "narrator";
    entry.line = "";
    tree.nodes.push_back(std::move(entry));
    session.dialogues.trees.push_back(std::move(tree));
    session.selected_id = id;
    session.dialogue_selected_node_id = "start";
    session.dialogue_graph_full_relayout = true;
    session.dialogue_graph_fit_requested = true;
    session.dialogue_undo_stack.clear();
    session.dialogue_redo_stack.clear();
    session.dirty = true;
    session.status = "Added dialogue tree " + id;
    return true;
}

std::string unique_relationship_id(const WorldForgeRelationshipsAsset& asset, const std::string& preferred) {
    return unique_slugify_id(
        preferred, [&](const std::string& candidate) { return find_node(asset, candidate) || find_edge(asset, candidate); },
        "item");
}

bool add_relationship_node(WorldForgeEditorSession& session, std::string id, WorldForgeRelationshipNodeKind kind,
    std::string display_name) {
    id = sanitize_id_token(std::move(id));
    if (!is_valid_id_token(id) || find_node(session.relationships, id) || find_edge(session.relationships, id))
        return false;
    WorldForgeRelationshipNode node;
    node.id = id;
    node.kind = kind;
    node.display_name = std::move(display_name);
    node.canon_status = WorldForgeRelationshipCanonStatus::Draft;
    session.relationships.nodes.push_back(std::move(node));
    session.selected_id = id;
    session.dirty = true;
    session.graph_needs_layout = true;
    session.status = "Added node " + id;
    return true;
}

bool remove_relationship_node(WorldForgeEditorSession& session, const std::string& id) {
    auto& nodes = session.relationships.nodes;
    const auto it = std::find_if(nodes.begin(), nodes.end(), [&](const auto& n) { return n.id == id; });
    if (it == nodes.end()) return false;
    nodes.erase(it);
    auto& edges = session.relationships.edges;
    edges.erase(std::remove_if(edges.begin(), edges.end(),
                     [&](const WorldForgeRelationshipEdge& edge) {
                         return (edge.from.target == WorldForgeRelationshipEndpointTarget::Node &&
                                    edge.from.id == id) ||
                                (edge.to.target == WorldForgeRelationshipEndpointTarget::Node && edge.to.id == id);
                     }),
        edges.end());
    session.graph_positions.erase(id);
    if (session.selected_id == id || session.graph_link_from == id) {
        session.selected_id.clear();
        session.graph_link_from.clear();
    }
    session.dirty = true;
    session.graph_needs_layout = true;
    session.status = "Removed node " + id;
    return true;
}

bool add_relationship_edge(WorldForgeEditorSession& session, std::string id, WorldForgeRelationshipEndpoint from,
    WorldForgeRelationshipEndpoint to, WorldForgeRelationshipEdgeKind kind) {
    id = sanitize_id_token(std::move(id));
    if (!is_valid_id_token(id) || find_node(session.relationships, id) || find_edge(session.relationships, id))
        return false;
    if (from.id.empty() || to.id.empty()) return false;
    if (from.target == WorldForgeRelationshipEndpointTarget::Node && !find_node(session.relationships, from.id))
        return false;
    if (to.target == WorldForgeRelationshipEndpointTarget::Node && !find_node(session.relationships, to.id))
        return false;
    if (from.target == WorldForgeRelationshipEndpointTarget::Faction && !find_faction(session.factions, from.id))
        return false;
    if (to.target == WorldForgeRelationshipEndpointTarget::Faction && !find_faction(session.factions, to.id))
        return false;
    if (from.target == to.target && from.id == to.id) return false;

    WorldForgeRelationshipEdge edge;
    edge.id = id;
    edge.from = std::move(from);
    edge.to = std::move(to);
    edge.kind = kind;
    edge.canon_status = WorldForgeRelationshipCanonStatus::Draft;
    session.relationships.edges.push_back(std::move(edge));
    session.selected_id = id;
    session.dirty = true;
    session.graph_needs_layout = true;
    session.graph_link_from.clear();
    session.status = "Added edge " + id;
    return true;
}

bool remove_relationship_edge(WorldForgeEditorSession& session, const std::string& id) {
    auto& edges = session.relationships.edges;
    const auto it = std::find_if(edges.begin(), edges.end(), [&](const auto& e) { return e.id == id; });
    if (it == edges.end()) return false;
    edges.erase(it);
    if (session.selected_id == id) session.selected_id.clear();
    session.dirty = true;
    session.status = "Removed edge " + id;
    return true;
}

void parse_graph_endpoint(const std::string& key, WorldForgeRelationshipEndpoint& out) {
    if (key.rfind("faction:", 0) == 0) {
        out.target = WorldForgeRelationshipEndpointTarget::Faction;
        out.id = key.substr(8);
    } else {
        out.target = WorldForgeRelationshipEndpointTarget::Node;
        out.id = key;
    }
}

float graph_node_radius(float zoom, const ImVec2& canvas_size) {
    const float soft = 20.0f * std::sqrt((std::max)(zoom, 0.25f));
    const float cap = (std::min)(canvas_size.x, canvas_size.y) * 0.065f;
    return (std::clamp)(soft, 10.0f, (std::max)(12.0f, cap));
}

void fit_graph_camera(WorldForgeEditorSession& session, const ImVec2& canvas_size,
    const std::unordered_set<std::string>& visible_keys) {
    std::vector<std::string> keys;
    if (!visible_keys.empty()) keys.assign(visible_keys.begin(), visible_keys.end());
    const auto bounds = keys.empty() ? compute_graph_bounds(session.graph_positions)
                                     : compute_graph_bounds(session.graph_positions, &keys);
    WorldForgeGraphCamera cam;
    cam.zoom = session.graph_zoom;
    cam.pan = session.graph_pan;
    cam.min_zoom = 0.4f;
    cam.max_zoom = 1.75f;
    fit_graph_camera_to_bounds(cam, canvas_size.x, canvas_size.y, bounds);
    session.graph_zoom = cam.zoom;
    session.graph_pan = cam.pan;
}

void clamp_graph_camera(WorldForgeEditorSession& session, const ImVec2& canvas_size,
    const std::unordered_set<std::string>& visible_keys, float node_radius) {
    bool any = false;
    float min_x = 0.0f, max_x = 0.0f, min_y = 0.0f, max_y = 0.0f;
    for (const auto& entry : session.graph_positions) {
        if (!visible_keys.empty() && !visible_keys.count(entry.first)) continue;
        if (!any) {
            min_x = max_x = entry.second[0];
            min_y = max_y = entry.second[1];
            any = true;
        } else {
            min_x = (std::min)(min_x, entry.second[0]);
            max_x = (std::max)(max_x, entry.second[0]);
            min_y = (std::min)(min_y, entry.second[1]);
            max_y = (std::max)(max_y, entry.second[1]);
        }
    }
    if (!any) return;

    const float pad = node_radius + 8.0f;
    const float z = (std::max)(session.graph_zoom, 0.01f);
    const float content_w = (max_x - min_x) * z;
    const float content_h = (max_y - min_y) * z;
    float pan_min_x = pad - max_x * z;
    float pan_max_x = canvas_size.x - pad - min_x * z;
    float pan_min_y = pad - max_y * z;
    float pan_max_y = canvas_size.y - pad - min_y * z;
    if (content_w > canvas_size.x - 2.0f * pad) {
        pan_min_x = canvas_size.x - pad - max_x * z;
        pan_max_x = pad - min_x * z;
    }
    if (content_h > canvas_size.y - 2.0f * pad) {
        pan_min_y = canvas_size.y - pad - max_y * z;
        pan_max_y = pad - min_y * z;
    }
    if (pan_min_x > pan_max_x) session.graph_pan[0] = (pan_min_x + pan_max_x) * 0.5f;
    else session.graph_pan[0] = (std::clamp)(session.graph_pan[0], pan_min_x, pan_max_x);
    if (pan_min_y > pan_max_y) session.graph_pan[1] = (pan_min_y + pan_max_y) * 0.5f;
    else session.graph_pan[1] = (std::clamp)(session.graph_pan[1], pan_min_y, pan_max_y);
}

void clamp_node_world_to_canvas(WorldForgeEditorSession& session, const std::string& key, const ImVec2& canvas_size,
    float node_radius) {
    auto it = session.graph_positions.find(key);
    if (it == session.graph_positions.end()) return;
    const float z = (std::max)(session.graph_zoom, 0.01f);
    const float pad = node_radius + 4.0f;
    const float min_world_x = (pad - session.graph_pan[0]) / z;
    const float max_world_x = (canvas_size.x - pad - session.graph_pan[0]) / z;
    const float min_world_y = (pad - session.graph_pan[1]) / z;
    const float max_world_y = (canvas_size.y - pad - session.graph_pan[1]) / z;
    it->second[0] = (std::clamp)(it->second[0], min_world_x, max_world_x);
    it->second[1] = (std::clamp)(it->second[1], min_world_y, max_world_y);
}

void draw_add_quest_controls(WorldForgeEditorSession& session) {
    if (ImGui::TreeNode("Add quest##WorldForgeAddQuest")) {
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputTextWithHint("##CreateQuestName", "display name (required)", session.create_quest_name.data(),
            session.create_quest_name.size());
        const auto name = trim(session.create_quest_name.data());
        const auto preview_id = name.empty() ? std::string{} : unique_quest_id(session.quests, name);
        if (!preview_id.empty()) ImGui::TextDisabled("id: %s", preview_id.c_str());
        else ImGui::TextDisabled("id: (from display name)");
        if (ImGui::Button("Create quest##WorldForgeCreateQuest")) {
            if (name.empty()) {
                session.status = "Enter a display name for the quest";
            } else if (add_quest(session, preview_id, name)) {
                session.create_quest_id.fill('\0');
                session.create_quest_name.fill('\0');
            } else {
                session.status = "Could not create quest (invalid or duplicate id)";
            }
        }
        ImGui::TreePop();
    }
}

void draw_add_faction_controls(WorldForgeEditorSession& session) {
    if (ImGui::TreeNode("Add faction##WorldForgeAddFaction")) {
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputTextWithHint("##CreateFactionName", "display name (required)", session.create_faction_name.data(),
            session.create_faction_name.size());
        draw_enum_combo("kind##CreateFactionKind", session.create_faction_kind,
            {WorldForgeFactionKind::Faction, WorldForgeFactionKind::Culture, WorldForgeFactionKind::Clan,
                WorldForgeFactionKind::Warband});
        const auto name = trim(session.create_faction_name.data());
        const auto preview_id = name.empty() ? std::string{} : unique_faction_id(session.factions, name);
        if (!preview_id.empty()) ImGui::TextDisabled("id: %s", preview_id.c_str());
        else ImGui::TextDisabled("id: (from display name)");
        if (ImGui::Button("Create faction##WorldForgeCreateFaction")) {
            if (name.empty()) {
                session.status = "Enter a display name for the faction";
            } else if (add_faction(session, preview_id, name, session.create_faction_kind)) {
                session.create_faction_name.fill('\0');
            } else {
                session.status = "Could not create faction (invalid or duplicate id)";
            }
        }
        ImGui::TreePop();
    }
}

void draw_add_region_controls(WorldForgeEditorSession& session) {
    if (ImGui::TreeNode("Add region##WorldForgeAddRegion")) {
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputTextWithHint("##CreateRegionName", "display name (required)", session.create_region_name.data(),
            session.create_region_name.size());
        draw_enum_combo("kind##CreateRegionKind", session.create_region_kind,
            {WorldForgeRegionKind::Region, WorldForgeRegionKind::Fortress, WorldForgeRegionKind::City,
                WorldForgeRegionKind::Wilderness, WorldForgeRegionKind::Chaotic, WorldForgeRegionKind::Settlement,
                WorldForgeRegionKind::Other});
        const auto name = trim(session.create_region_name.data());
        const auto preview_id = name.empty() ? std::string{} : unique_region_id(session.map, name);
        if (!preview_id.empty()) ImGui::TextDisabled("id: %s", preview_id.c_str());
        else ImGui::TextDisabled("id: (from display name)");
        if (ImGui::Button("Create region##WorldForgeCreateRegion")) {
            if (name.empty()) {
                session.status = "Enter a display name for the region";
            } else if (add_region(session, preview_id, name, session.create_region_kind)) {
                session.create_region_name.fill('\0');
            } else {
                session.status = "Could not create region (invalid or duplicate id)";
            }
        }
        ImGui::TreePop();
    }
}

void draw_add_poi_controls(WorldForgeEditorSession& session) {
    if (ImGui::TreeNode("Add POI##WorldForgeAddPoi")) {
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputTextWithHint("##CreatePoiName", "display name (required)", session.create_poi_name.data(),
            session.create_poi_name.size());
        draw_enum_combo("kind##CreatePoiKind", session.create_poi_kind,
            {WorldForgePoiKind::Landmark, WorldForgePoiKind::Settlement, WorldForgePoiKind::Gate,
                WorldForgePoiKind::Shrine, WorldForgePoiKind::Camp, WorldForgePoiKind::Other});
        {
            std::string region_id = trim(session.create_poi_region_id.data());
            if (draw_id_combo("regionId##CreatePoiRegion", region_id, collect_region_ids(session.map), false,
                    "(select region)")) {
                std::snprintf(session.create_poi_region_id.data(), session.create_poi_region_id.size(), "%s",
                    region_id.c_str());
            }
        }
        const auto name = trim(session.create_poi_name.data());
        const auto region_id = trim(session.create_poi_region_id.data());
        const auto preview_id = name.empty() ? std::string{} : unique_poi_id(session.map, name);
        if (!preview_id.empty()) ImGui::TextDisabled("id: %s", preview_id.c_str());
        else ImGui::TextDisabled("id: (from display name)");
        if (ImGui::Button("Create POI##WorldForgeCreatePoi")) {
            if (name.empty()) {
                session.status = "Enter a display name for the POI";
            } else if (region_id.empty()) {
                session.status = "Select a region for the POI";
            } else if (add_poi(session, preview_id, name, session.create_poi_kind, region_id)) {
                session.create_poi_name.fill('\0');
            } else {
                session.status = "Could not create POI (invalid id, duplicate, or unknown region)";
            }
        }
        ImGui::TreePop();
    }
}

std::vector<std::string> collect_ids_for_endpoint(const WorldForgeMapAsset& asset, WorldForgeMapEndpointKind kind) {
    if (kind == WorldForgeMapEndpointKind::Region) return collect_region_ids(asset);
    std::vector<std::string> ids;
    ids.reserve(asset.pois.size());
    for (const auto& poi : asset.pois) ids.push_back(poi.id);
    std::sort(ids.begin(), ids.end());
    return ids;
}

void draw_add_map_link_controls(WorldForgeEditorSession& session) {
    if (ImGui::TreeNode("Add link##WorldForgeAddLink")) {
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputTextWithHint("##CreateLinkId", "link id (optional)", session.create_link_id.data(),
            session.create_link_id.size());
        draw_enum_combo("kind##CreateLinkKind", session.create_link_kind,
            {WorldForgeMapLinkKind::Travel, WorldForgeMapLinkKind::SoftGate, WorldForgeMapLinkKind::StoryGate,
                WorldForgeMapLinkKind::Adjacency});
        draw_enum_combo("fromKind##CreateLinkFromKind", session.create_link_from_kind,
            {WorldForgeMapEndpointKind::Region, WorldForgeMapEndpointKind::Poi});
        {
            std::string from_id = trim(session.create_link_from_id.data());
            if (draw_id_combo("fromId##CreateLinkFrom", from_id,
                    collect_ids_for_endpoint(session.map, session.create_link_from_kind), false, "(select)")) {
                std::snprintf(session.create_link_from_id.data(), session.create_link_from_id.size(), "%s",
                    from_id.c_str());
            }
        }
        draw_enum_combo("toKind##CreateLinkToKind", session.create_link_to_kind,
            {WorldForgeMapEndpointKind::Region, WorldForgeMapEndpointKind::Poi});
        {
            std::string to_id = trim(session.create_link_to_id.data());
            if (draw_id_combo("toId##CreateLinkTo", to_id,
                    collect_ids_for_endpoint(session.map, session.create_link_to_kind), false, "(select)")) {
                std::snprintf(session.create_link_to_id.data(), session.create_link_to_id.size(), "%s", to_id.c_str());
            }
        }
        if (ImGui::Button("Create link##WorldForgeCreateLink")) {
            const auto from_id = trim(session.create_link_from_id.data());
            const auto to_id = trim(session.create_link_to_id.data());
            std::string id = trim(session.create_link_id.data());
            if (id.empty()) {
                id = unique_map_link_id(session.map, from_id + "_" + to_string(session.create_link_kind) + "_" + to_id);
            } else {
                id = unique_map_link_id(session.map, id);
            }
            if (from_id.empty() || to_id.empty()) {
                session.status = "Select from and to endpoints for the link";
            } else if (add_map_link(session, id, session.create_link_kind, session.create_link_from_kind, from_id,
                           session.create_link_to_kind, to_id)) {
                session.create_link_id.fill('\0');
                session.create_link_from_id.fill('\0');
                session.create_link_to_id.fill('\0');
            } else {
                session.status = "Could not create link (invalid endpoints or duplicate id)";
            }
        }
        ImGui::TreePop();
    }
}

void draw_add_dialogue_tree_controls(WorldForgeEditorSession& session) {
    if (ImGui::TreeNode("Add dialogue tree##WorldForgeAddDialogueTree")) {
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputTextWithHint("##CreateDlgTreeName", "display name (required)", session.create_dialogue_tree_name.data(),
            session.create_dialogue_tree_name.size());
        {
            std::string parent = trim(session.create_dialogue_tree_parent_quest.data());
            if (draw_id_combo("parentQuestId##CreateDlgTreeParent", parent, collect_quest_ids(session.quests), true,
                    "(optional)")) {
                std::snprintf(session.create_dialogue_tree_parent_quest.data(),
                    session.create_dialogue_tree_parent_quest.size(), "%s", parent.c_str());
            }
        }
        const auto name = trim(session.create_dialogue_tree_name.data());
        const auto preview_id = name.empty() ? std::string{} : unique_dialogue_tree_id(session.dialogues, name);
        if (!preview_id.empty()) ImGui::TextDisabled("id: %s", preview_id.c_str());
        else ImGui::TextDisabled("id: (from display name)");
        ImGui::TextDisabled("Creates tree with entry node \"start\"");
        if (ImGui::Button("Create dialogue tree##WorldForgeCreateDlgTree")) {
            if (name.empty()) {
                session.status = "Enter a display name for the dialogue tree";
            } else if (add_dialogue_tree(session, preview_id, name,
                           trim(session.create_dialogue_tree_parent_quest.data()))) {
                session.create_dialogue_tree_name.fill('\0');
                session.create_dialogue_tree_parent_quest.fill('\0');
            } else {
                session.status = "Could not create dialogue tree (invalid or duplicate id)";
            }
        }
        ImGui::TreePop();
    }
}

void draw_add_node_controls(WorldForgeEditorSession& session) {
    if (ImGui::TreeNode("Add node##WorldForgeAddNode")) {
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputTextWithHint("##CreateNodeName", "display name (required)", session.create_node_name.data(),
            session.create_node_name.size());
        draw_enum_combo("kind##CreateNodeKind", session.create_node_kind,
            {WorldForgeRelationshipNodeKind::Person, WorldForgeRelationshipNodeKind::Deity,
                WorldForgeRelationshipNodeKind::Artifact, WorldForgeRelationshipNodeKind::Organization});
        const auto name = trim(session.create_node_name.data());
        const auto preview_id = name.empty() ? std::string{} : unique_relationship_id(session.relationships, name);
        if (!preview_id.empty()) ImGui::TextDisabled("id: %s", preview_id.c_str());
        else ImGui::TextDisabled("id: (from display name)");
        if (ImGui::Button("Create node##WorldForgeCreateNode")) {
            if (name.empty()) {
                session.status = "Enter a display name for the node";
            } else if (add_relationship_node(session, preview_id, session.create_node_kind, name)) {
                session.create_node_id.fill('\0');
                session.create_node_name.fill('\0');
            } else {
                session.status = "Could not create node (invalid or duplicate id)";
            }
        }
        ImGui::TreePop();
    }
}

void draw_add_edge_controls(WorldForgeEditorSession& session) {
    if (ImGui::TreeNode("Add relationship##WorldForgeAddEdge")) {
        ImGui::SetNextItemWidth(180.0f);
        ImGui::InputTextWithHint("##CreateEdgeId", "edge id (optional)", session.create_edge_id.data(),
            session.create_edge_id.size());
        draw_enum_combo("from.target##CreateEdgeFromT", session.create_edge_from_target,
            {WorldForgeRelationshipEndpointTarget::Node, WorldForgeRelationshipEndpointTarget::Faction});
        {
            std::string from_id = trim(session.create_edge_from.data());
            if (draw_id_combo("from.id##CreateEdgeFrom", from_id,
                    collect_endpoint_ids(session, session.create_edge_from_target), true, "(select)")) {
                std::snprintf(session.create_edge_from.data(), session.create_edge_from.size(), "%s", from_id.c_str());
            }
        }
        draw_enum_combo("to.target##CreateEdgeToT", session.create_edge_to_target,
            {WorldForgeRelationshipEndpointTarget::Node, WorldForgeRelationshipEndpointTarget::Faction});
        {
            std::string to_id = trim(session.create_edge_to.data());
            if (draw_id_combo("to.id##CreateEdgeTo", to_id,
                    collect_endpoint_ids(session, session.create_edge_to_target), true, "(select)")) {
                std::snprintf(session.create_edge_to.data(), session.create_edge_to.size(), "%s", to_id.c_str());
            }
        }
        draw_enum_combo("kind##CreateEdgeKind", session.create_edge_kind,
            {WorldForgeRelationshipEdgeKind::Ally, WorldForgeRelationshipEdgeKind::Rival,
                WorldForgeRelationshipEdgeKind::MemberOf, WorldForgeRelationshipEdgeKind::Leads,
                WorldForgeRelationshipEdgeKind::Kin, WorldForgeRelationshipEdgeKind::Serves,
                WorldForgeRelationshipEdgeKind::Opposes, WorldForgeRelationshipEdgeKind::Influences,
                WorldForgeRelationshipEdgeKind::Related});
        if (ImGui::Button("Create relationship##WorldForgeCreateEdge")) {
            WorldForgeRelationshipEndpoint from{session.create_edge_from_target, trim(session.create_edge_from.data())};
            WorldForgeRelationshipEndpoint to{session.create_edge_to_target, trim(session.create_edge_to.data())};
            std::string id = trim(session.create_edge_id.data());
            if (id.empty())
                id = unique_relationship_id(session.relationships,
                    from.id + "_" + to_string(session.create_edge_kind) + "_" + to.id);
            else
                id = unique_relationship_id(session.relationships, id);
            if (add_relationship_edge(session, id, from, to, session.create_edge_kind)) {
                session.create_edge_id.fill('\0');
                session.create_edge_from.fill('\0');
                session.create_edge_to.fill('\0');
            } else {
                session.status = "Could not create edge (check endpoints exist / unique id)";
            }
        }
        ImGui::TreePop();
    }
}

// --- list + detail panes -----------------------------------------------------

constexpr float kListFraction = 0.35f;

void begin_list_detail(const ImVec2& avail, float& list_w) {
    list_w = (std::clamp)(avail.x * kListFraction, 180.0f, 420.0f);
}

/// Kind-colored silhouette card used until real portraits/POI images exist (GPU textures later).
enum class ConceptSilhouette : std::uint8_t { Person, Deity, Artifact, Org, Banner, Place, Landmark };

void draw_concept_placeholder(const WorldForgeEditorSession& session, const char* tex_key, const char* kind_label,
    const char* title, const char* subtitle, ConceptSilhouette silhouette, ImU32 fill, ImU32 accent) {
    const float width = (std::min)(ImGui::GetContentRegionAvail().x, 220.0f);
    const float height = 140.0f;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 max{origin.x + width, origin.y + height};
    auto* draw = ImGui::GetWindowDrawList();

    std::uint64_t tex_bits = 0;
    if (tex_key) {
        if (const auto found = session.concept_placeholder_tex.find(tex_key);
            found != session.concept_placeholder_tex.end())
            tex_bits = found->second;
    }
    if (tex_bits != 0) {
        ImGui::Dummy(ImVec2(width, height));
        draw->AddImageRounded(static_cast<ImTextureID>(tex_bits), origin, max, ImVec2(0, 0), ImVec2(1, 1),
            IM_COL32_WHITE, 8.0f);
        draw->AddRect(origin, max, accent, 8.0f, 0, 2.0f);
        draw->AddRectFilled(ImVec2(origin.x, max.y - 40.0f), max, IM_COL32(12, 14, 18, 180), 8.0f,
            ImDrawFlags_RoundCornersBottom);
        draw->AddText(ImVec2(origin.x + 10.0f, origin.y + 8.0f), IM_COL32(230, 230, 235, 255), kind_label);
        if (title && title[0]) {
            const ImVec2 title_size = ImGui::CalcTextSize(title);
            draw->AddText(ImVec2(origin.x + (width - title_size.x) * 0.5f, max.y - 36.0f),
                IM_COL32(245, 245, 245, 255), title);
        }
        if (subtitle && subtitle[0])
            draw->AddText(ImVec2(origin.x + 10.0f, max.y - 18.0f), IM_COL32(180, 185, 195, 220), subtitle);
        ImGui::SetCursorScreenPos(ImVec2(origin.x, max.y + 4.0f));
        ImGui::Dummy(ImVec2(width, 0.0f));
        ImGui::TextDisabled("Concept art placeholder");
        return;
    }

    draw->AddRectFilled(origin, max, fill, 8.0f);
    draw->AddRect(origin, max, accent, 8.0f, 0, 2.0f);

    const ImVec2 center{origin.x + width * 0.5f, origin.y + height * 0.42f};
    switch (silhouette) {
    case ConceptSilhouette::Person:
        draw->AddCircleFilled(ImVec2(center.x, center.y - 18.0f), 16.0f, accent);
        draw->AddRectFilled(ImVec2(center.x - 22.0f, center.y + 2.0f), ImVec2(center.x + 22.0f, center.y + 38.0f),
            accent, 10.0f);
        break;
    case ConceptSilhouette::Deity:
        for (int i = 0; i < 6; ++i) {
            const float a = 6.2831853f * (static_cast<float>(i) / 6.0f) - 1.5708f;
            draw->AddLine(center, ImVec2(center.x + std::cos(a) * 34.0f, center.y + std::sin(a) * 34.0f), accent, 3.0f);
        }
        draw->AddCircleFilled(center, 14.0f, accent);
        break;
    case ConceptSilhouette::Artifact: {
        const ImVec2 diamond[4] = {
            {center.x, center.y - 28.0f},
            {center.x + 24.0f, center.y},
            {center.x, center.y + 28.0f},
            {center.x - 24.0f, center.y},
        };
        draw->AddConvexPolyFilled(diamond, 4, accent);
        break;
    }
    case ConceptSilhouette::Org:
        draw->AddRectFilled(ImVec2(center.x - 26.0f, center.y - 8.0f), ImVec2(center.x + 26.0f, center.y + 28.0f),
            accent, 2.0f);
        draw->AddTriangleFilled(ImVec2(center.x - 30.0f, center.y - 8.0f), ImVec2(center.x + 30.0f, center.y - 8.0f),
            ImVec2(center.x, center.y - 30.0f), accent);
        break;
    case ConceptSilhouette::Banner:
        draw->AddRectFilled(ImVec2(center.x - 6.0f, center.y - 30.0f), ImVec2(center.x + 6.0f, center.y + 30.0f),
            accent);
        draw->AddTriangleFilled(ImVec2(center.x + 6.0f, center.y - 28.0f), ImVec2(center.x + 34.0f, center.y - 12.0f),
            ImVec2(center.x + 6.0f, center.y + 4.0f), accent);
        break;
    case ConceptSilhouette::Place:
        draw->AddCircle(center, 28.0f, accent, 0, 2.0f);
        draw->AddCircleFilled(ImVec2(center.x, center.y - 4.0f), 6.0f, accent);
        break;
    case ConceptSilhouette::Landmark:
        draw->AddTriangleFilled(ImVec2(center.x, center.y - 30.0f), ImVec2(center.x + 26.0f, center.y + 24.0f),
            ImVec2(center.x - 26.0f, center.y + 24.0f), accent);
        break;
    }

    draw->AddText(ImVec2(origin.x + 10.0f, origin.y + 8.0f), IM_COL32(230, 230, 235, 255), kind_label);
    if (title && title[0]) {
        const ImVec2 title_size = ImGui::CalcTextSize(title);
        draw->AddText(ImVec2(origin.x + (width - title_size.x) * 0.5f, max.y - 36.0f), IM_COL32(245, 245, 245, 255),
            title);
    }
    if (subtitle && subtitle[0]) {
        draw->AddText(ImVec2(origin.x + 10.0f, max.y - 18.0f), IM_COL32(180, 185, 195, 220), subtitle);
    }

    ImGui::Dummy(ImVec2(width, height + 4.0f));
    ImGui::TextDisabled("Concept silhouette (image missing)");
}

void draw_relationship_node_placeholder(const WorldForgeEditorSession& session, const WorldForgeRelationshipNode& node) {
    const char* kind = to_string(node.kind);
    ImU32 fill = IM_COL32(40, 48, 44, 255);
    ImU32 accent = IM_COL32(90, 160, 120, 255);
    ConceptSilhouette silhouette = ConceptSilhouette::Person;
    const char* tex_key = "person";
    switch (node.kind) {
    case WorldForgeRelationshipNodeKind::Person:
        fill = IM_COL32(42, 48, 58, 255);
        accent = IM_COL32(120, 160, 210, 255);
        silhouette = ConceptSilhouette::Person;
        tex_key = "person";
        break;
    case WorldForgeRelationshipNodeKind::Deity:
        fill = IM_COL32(52, 42, 58, 255);
        accent = IM_COL32(210, 170, 90, 255);
        silhouette = ConceptSilhouette::Deity;
        tex_key = "deity";
        break;
    case WorldForgeRelationshipNodeKind::Artifact:
        fill = IM_COL32(48, 40, 40, 255);
        accent = IM_COL32(200, 110, 110, 255);
        silhouette = ConceptSilhouette::Artifact;
        tex_key = "artifact";
        break;
    case WorldForgeRelationshipNodeKind::Organization:
        fill = IM_COL32(40, 44, 52, 255);
        accent = IM_COL32(140, 150, 190, 255);
        silhouette = ConceptSilhouette::Org;
        tex_key = "organization";
        break;
    }
    const std::string title = node.display_name.empty() ? node.id : node.display_name;
    const std::string subtitle = std::string(kind) + " · " + node.id;
    draw_concept_placeholder(session, tex_key, kind, title.c_str(), subtitle.c_str(), silhouette, fill, accent);
}

void draw_faction_placeholder(const WorldForgeEditorSession& session, const WorldForgeFactionEntity& entity) {
    const char* kind = to_string(entity.kind);
    const std::string title = entity.display_name.empty() ? entity.id : entity.display_name;
    const std::string subtitle = std::string(kind) + " · " + entity.id;
    draw_concept_placeholder(session, "faction", kind, title.c_str(), subtitle.c_str(), ConceptSilhouette::Banner,
        IM_COL32(40, 46, 58, 255), IM_COL32(100, 130, 180, 255));
}

bool draw_faction_standing_section(WorldForgeEditorSession& session, WorldForgeFactionEntity& entity) {
    bool dirty = false;
    ImGui::Separator();
    ImGui::TextUnformatted("standing (DEC-0029)");
    bool tracks = entity.standing && entity.standing->tracks_player;
    if (ImGui::Checkbox("tracksPlayer##FactionStanding", &tracks)) {
        if (tracks) {
            if (!entity.standing) entity.standing = WorldForgeFactionStandingConfig{};
            entity.standing->tracks_player = true;
        } else if (entity.standing) {
            entity.standing->tracks_player = false;
            // Keep authored ranks/lock-in when toggled off so authors can re-enable without data loss.
        } else {
            entity.standing = WorldForgeFactionStandingConfig{};
            entity.standing->tracks_player = false;
        }
        dirty = true;
    }
    if (!entity.standing) return dirty;
    auto& standing = *entity.standing;
    if (draw_double_field("min##FactionStanding", standing.min_score)) dirty = true;
    if (draw_double_field("max##FactionStanding", standing.max_score)) dirty = true;
    ImGui::Text("ranks (%zu)", standing.ranks.size());
    if (ImGui::Button("Add rank##FactionStanding")) {
        WorldForgeFactionStandingRank rank;
        rank.id = "rank_" + std::to_string(standing.ranks.size() + 1);
        rank.min_score = standing.ranks.empty() ? standing.min_score : standing.ranks.back().min_score;
        rank.display_name = rank.id;
        standing.ranks.push_back(std::move(rank));
        dirty = true;
    }
    for (std::size_t i = 0; i < standing.ranks.size(); ++i) {
        auto& rank = standing.ranks[i];
        ImGui::PushID(static_cast<int>(i));
        if (draw_input_text("id##rank", rank.id)) dirty = true;
        if (draw_double_field("minScore##rank", rank.min_score)) dirty = true;
        if (draw_input_text("displayName##rank", rank.display_name)) dirty = true;
        if (ImGui::Button("Remove rank##FactionStanding")) {
            standing.ranks.erase(standing.ranks.begin() + static_cast<std::ptrdiff_t>(i));
            dirty = true;
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    bool has_lock = standing.lock_in.has_value();
    if (ImGui::Checkbox("lockIn enabled##FactionStanding", &has_lock)) {
        if (has_lock && !standing.lock_in) standing.lock_in = WorldForgeFactionStandingLockIn{};
        if (!has_lock) standing.lock_in.reset();
        dirty = true;
    }
    if (standing.lock_in) {
        if (draw_double_field("threshold##FactionStanding", standing.lock_in->threshold)) dirty = true;
        if (draw_csv_field("exclusiveFactionIds##FactionStanding", standing.lock_in->exclusive_faction_ids)) dirty = true;
    }
    if (dirty) session.dirty = true;
    return dirty;
}

bool draw_edge_standing_transfer(WorldForgeEditorSession& session, WorldForgeRelationshipEdge& edge) {
    const bool hostility = edge.kind == WorldForgeRelationshipEdgeKind::Rival ||
        edge.kind == WorldForgeRelationshipEdgeKind::Opposes;
    const bool faction_edge = edge.from.target == WorldForgeRelationshipEndpointTarget::Faction &&
        edge.to.target == WorldForgeRelationshipEndpointTarget::Faction;
    if (!hostility || !faction_edge) {
        ImGui::TextDisabled("standingTransfer applies to rival/opposes faction↔faction edges");
    }
    if (draw_double_field("standingTransfer", edge.standing_transfer)) {
        session.dirty = true;
        return true;
    }
    return false;
}

bool draw_quest_standing_section(WorldForgeEditorSession& session, WorldForgeQuest& quest) {
    bool dirty = false;
    ImGui::Separator();
    ImGui::TextUnformatted("standing requirements / rewards (DEC-0029)");
    ImGui::TextWrapped("Rewards are applied by Lua/MCP standing_adjust (or a later QuestRuntime hook); "
        "QuestRuntime complete does not auto-apply rewards in v1.");
    const auto faction_ids = collect_faction_ids(session.factions);
    ImGui::Text("standingRequirements (%zu)", quest.standing_requirements.size());
    if (ImGui::Button("Add requirement##QuestStandingReq")) {
        WorldForgeQuestStandingRequirement req;
        if (!faction_ids.empty()) req.faction_id = faction_ids.front();
        req.min_score = 0.0;
        quest.standing_requirements.push_back(std::move(req));
        dirty = true;
    }
    for (std::size_t i = 0; i < quest.standing_requirements.size(); ++i) {
        auto& req = quest.standing_requirements[i];
        ImGui::PushID(static_cast<int>(2000 + i));
        if (draw_id_combo("factionId##req", req.faction_id, faction_ids, false)) dirty = true;
        bool has_min = req.min_score.has_value();
        if (ImGui::Checkbox("minScore##reqEnable", &has_min)) {
            if (has_min) req.min_score = 0.0;
            else req.min_score.reset();
            dirty = true;
        }
        if (req.min_score) {
            if (draw_double_field("minScore##req", *req.min_score)) dirty = true;
        }
        if (draw_input_text("minRankId##req", req.min_rank_id)) dirty = true;
        if (ImGui::Button("Remove requirement##QuestStandingReq")) {
            quest.standing_requirements.erase(quest.standing_requirements.begin() + static_cast<std::ptrdiff_t>(i));
            dirty = true;
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    ImGui::Text("standingRewards (%zu)", quest.standing_rewards.size());
    if (ImGui::Button("Add reward##QuestStandingReward")) {
        WorldForgeQuestStandingReward reward;
        if (!faction_ids.empty()) reward.faction_id = faction_ids.front();
        reward.delta = 10.0;
        quest.standing_rewards.push_back(std::move(reward));
        dirty = true;
    }
    for (std::size_t i = 0; i < quest.standing_rewards.size(); ++i) {
        auto& reward = quest.standing_rewards[i];
        ImGui::PushID(static_cast<int>(3000 + i));
        if (draw_id_combo("factionId##reward", reward.faction_id, faction_ids, false)) dirty = true;
        if (draw_double_field("delta##reward", reward.delta)) dirty = true;
        if (ImGui::Button("Remove reward##QuestStandingReward")) {
            quest.standing_rewards.erase(quest.standing_rewards.begin() + static_cast<std::ptrdiff_t>(i));
            dirty = true;
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    if (dirty) session.dirty = true;
    return dirty;
}

void draw_region_placeholder(const WorldForgeEditorSession& session, const WorldForgeRegion& region) {
    const char* kind = to_string(region.kind);
    const std::string title = region.display_name.empty() ? region.id : region.display_name;
    const std::string subtitle = std::string(kind) + " · " + region.id;
    draw_concept_placeholder(session, "region", kind, title.c_str(), subtitle.c_str(), ConceptSilhouette::Place,
        IM_COL32(36, 48, 42, 255), IM_COL32(100, 170, 130, 255));
}

void draw_poi_placeholder(const WorldForgeEditorSession& session, const WorldForgePoi& poi) {
    const char* kind = to_string(poi.kind);
    const std::string title = poi.display_name.empty() ? poi.id : poi.display_name;
    const std::string subtitle = std::string(kind) + " · " + poi.id;
    const ConceptSilhouette silhouette =
        (poi.kind == WorldForgePoiKind::Settlement || poi.kind == WorldForgePoiKind::Camp) ?
        ConceptSilhouette::Org :
        ConceptSilhouette::Landmark;
    draw_concept_placeholder(session, "poi", kind, title.c_str(), subtitle.c_str(), silhouette, IM_COL32(48, 44, 36, 255),
        IM_COL32(190, 150, 90, 255));
}

std::string endpoint_graph_key(const WorldForgeRelationshipEndpoint& endpoint) {
    if (endpoint.target == WorldForgeRelationshipEndpointTarget::Faction) return "faction:" + endpoint.id;
    return endpoint.id;
}

float point_segment_distance(const ImVec2& p, const ImVec2& a, const ImVec2& b) {
    const float abx = b.x - a.x;
    const float aby = b.y - a.y;
    const float len2 = abx * abx + aby * aby;
    if (len2 < 0.0001f) {
        const float dx = p.x - a.x;
        const float dy = p.y - a.y;
        return std::sqrt(dx * dx + dy * dy);
    }
    float t = ((p.x - a.x) * abx + (p.y - a.y) * aby) / len2;
    t = (std::clamp)(t, 0.0f, 1.0f);
    const float cx = a.x + t * abx;
    const float cy = a.y + t * aby;
    const float dx = p.x - cx;
    const float dy = p.y - cy;
    return std::sqrt(dx * dx + dy * dy);
}

void ensure_graph_layout(WorldForgeEditorSession& session, const ImVec2& canvas_size) {
    std::unordered_map<std::string, bool> needed;
    for (const auto& node : session.relationships.nodes) needed[node.id] = false;
    for (const auto& edge : session.relationships.edges) {
        needed[endpoint_graph_key(edge.from)] =
            endpoint_graph_key(edge.from).rfind("faction:", 0) == 0;
        needed[endpoint_graph_key(edge.to)] = endpoint_graph_key(edge.to).rfind("faction:", 0) == 0;
    }

    bool missing = session.graph_needs_layout;
    for (const auto& entry : needed) {
        if (session.graph_positions.find(entry.first) == session.graph_positions.end()) missing = true;
    }
    if (!missing) return;

    session.graph_positions.clear();
    const float cx = canvas_size.x * 0.5f;
    const float cy = canvas_size.y * 0.5f;
    const float radius = (std::min)(canvas_size.x, canvas_size.y) * 0.35f;

    std::vector<std::string> story_ids;
    std::vector<std::string> faction_ids;
    for (const auto& entry : needed) {
        if (entry.second) faction_ids.push_back(entry.first);
        else story_ids.push_back(entry.first);
    }
    std::sort(story_ids.begin(), story_ids.end());
    std::sort(faction_ids.begin(), faction_ids.end());

    const float two_pi = 6.28318530718f;
    for (std::size_t i = 0; i < story_ids.size(); ++i) {
        const float angle = (story_ids.size() == 1) ? -1.5708f : (two_pi * static_cast<float>(i) / static_cast<float>(story_ids.size()) - 1.5708f);
        session.graph_positions[story_ids[i]] = {cx + std::cos(angle) * radius, cy + std::sin(angle) * radius};
    }
    const float outer = radius * 1.35f;
    for (std::size_t i = 0; i < faction_ids.size(); ++i) {
        const float angle = (faction_ids.size() == 1)
                                ? 0.5236f
                                : (two_pi * static_cast<float>(i) / static_cast<float>(faction_ids.size()));
        session.graph_positions[faction_ids[i]] = {cx + std::cos(angle) * outer, cy + std::sin(angle) * outer};
    }
    session.graph_needs_layout = false;
}

int edge_kind_index(WorldForgeRelationshipEdgeKind kind) {
    switch (kind) {
    case WorldForgeRelationshipEdgeKind::Ally: return 0;
    case WorldForgeRelationshipEdgeKind::Rival: return 1;
    case WorldForgeRelationshipEdgeKind::MemberOf: return 2;
    case WorldForgeRelationshipEdgeKind::Leads: return 3;
    case WorldForgeRelationshipEdgeKind::Kin: return 4;
    case WorldForgeRelationshipEdgeKind::Serves: return 5;
    case WorldForgeRelationshipEdgeKind::Opposes: return 6;
    case WorldForgeRelationshipEdgeKind::Influences: return 7;
    case WorldForgeRelationshipEdgeKind::Related: return 8;
    }
    return 8;
}

bool node_kind_allowed(const WorldForgeEditorSession& session, WorldForgeRelationshipNodeKind kind) {
    switch (kind) {
    case WorldForgeRelationshipNodeKind::Person: return session.graph_filter_person;
    case WorldForgeRelationshipNodeKind::Deity: return session.graph_filter_deity;
    case WorldForgeRelationshipNodeKind::Artifact: return session.graph_filter_artifact;
    case WorldForgeRelationshipNodeKind::Organization: return session.graph_filter_organization;
    }
    return true;
}

bool text_matches_filter(const WorldForgeEditorSession& session, const std::string& haystack) {
    const std::string needle = trim(session.graph_filter_text.data());
    if (needle.empty()) return true;
    auto lower = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    };
    return lower(haystack).find(lower(needle)) != std::string::npos;
}

std::unordered_set<std::string> compute_visible_graph_keys(const WorldForgeEditorSession& session) {
    std::unordered_set<std::string> visible;

    auto story_passes = [&](const WorldForgeRelationshipNode& node) {
        if (!node_kind_allowed(session, node.kind)) return false;
        return text_matches_filter(session, node.id) || text_matches_filter(session, node.display_name) ||
               text_matches_filter(session, node.summary);
    };

    for (const auto& node : session.relationships.nodes) {
        if (story_passes(node)) visible.insert(node.id);
    }

    for (const auto& edge : session.relationships.edges) {
        const int kind_i = edge_kind_index(edge.kind);
        if (kind_i < 0 || kind_i >= 9 || !session.graph_filter_edge_kinds[kind_i]) continue;
        const auto from_key = endpoint_graph_key(edge.from);
        const auto to_key = endpoint_graph_key(edge.to);
        const bool from_ok = (from_key.rfind("faction:", 0) == 0)
                                 ? (!session.graph_filter_hide_factions &&
                                       (text_matches_filter(session, from_key) || text_matches_filter(session, edge.from.id)))
                                 : visible.count(from_key) != 0;
        const bool to_ok = (to_key.rfind("faction:", 0) == 0)
                               ? (!session.graph_filter_hide_factions &&
                                     (text_matches_filter(session, to_key) || text_matches_filter(session, edge.to.id)))
                               : visible.count(to_key) != 0;
        // Keep faction proxy only when linked to a visible story node (or both sides faction with filter match).
        if (from_key.rfind("faction:", 0) == 0 && to_key.rfind("faction:", 0) != 0) {
            if (visible.count(to_key) && !session.graph_filter_hide_factions) visible.insert(from_key);
        } else if (to_key.rfind("faction:", 0) == 0 && from_key.rfind("faction:", 0) != 0) {
            if (visible.count(from_key) && !session.graph_filter_hide_factions) visible.insert(to_key);
        } else if (from_key.rfind("faction:", 0) == 0 && to_key.rfind("faction:", 0) == 0) {
            if (!session.graph_filter_hide_factions && from_ok && to_ok) {
                visible.insert(from_key);
                visible.insert(to_key);
            }
        }
        (void)from_ok;
        (void)to_ok;
    }

    if (session.graph_filter_focus_neighborhood && !session.selected_id.empty()) {
        std::unordered_set<std::string> seeds;
        if (session.selected_id.rfind("faction:", 0) == 0 || find_node(session.relationships, session.selected_id) ||
            visible.count(session.selected_id)) {
            seeds.insert(session.selected_id);
        }
        if (find_edge(session.relationships, session.selected_id)) {
            for (const auto& edge : session.relationships.edges) {
                if (edge.id != session.selected_id) continue;
                seeds.insert(endpoint_graph_key(edge.from));
                seeds.insert(endpoint_graph_key(edge.to));
            }
        }
        const int hops = (std::clamp)(session.graph_expand_hops, 1, 2);
        std::unordered_set<std::string> frontier = seeds;
        std::unordered_set<std::string> neighborhood = seeds;
        for (int hop = 0; hop < hops; ++hop) {
            std::unordered_set<std::string> next;
            for (const auto& edge : session.relationships.edges) {
                const int kind_i = edge_kind_index(edge.kind);
                if (kind_i < 0 || kind_i >= 9 || !session.graph_filter_edge_kinds[kind_i]) continue;
                const auto from_key = endpoint_graph_key(edge.from);
                const auto to_key = endpoint_graph_key(edge.to);
                if (frontier.count(from_key)) {
                    next.insert(to_key);
                    neighborhood.insert(to_key);
                    neighborhood.insert(from_key);
                }
                if (frontier.count(to_key)) {
                    next.insert(from_key);
                    neighborhood.insert(from_key);
                    neighborhood.insert(to_key);
                }
            }
            frontier = std::move(next);
        }
        std::unordered_set<std::string> focused;
        for (const auto& key : visible) {
            if (neighborhood.count(key)) focused.insert(key);
        }
        // Always keep seed even if filters excluded it briefly.
        for (const auto& seed : seeds) focused.insert(seed);
        visible = std::move(focused);
    }

    return visible;
}

bool edge_visible(const WorldForgeEditorSession& session, const WorldForgeRelationshipEdge& edge,
    const std::unordered_set<std::string>& visible_keys) {
    const int kind_i = edge_kind_index(edge.kind);
    if (kind_i < 0 || kind_i >= 9 || !session.graph_filter_edge_kinds[kind_i]) return false;
    const auto from_key = endpoint_graph_key(edge.from);
    const auto to_key = endpoint_graph_key(edge.to);
    return visible_keys.count(from_key) && visible_keys.count(to_key);
}

void draw_graph_filter_bar(WorldForgeEditorSession& session) {
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputTextWithHint("##WorldForgeGraphFilter", "Filter id/name…", session.graph_filter_text.data(),
        session.graph_filter_text.size());
    ImGui::SameLine();
    ImGui::Checkbox("Person", &session.graph_filter_person);
    ImGui::SameLine();
    ImGui::Checkbox("Deity", &session.graph_filter_deity);
    ImGui::SameLine();
    ImGui::Checkbox("Artifact", &session.graph_filter_artifact);
    ImGui::SameLine();
    ImGui::Checkbox("Org", &session.graph_filter_organization);
    ImGui::SameLine();
    ImGui::Checkbox("Hide factions", &session.graph_filter_hide_factions);

    if (ImGui::TreeNode("Edge kinds##WorldForgeGraphEdgeKinds")) {
        const char* labels[] = {"ally", "rival", "member_of", "leads", "kin", "serves", "opposes", "influences",
            "related"};
        for (int i = 0; i < 9; ++i) {
            if (i) ImGui::SameLine();
            ImGui::Checkbox(labels[i], &session.graph_filter_edge_kinds[i]);
        }
        ImGui::TreePop();
    }

    ImGui::Checkbox("Focus selection neighborhood", &session.graph_filter_focus_neighborhood);
    if (session.graph_filter_focus_neighborhood) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        ImGui::SliderInt("Expand hops", &session.graph_expand_hops, 1, 2);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Fit##WorldForgeGraphFit")) session.graph_fit_requested = true;
}

void draw_relationship_graph_canvas(WorldForgeEditorSession& session, const ImVec2& size) {
    ImGui::BeginChild("WorldForgeGraphCanvas", size, true, ImGuiWindowFlags_NoScrollbar);

    draw_graph_filter_bar(session);

    const ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    const ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    if (canvas_size.x < 32.0f || canvas_size.y < 32.0f) {
        ImGui::EndChild();
        return;
    }
    ImGui::InvisibleButton("WorldForgeGraphHit", canvas_size);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const ImVec2 local{mouse.x - canvas_pos.x, mouse.y - canvas_pos.y};

    ensure_graph_layout(session, canvas_size);
    const auto visible_keys = compute_visible_graph_keys(session);
    if (session.graph_fit_requested) {
        fit_graph_camera(session, canvas_size, visible_keys);
        session.graph_fit_requested = false;
    }

    // Cap zoom so content cannot balloon far past the canvas (nodes stay manageable).
    {
        bool any = false;
        float min_x = 0.0f, max_x = 0.0f, min_y = 0.0f, max_y = 0.0f;
        for (const auto& entry : session.graph_positions) {
            if (!visible_keys.count(entry.first)) continue;
            if (!any) {
                min_x = max_x = entry.second[0];
                min_y = max_y = entry.second[1];
                any = true;
            } else {
                min_x = (std::min)(min_x, entry.second[0]);
                max_x = (std::max)(max_x, entry.second[0]);
                min_y = (std::min)(min_y, entry.second[1]);
                max_y = (std::max)(max_y, entry.second[1]);
            }
        }
        float max_zoom = 1.75f;
        if (any) {
            const float pad = 40.0f;
            const float content_w = (std::max)(max_x - min_x, 40.0f);
            const float content_h = (std::max)(max_y - min_y, 40.0f);
            max_zoom = (std::clamp)((std::min)((canvas_size.x - 2.0f * pad) / content_w,
                                         (canvas_size.y - 2.0f * pad) / content_h) *
                                         1.15f,
                0.85f, 1.85f);
        }
        if (hovered) {
            const float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f) {
                const float old_zoom = session.graph_zoom;
                session.graph_zoom =
                    (std::clamp)(session.graph_zoom * (wheel > 0.0f ? 1.1f : 1.0f / 1.1f), 0.4f, max_zoom);
                const float wx = (local.x - session.graph_pan[0]) / old_zoom;
                const float wy = (local.y - session.graph_pan[1]) / old_zoom;
                session.graph_pan[0] = local.x - wx * session.graph_zoom;
                session.graph_pan[1] = local.y - wy * session.graph_zoom;
            }
        }
        session.graph_zoom = (std::clamp)(session.graph_zoom, 0.4f, max_zoom);
    }

    // Middle mouse or Alt+LMB pans.
    const bool want_pan = hovered && (ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
                                         (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::GetIO().KeyAlt));
    if (want_pan && !session.graph_panning) {
        session.graph_panning = true;
        session.graph_pan_start_mouse = {mouse.x, mouse.y};
        session.graph_pan_start_pan = session.graph_pan;
        session.graph_drag_key.clear();
    }
    if (session.graph_panning) {
        if (want_pan) {
            session.graph_pan[0] = session.graph_pan_start_pan[0] + (mouse.x - session.graph_pan_start_mouse[0]);
            session.graph_pan[1] = session.graph_pan_start_pan[1] + (mouse.y - session.graph_pan_start_mouse[1]);
        } else {
            session.graph_panning = false;
        }
    }

    const float node_radius = graph_node_radius(session.graph_zoom, canvas_size);
    clamp_graph_camera(session, canvas_size, visible_keys, node_radius);

    auto to_screen = [&](const std::array<float, 2>& world) {
        return ImVec2(canvas_pos.x + world[0] * session.graph_zoom + session.graph_pan[0],
            canvas_pos.y + world[1] * session.graph_zoom + session.graph_pan[1]);
    };
    auto to_world = [&](const ImVec2& screen_local) {
        return std::array<float, 2>{(screen_local.x - session.graph_pan[0]) / session.graph_zoom,
            (screen_local.y - session.graph_pan[1]) / session.graph_zoom};
    };

    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(canvas_pos,
        ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
        IM_COL32(28, 30, 34, 255));
    draw->AddRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
        IM_COL32(70, 74, 82, 255));
    draw->PushClipRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), true);

    const bool selecting_edge = find_edge(session.relationships, session.selected_id) != nullptr;

    std::string hovered_edge;
    float best_edge_dist = 8.0f * session.graph_zoom;
    for (const auto& edge : session.relationships.edges) {
        if (!edge_visible(session, edge, visible_keys)) continue;
        const auto from_key = endpoint_graph_key(edge.from);
        const auto to_key = endpoint_graph_key(edge.to);
        const auto from_it = session.graph_positions.find(from_key);
        const auto to_it = session.graph_positions.find(to_key);
        if (from_it == session.graph_positions.end() || to_it == session.graph_positions.end()) continue;
        const ImVec2 a = to_screen(from_it->second);
        const ImVec2 b = to_screen(to_it->second);
        const bool selected = selecting_edge && session.selected_id == edge.id;
        const ImU32 color = selected ? IM_COL32(255, 200, 90, 255) : IM_COL32(150, 160, 180, 220);
        draw->AddLine(a, b, color, selected ? 3.0f : 2.0f);
        if (edge.bidirectional)
            draw->AddCircleFilled(ImVec2((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f), 3.0f * session.graph_zoom, color);
        const ImVec2 mid{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
        draw->AddText(ImVec2(mid.x + 4.0f, mid.y + 4.0f), IM_COL32(200, 200, 210, 230), to_string(edge.kind));
        if (hovered && !session.graph_panning) {
            const float dist = point_segment_distance(mouse, a, b);
            if (dist < best_edge_dist) {
                best_edge_dist = dist;
                hovered_edge = edge.id;
            }
        }
    }

    std::string hovered_node;
    std::size_t visible_node_count = 0;
    for (const auto& entry : session.graph_positions) {
        const auto& key = entry.first;
        if (!visible_keys.count(key)) continue;
        ++visible_node_count;
        const ImVec2 center = to_screen(entry.second);
        const bool is_faction = key.rfind("faction:", 0) == 0;
        const bool selected = !selecting_edge && session.selected_id == key;
        const bool link_from = session.graph_link_from == key;
        ImU32 fill = is_faction ? IM_COL32(70, 90, 120, 255) : IM_COL32(55, 95, 75, 255);
        if (selected) fill = IM_COL32(210, 150, 60, 255);
        if (link_from) fill = IM_COL32(90, 150, 210, 255);
        draw->AddCircleFilled(center, node_radius, fill);
        draw->AddCircle(center, node_radius, IM_COL32(230, 230, 235, 255), 0, 1.5f);

        std::string label = key;
        if (!is_faction) {
            if (const auto* node = find_node(session.relationships, key); node && !node->display_name.empty())
                label = node->display_name;
        } else {
            label = key.substr(8);
        }
        const ImVec2 text_size = ImGui::CalcTextSize(label.c_str());
        draw->AddText(ImVec2(center.x - text_size.x * 0.5f, center.y - text_size.y * 0.5f),
            IM_COL32(245, 245, 245, 255), label.c_str());

        const float dx = mouse.x - center.x;
        const float dy = mouse.y - center.y;
        if (hovered && !session.graph_panning && (dx * dx + dy * dy) <= (node_radius * node_radius))
            hovered_node = key;
    }
    std::vector<std::string> visible_key_list(visible_keys.begin(), visible_keys.end());
    const auto rel_bounds = compute_graph_bounds(session.graph_positions, &visible_key_list);
    WorldForgeGraphCamera rel_cam;
    rel_cam.zoom = session.graph_zoom;
    rel_cam.pan = session.graph_pan;
    rel_cam.min_zoom = 0.4f;
    rel_cam.max_zoom = 2.0f;
    const bool minimap_clicked = draw_world_forge_graph_minimap(draw, canvas_pos.x, canvas_pos.y, canvas_size.x,
        canvas_size.y, rel_cam, rel_bounds, session.graph_positions, mouse.x, mouse.y,
        hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().KeyAlt);
    session.graph_zoom = rel_cam.zoom;
    session.graph_pan = rel_cam.pan;
    draw->PopClipRect();

    if (!minimap_clicked && !session.graph_panning && active && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !ImGui::GetIO().KeyAlt) {
        if (!hovered_node.empty()) {
            if (!session.graph_link_from.empty() && session.graph_link_from != hovered_node) {
                WorldForgeRelationshipEndpoint from{};
                WorldForgeRelationshipEndpoint to{};
                parse_graph_endpoint(session.graph_link_from, from);
                parse_graph_endpoint(hovered_node, to);
                const std::string id = unique_relationship_id(session.relationships,
                    from.id + "_related_" + to.id);
                add_relationship_edge(session, id, from, to, WorldForgeRelationshipEdgeKind::Related);
            } else {
                session.graph_drag_key = hovered_node;
                session.selected_id = hovered_node;
            }
        } else if (!hovered_edge.empty()) {
            session.graph_drag_key.clear();
            session.selected_id = hovered_edge;
            session.graph_link_from.clear();
        } else {
            session.graph_drag_key.clear();
            session.graph_link_from.clear();
        }
    }
    if (!session.graph_panning && active && !session.graph_drag_key.empty() &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left) && !ImGui::GetIO().KeyAlt) {
        auto it = session.graph_positions.find(session.graph_drag_key);
        if (it != session.graph_positions.end()) {
            it->second = to_world(local);
            clamp_node_world_to_canvas(session, session.graph_drag_key, canvas_size, node_radius);
        }
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) session.graph_drag_key.clear();

    if (ImGui::Button("Reset layout##WorldForgeGraph")) {
        session.graph_needs_layout = true;
        session.graph_fit_requested = true;
        session.graph_link_from.clear();
    }
    ImGui::SameLine();
    if (!session.graph_link_from.empty()) {
        if (ImGui::Button("Cancel link##WorldForgeCancelLink")) session.graph_link_from.clear();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.55f, 0.75f, 1.0f, 1.0f), "Click target node to link from %s",
            session.graph_link_from.c_str());
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%zu shown · %zu nodes · %zu edges · zoom %.2f  (wheel zoom, Alt+drag pan)",
        visible_node_count, session.relationships.nodes.size(), session.relationships.edges.size(),
        session.graph_zoom);
    ImGui::EndChild();
}

void draw_factions_pane(WorldForgeEditorSession& session) {
    session.list_kind = ListKind::Entities;
    draw_add_faction_controls(session);
    ImGui::Separator();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    float list_w = 0.0f;
    begin_list_detail(avail, list_w);

    ImGui::BeginChild("WorldForgeFactionsList", ImVec2(list_w, avail.y), true);
    if (session.factions.entities.empty()) ImGui::TextDisabled("(no factions — use Add faction above)");
    for (const auto& entity : session.factions.entities) {
        std::string label = entity.id;
        if (!entity.display_name.empty()) label += "  (" + entity.display_name + ")";
        const bool selected = session.selected_id == entity.id;
        if (ImGui::Selectable(label.c_str(), selected)) session.selected_id = entity.id;
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("WorldForgeFactionsDetail", ImVec2(0.0f, avail.y), true);
    auto* entity = find_faction(session.factions, session.selected_id);
    if (!entity) {
        ImGui::TextDisabled("Select a faction/culture/clan/warband, or create one above");
    } else {
        draw_faction_placeholder(session, *entity);
        ImGui::Separator();
        ImGui::Text("id: %s", entity->id.c_str());
        if (draw_input_text("displayName", entity->display_name)) session.dirty = true;
        if (draw_enum_combo("kind", entity->kind,
                {WorldForgeFactionKind::Faction, WorldForgeFactionKind::Culture, WorldForgeFactionKind::Clan,
                    WorldForgeFactionKind::Warband}))
            session.dirty = true;
        if (draw_enum_combo("canonStatus", entity->canon_status,
                {WorldForgeCanonStatus::Established, WorldForgeCanonStatus::Draft, WorldForgeCanonStatus::Proposal,
                    WorldForgeCanonStatus::Open}))
            session.dirty = true;
        if (draw_political_role_combo("politicalRole", entity->political_role)) session.dirty = true;
        if (draw_text_area("summary", entity->summary, 96.0f)) session.dirty = true;
        if (draw_input_text("storyRef", entity->story_ref)) session.dirty = true;
        if (draw_id_combo("parentId", entity->parent_id, collect_faction_ids(session.factions))) session.dirty = true;
        if (draw_csv_field("tags (comma-separated)", entity->tags)) session.dirty = true;
        if (draw_open_questions_field(entity->open_questions)) session.dirty = true;
        draw_faction_standing_section(session, *entity);
    }
    ImGui::EndChild();
}

void draw_relationships_pane(WorldForgeEditorSession& session) {
    if (session.list_kind != ListKind::Nodes && session.list_kind != ListKind::Edges &&
        session.list_kind != ListKind::Graph)
        session.list_kind = ListKind::Nodes;

    if (ImGui::RadioButton("Nodes##WorldForgeRelKind", session.list_kind == ListKind::Nodes)) {
        session.list_kind = ListKind::Nodes;
        session.selected_id.clear();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Edges##WorldForgeRelKind", session.list_kind == ListKind::Edges)) {
        session.list_kind = ListKind::Edges;
        session.selected_id.clear();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Graph##WorldForgeRelKind", session.list_kind == ListKind::Graph)) {
        session.list_kind = ListKind::Graph;
        session.graph_needs_layout = true;
    }
    ImGui::Separator();

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    float list_w = 0.0f;
    if (session.list_kind == ListKind::Graph) {
        list_w = (std::clamp)(avail.x * 0.70f, 320.0f, avail.x - 200.0f);
        draw_relationship_graph_canvas(session, ImVec2(list_w, avail.y));
        ImGui::SameLine();
        ImGui::BeginChild("WorldForgeGraphDetail", ImVec2(0.0f, avail.y), true);
        draw_add_node_controls(session);
        draw_add_edge_controls(session);
        ImGui::Separator();
        // Prefer edge detail if the selected id matches an edge; otherwise node.
        if (auto* edge = find_edge(session.relationships, session.selected_id)) {
            ImGui::TextDisabled("Edge");
            ImGui::Text("id: %s", edge->id.c_str());
            if (ImGui::Button("Delete relationship##WorldForgeDeleteEdgeGraph")) {
                remove_relationship_edge(session, edge->id);
                ImGui::EndChild();
                return;
            }
            ImGui::Separator();
            if (draw_enum_combo("from.target", edge->from.target,
                    {WorldForgeRelationshipEndpointTarget::Node, WorldForgeRelationshipEndpointTarget::Faction}))
                session.dirty = true;
            if (draw_id_combo("from.id", edge->from.id, collect_endpoint_ids(session, edge->from.target), false)) {
                session.dirty = true;
                session.graph_needs_layout = true;
            }
            if (draw_enum_combo("to.target", edge->to.target,
                    {WorldForgeRelationshipEndpointTarget::Node, WorldForgeRelationshipEndpointTarget::Faction}))
                session.dirty = true;
            if (draw_id_combo("to.id", edge->to.id, collect_endpoint_ids(session, edge->to.target), false)) {
                session.dirty = true;
                session.graph_needs_layout = true;
            }
            if (draw_enum_combo("kind", edge->kind,
                    {WorldForgeRelationshipEdgeKind::Ally, WorldForgeRelationshipEdgeKind::Rival,
                        WorldForgeRelationshipEdgeKind::MemberOf, WorldForgeRelationshipEdgeKind::Leads,
                        WorldForgeRelationshipEdgeKind::Kin, WorldForgeRelationshipEdgeKind::Serves,
                        WorldForgeRelationshipEdgeKind::Opposes, WorldForgeRelationshipEdgeKind::Influences,
                        WorldForgeRelationshipEdgeKind::Related}))
                session.dirty = true;
            if (draw_enum_combo("canonStatus", edge->canon_status,
                    {WorldForgeRelationshipCanonStatus::Established, WorldForgeRelationshipCanonStatus::Draft,
                        WorldForgeRelationshipCanonStatus::Proposal, WorldForgeRelationshipCanonStatus::Open}))
                session.dirty = true;
            if (ImGui::Checkbox("bidirectional", &edge->bidirectional)) session.dirty = true;
            if (draw_edge_standing_transfer(session, *edge)) {
            }
            if (draw_text_area("summary", edge->summary, 80.0f)) session.dirty = true;
            if (draw_input_text("storyRef", edge->story_ref)) session.dirty = true;
            if (draw_open_questions_field(edge->open_questions)) session.dirty = true;
        } else if (auto* node = find_node(session.relationships, session.selected_id)) {
            ImGui::TextDisabled("Node");
            draw_relationship_node_placeholder(session, *node);
            ImGui::Separator();
            ImGui::Text("id: %s", node->id.c_str());
            if (ImGui::Button("Link from this…##WorldForgeLinkFrom")) {
                session.graph_link_from = node->id;
                session.status = "Click another node on the graph to create a relationship";
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete node##WorldForgeDeleteNodeGraph")) {
                remove_relationship_node(session, node->id);
                ImGui::EndChild();
                return;
            }
            ImGui::Separator();
            if (draw_input_text("displayName", node->display_name)) session.dirty = true;
            if (draw_enum_combo("kind", node->kind,
                    {WorldForgeRelationshipNodeKind::Person, WorldForgeRelationshipNodeKind::Deity,
                        WorldForgeRelationshipNodeKind::Artifact, WorldForgeRelationshipNodeKind::Organization}))
                session.dirty = true;
            if (draw_enum_combo("canonStatus", node->canon_status,
                    {WorldForgeRelationshipCanonStatus::Established, WorldForgeRelationshipCanonStatus::Draft,
                        WorldForgeRelationshipCanonStatus::Proposal, WorldForgeRelationshipCanonStatus::Open}))
                session.dirty = true;
            if (draw_text_area("summary", node->summary, 96.0f)) session.dirty = true;
            if (draw_input_text("storyRef", node->story_ref)) session.dirty = true;
            if (draw_csv_field("tags (comma-separated)", node->tags)) session.dirty = true;
            if (draw_open_questions_field(node->open_questions)) session.dirty = true;
        } else if (session.selected_id.rfind("faction:", 0) == 0) {
            ImGui::TextDisabled("Faction endpoint (registry)");
            const std::string faction_id = session.selected_id.substr(std::string("faction:").size());
            if (auto* faction = find_faction(session.factions, faction_id)) {
                draw_faction_placeholder(session, *faction);
                ImGui::Separator();
            }
            ImGui::TextUnformatted(session.selected_id.c_str());
            if (ImGui::Button("Link from this…##WorldForgeLinkFromFaction")) {
                session.graph_link_from = session.selected_id;
                session.status = "Click another node on the graph to create a relationship";
            }
            ImGui::TextWrapped(
                "Faction proxies come from edge endpoints. Edit them under the Factions pane or in "
                "factions.worldforge.json.");
        } else {
            ImGui::TextDisabled("Click a node or edge on the graph");
            ImGui::TextWrapped(
                "Drag nodes to rearrange (ephemeral). Use Add node / Add relationship, or Link from a selected "
                "node. Save writes JSON via apply_world_forge_operation.");
        }
        ImGui::EndChild();
        return;
    }

    begin_list_detail(avail, list_w);
    if (session.list_kind == ListKind::Nodes) {
        ImGui::BeginChild("WorldForgeNodesList", ImVec2(list_w, avail.y), true);
        draw_add_node_controls(session);
        ImGui::Separator();
        if (session.relationships.nodes.empty()) ImGui::TextDisabled("(no nodes)");
        for (const auto& node : session.relationships.nodes) {
            std::string label = node.id;
            if (!node.display_name.empty()) label += "  (" + node.display_name + ")";
            const bool selected = session.selected_id == node.id;
            if (ImGui::Selectable(label.c_str(), selected)) session.selected_id = node.id;
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("WorldForgeNodesDetail", ImVec2(0.0f, avail.y), true);
        auto* node = find_node(session.relationships, session.selected_id);
        if (!node) {
            ImGui::TextDisabled("Select a relationship node");
        } else {
            draw_relationship_node_placeholder(session, *node);
            ImGui::Separator();
            ImGui::Text("id: %s", node->id.c_str());
            if (ImGui::Button("Delete node##WorldForgeDeleteNodeList")) {
                remove_relationship_node(session, node->id);
                ImGui::EndChild();
                return;
            }
            ImGui::Separator();
            if (draw_input_text("displayName", node->display_name)) session.dirty = true;
            if (draw_enum_combo("kind", node->kind,
                    {WorldForgeRelationshipNodeKind::Person, WorldForgeRelationshipNodeKind::Deity,
                        WorldForgeRelationshipNodeKind::Artifact, WorldForgeRelationshipNodeKind::Organization}))
                session.dirty = true;
            if (draw_enum_combo("canonStatus", node->canon_status,
                    {WorldForgeRelationshipCanonStatus::Established, WorldForgeRelationshipCanonStatus::Draft,
                        WorldForgeRelationshipCanonStatus::Proposal, WorldForgeRelationshipCanonStatus::Open}))
                session.dirty = true;
            if (draw_text_area("summary", node->summary, 96.0f)) session.dirty = true;
            if (draw_input_text("storyRef", node->story_ref)) session.dirty = true;
            if (draw_csv_field("tags (comma-separated)", node->tags)) session.dirty = true;
            if (draw_open_questions_field(node->open_questions)) session.dirty = true;
        }
        ImGui::EndChild();
    } else {
        ImGui::BeginChild("WorldForgeEdgesList", ImVec2(list_w, avail.y), true);
        draw_add_edge_controls(session);
        ImGui::Separator();
        if (session.relationships.edges.empty()) ImGui::TextDisabled("(no edges)");
        for (const auto& edge : session.relationships.edges) {
            std::string label = edge.id + "  [" + to_string(edge.kind) + "] " + edge.from.id + " -> " + edge.to.id;
            const bool selected = session.selected_id == edge.id;
            if (ImGui::Selectable(label.c_str(), selected)) session.selected_id = edge.id;
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("WorldForgeEdgesDetail", ImVec2(0.0f, avail.y), true);
        auto* edge = find_edge(session.relationships, session.selected_id);
        if (!edge) {
            ImGui::TextDisabled("Select a relationship edge");
        } else {
            ImGui::Text("id: %s", edge->id.c_str());
            if (ImGui::Button("Delete relationship##WorldForgeDeleteEdgeList")) {
                remove_relationship_edge(session, edge->id);
                ImGui::EndChild();
                return;
            }
            ImGui::Separator();
            if (draw_enum_combo("from.target", edge->from.target,
                    {WorldForgeRelationshipEndpointTarget::Node, WorldForgeRelationshipEndpointTarget::Faction}))
                session.dirty = true;
            if (draw_id_combo("from.id", edge->from.id, collect_endpoint_ids(session, edge->from.target), false))
                session.dirty = true;
            if (draw_enum_combo("to.target", edge->to.target,
                    {WorldForgeRelationshipEndpointTarget::Node, WorldForgeRelationshipEndpointTarget::Faction}))
                session.dirty = true;
            if (draw_id_combo("to.id", edge->to.id, collect_endpoint_ids(session, edge->to.target), false))
                session.dirty = true;
            if (draw_enum_combo("kind", edge->kind,
                    {WorldForgeRelationshipEdgeKind::Ally, WorldForgeRelationshipEdgeKind::Rival,
                        WorldForgeRelationshipEdgeKind::MemberOf, WorldForgeRelationshipEdgeKind::Leads,
                        WorldForgeRelationshipEdgeKind::Kin, WorldForgeRelationshipEdgeKind::Serves,
                        WorldForgeRelationshipEdgeKind::Opposes, WorldForgeRelationshipEdgeKind::Influences,
                        WorldForgeRelationshipEdgeKind::Related}))
                session.dirty = true;
            if (draw_enum_combo("canonStatus", edge->canon_status,
                    {WorldForgeRelationshipCanonStatus::Established, WorldForgeRelationshipCanonStatus::Draft,
                        WorldForgeRelationshipCanonStatus::Proposal, WorldForgeRelationshipCanonStatus::Open}))
                session.dirty = true;
            if (ImGui::Checkbox("bidirectional", &edge->bidirectional)) session.dirty = true;
            if (draw_edge_standing_transfer(session, *edge)) {
            }
            if (draw_text_area("summary", edge->summary, 80.0f)) session.dirty = true;
            if (draw_input_text("storyRef", edge->story_ref)) session.dirty = true;
            if (draw_open_questions_field(edge->open_questions)) session.dirty = true;
        }
        ImGui::EndChild();
    }
}

void draw_map_pane(WorldForgeEditorSession& session) {
    if (session.list_kind != ListKind::Regions && session.list_kind != ListKind::Pois &&
        session.list_kind != ListKind::Links)
        session.list_kind = ListKind::Regions;

    if (ImGui::RadioButton("Regions##WorldForgeMapKind", session.list_kind == ListKind::Regions)) {
        session.list_kind = ListKind::Regions;
        session.selected_id.clear();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("POIs##WorldForgeMapKind", session.list_kind == ListKind::Pois)) {
        session.list_kind = ListKind::Pois;
        session.selected_id.clear();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Links##WorldForgeMapKind", session.list_kind == ListKind::Links)) {
        session.list_kind = ListKind::Links;
        session.selected_id.clear();
    }
    ImGui::Separator();
    if (session.list_kind == ListKind::Regions) draw_add_region_controls(session);
    else if (session.list_kind == ListKind::Pois) draw_add_poi_controls(session);
    else draw_add_map_link_controls(session);
    ImGui::Separator();

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    float list_w = 0.0f;
    begin_list_detail(avail, list_w);

    if (session.list_kind == ListKind::Regions) {
        ImGui::BeginChild("WorldForgeRegionsList", ImVec2(list_w, avail.y), true);
        if (session.map.regions.empty()) ImGui::TextDisabled("(no regions — use Add region above)");
        for (const auto& region : session.map.regions) {
            std::string label = region.id;
            if (!region.display_name.empty()) label += "  (" + region.display_name + ")";
            const bool selected = session.selected_id == region.id;
            if (ImGui::Selectable(label.c_str(), selected)) session.selected_id = region.id;
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("WorldForgeRegionsDetail", ImVec2(0.0f, avail.y), true);
        auto* region = find_region(session.map, session.selected_id);
        if (!region) {
            ImGui::TextDisabled("Select a region, or create one above");
        } else {
            draw_region_placeholder(session, *region);
            ImGui::Separator();
            ImGui::Text("id: %s", region->id.c_str());
            if (draw_input_text("displayName", region->display_name)) session.dirty = true;
            if (draw_enum_combo("kind", region->kind,
                    {WorldForgeRegionKind::Region, WorldForgeRegionKind::Fortress, WorldForgeRegionKind::City,
                        WorldForgeRegionKind::Wilderness, WorldForgeRegionKind::Chaotic,
                        WorldForgeRegionKind::Settlement, WorldForgeRegionKind::Other}))
                session.dirty = true;
            if (draw_enum_combo("canonStatus", region->canon_status,
                    {WorldForgeMapCanonStatus::Established, WorldForgeMapCanonStatus::Draft,
                        WorldForgeMapCanonStatus::Proposal, WorldForgeMapCanonStatus::Open}))
                session.dirty = true;
            if (draw_text_area("summary", region->summary, 96.0f)) session.dirty = true;
            if (draw_input_text("storyRef", region->story_ref)) session.dirty = true;
            if (draw_id_combo("parentRegionId", region->parent_region_id, collect_region_ids(session.map)))
                session.dirty = true;
            if (draw_csv_field("factionIds (comma-separated)", region->faction_ids)) session.dirty = true;
            if (draw_csv_field("tags (comma-separated)", region->tags)) session.dirty = true;
            if (ImGui::Checkbox("softGate.enabled", &region->soft_gate.enabled)) session.dirty = true;
            if (draw_text_area("softGate.notes", region->soft_gate.notes, 72.0f, 2048)) session.dirty = true;
            if (draw_open_questions_field(region->open_questions)) session.dirty = true;
        }
        ImGui::EndChild();
    } else if (session.list_kind == ListKind::Pois) {
        ImGui::BeginChild("WorldForgePoisList", ImVec2(list_w, avail.y), true);
        if (session.map.pois.empty()) ImGui::TextDisabled("(no POIs — use Add POI above)");
        for (const auto& poi : session.map.pois) {
            std::string label = poi.id;
            if (!poi.display_name.empty()) label += "  (" + poi.display_name + ")";
            const bool selected = session.selected_id == poi.id;
            if (ImGui::Selectable(label.c_str(), selected)) session.selected_id = poi.id;
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("WorldForgePoisDetail", ImVec2(0.0f, avail.y), true);
        auto* poi = find_poi(session.map, session.selected_id);
        if (!poi) {
            ImGui::TextDisabled("Select a POI, or create one above");
        } else {
            draw_poi_placeholder(session, *poi);
            ImGui::Separator();
            ImGui::Text("id: %s", poi->id.c_str());
            if (draw_input_text("displayName", poi->display_name)) session.dirty = true;
            if (draw_enum_combo("kind", poi->kind,
                    {WorldForgePoiKind::Landmark, WorldForgePoiKind::Settlement, WorldForgePoiKind::Gate,
                        WorldForgePoiKind::Shrine, WorldForgePoiKind::Camp, WorldForgePoiKind::Other}))
                session.dirty = true;
            if (draw_enum_combo("canonStatus", poi->canon_status,
                    {WorldForgeMapCanonStatus::Established, WorldForgeMapCanonStatus::Draft,
                        WorldForgeMapCanonStatus::Proposal, WorldForgeMapCanonStatus::Open}))
                session.dirty = true;
            if (draw_id_combo("regionId", poi->region_id, collect_region_ids(session.map), false)) session.dirty = true;
            if (draw_text_area("summary", poi->summary, 96.0f)) session.dirty = true;
            if (draw_input_text("storyRef", poi->story_ref)) session.dirty = true;
            if (draw_input_text("sceneEntityId", poi->scene_entity_id)) session.dirty = true;
            if (draw_input_text("prefabId", poi->prefab_id)) session.dirty = true;
            if (draw_csv_field("tags (comma-separated)", poi->tags)) session.dirty = true;
            if (draw_open_questions_field(poi->open_questions)) session.dirty = true;
        }
        ImGui::EndChild();
    } else {
        ImGui::BeginChild("WorldForgeLinksList", ImVec2(list_w, avail.y), true);
        if (session.map.links.empty()) ImGui::TextDisabled("(no links — use Add link above)");
        for (const auto& link : session.map.links) {
            std::string label = link.id + "  [" + to_string(link.kind) + "] " + link.from_id + " -> " + link.to_id;
            const bool selected = session.selected_id == link.id;
            if (ImGui::Selectable(label.c_str(), selected)) session.selected_id = link.id;
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("WorldForgeLinksDetail", ImVec2(0.0f, avail.y), true);
        auto* link = find_link(session.map, session.selected_id);
        if (!link) {
            ImGui::TextDisabled("Select a map link, or create one above");
        } else {
            ImGui::Text("id: %s", link->id.c_str());
            ImGui::Separator();
            if (draw_enum_combo("kind", link->kind,
                    {WorldForgeMapLinkKind::Travel, WorldForgeMapLinkKind::SoftGate, WorldForgeMapLinkKind::StoryGate,
                        WorldForgeMapLinkKind::Adjacency}))
                session.dirty = true;
            if (draw_enum_combo("fromKind", link->from_kind,
                    {WorldForgeMapEndpointKind::Region, WorldForgeMapEndpointKind::Poi}))
                session.dirty = true;
            if (draw_id_combo("fromId", link->from_id, collect_map_endpoint_ids(session.map), false)) session.dirty = true;
            if (draw_enum_combo(
                    "toKind", link->to_kind, {WorldForgeMapEndpointKind::Region, WorldForgeMapEndpointKind::Poi}))
                session.dirty = true;
            if (draw_id_combo("toId", link->to_id, collect_map_endpoint_ids(session.map), false)) session.dirty = true;
            if (draw_enum_combo("canonStatus", link->canon_status,
                    {WorldForgeMapCanonStatus::Established, WorldForgeMapCanonStatus::Draft,
                        WorldForgeMapCanonStatus::Proposal, WorldForgeMapCanonStatus::Open}))
                session.dirty = true;
            if (ImGui::Checkbox("bidirectional", &link->bidirectional)) session.dirty = true;
            if (ImGui::Checkbox("softGate.enabled", &link->soft_gate.enabled)) session.dirty = true;
            if (draw_text_area("softGate.notes", link->soft_gate.notes, 72.0f, 2048)) session.dirty = true;
            if (draw_text_area("summary", link->summary, 80.0f)) session.dirty = true;
            if (draw_input_text("storyRef", link->story_ref)) session.dirty = true;
            if (draw_open_questions_field(link->open_questions)) session.dirty = true;
        }
        ImGui::EndChild();
    }
}

void draw_quests_pane(WorldForgeEditorSession& session) {
    session.list_kind = ListKind::Quests;
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    float list_w = 0.0f;
    begin_list_detail(avail, list_w);

    ImGui::BeginChild("WorldForgeQuestsList", ImVec2(list_w, avail.y), true);
    draw_add_quest_controls(session);
    ImGui::Separator();
    if (session.quests.quests.empty()) ImGui::TextDisabled("(no quests)");
    for (const auto& quest : session.quests.quests) {
        std::string label = quest.id;
        if (!quest.display_name.empty()) label += "  (" + quest.display_name + ")";
        const bool selected = session.selected_id == quest.id;
        if (ImGui::Selectable(label.c_str(), selected)) session.selected_id = quest.id;
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("WorldForgeQuestsDetail", ImVec2(0.0f, avail.y), true);
    auto* quest = find_quest(session.quests, session.selected_id);
    if (!quest) {
        ImGui::TextDisabled("Select a quest or create one");
    } else {
        static std::string last_quest_id;
        static std::string selected_objective_id;
        static std::string selected_fork_id;
        if (last_quest_id != quest->id) {
            last_quest_id = quest->id;
            selected_objective_id.clear();
            selected_fork_id.clear();
        }

        ImGui::Text("id: %s", quest->id.c_str());
        if (ImGui::Button("Delete quest##WorldForgeDeleteQuest")) {
            const auto id = quest->id;
            if (remove_quest(session, id)) {
                ImGui::EndChild();
                return;
            }
            session.status = "Could not remove quest";
        }
        ImGui::Separator();
        if (draw_input_text("displayName", quest->display_name)) session.dirty = true;
        if (draw_enum_combo("kind", quest->kind,
                {WorldForgeQuestKind::Main, WorldForgeQuestKind::Side, WorldForgeQuestKind::Faction}))
            session.dirty = true;
        if (draw_enum_combo("canonStatus", quest->canon_status,
                {WorldForgeQuestCanonStatus::Established, WorldForgeQuestCanonStatus::Draft,
                    WorldForgeQuestCanonStatus::Proposal, WorldForgeQuestCanonStatus::Open}))
            session.dirty = true;
        if (ImGui::Checkbox("consequential", &quest->consequential)) session.dirty = true;
        if (draw_text_area("summary", quest->summary, 96.0f)) session.dirty = true;
        if (draw_input_text("storyRef", quest->story_ref)) session.dirty = true;
        if (draw_id_combo("regionId", quest->region_id, collect_region_ids(session.map))) session.dirty = true;
        if (draw_text_area("starts", quest->starts, 72.0f, 2048)) session.dirty = true;
        ImGui::Separator();
        ImGui::TextUnformatted("dialogue hooks (DEC-0026)");
        {
            const auto tree_ids = collect_dialogue_tree_ids(session.dialogues);
            if (draw_id_combo("dialogue.startId", quest->dialogue.start_id, tree_ids)) session.dirty = true;
            if (draw_id_combo("dialogue.completeId", quest->dialogue.complete_id, tree_ids)) session.dirty = true;
            if (draw_id_combo("dialogue.abandonId", quest->dialogue.abandon_id, tree_ids)) session.dirty = true;
        }
        ImGui::Separator();
        ImGui::Text("objectives (%zu)", quest->objectives.size());
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputTextWithHint("##CreateObjectiveId", "title / summary", session.create_objective_id.data(),
            session.create_objective_id.size());
        ImGui::SameLine();
        if (ImGui::Button("Add objective##WorldForgeAddObjective")) {
            const auto title = trim(session.create_objective_id.data());
            const auto id = unique_slugify_id(
                title,
                [&](const std::string& candidate) {
                    for (const auto& objective : quest->objectives) {
                        if (objective.id == candidate) return true;
                    }
                    return false;
                },
                "objective");
            if (title.empty()) {
                session.status = "Enter an objective title";
            } else if (add_quest_objective(*quest, id, title)) {
                session.dirty = true;
                selected_objective_id = id;
                session.create_objective_id.fill('\0');
                session.status = "Added objective " + id;
            } else {
                session.status = "Could not add objective (invalid or duplicate id)";
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(selected_objective_id.empty());
        if (ImGui::Button("Delete objective##WorldForgeDeleteObjective")) {
            if (remove_quest_objective(*quest, selected_objective_id)) {
                session.dirty = true;
                session.status = "Removed objective " + selected_objective_id;
                selected_objective_id.clear();
            }
        }
        ImGui::EndDisabled();
        for (std::size_t i = 0; i < quest->objectives.size(); ++i) {
            auto& objective = quest->objectives[i];
            ImGui::PushID(static_cast<int>(i));
            const bool obj_selected = selected_objective_id == objective.id;
            if (ImGui::Selectable(objective.id.c_str(), obj_selected)) selected_objective_id = objective.id;
            if (obj_selected) {
                if (draw_text_area("summary##obj", objective.summary, 64.0f, 2048)) session.dirty = true;
                if (draw_id_combo("dialogueId##obj", objective.dialogue_id, collect_dialogue_tree_ids(session.dialogues)))
                    session.dirty = true;
            }
            ImGui::PopID();
        }
        ImGui::Separator();
        ImGui::Text("forks (%zu)", quest->forks.size());
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputTextWithHint("##CreateForkId", "title / summary", session.create_fork_id.data(),
            session.create_fork_id.size());
        ImGui::SameLine();
        if (ImGui::Button("Add fork##WorldForgeAddFork")) {
            const auto title = trim(session.create_fork_id.data());
            const auto id = unique_slugify_id(
                title,
                [&](const std::string& candidate) {
                    for (const auto& fork : quest->forks) {
                        if (fork.id == candidate) return true;
                    }
                    return false;
                },
                "fork");
            if (title.empty()) {
                session.status = "Enter a fork title";
            } else if (add_quest_fork(*quest, id, title)) {
                session.dirty = true;
                selected_fork_id = id;
                session.create_fork_id.fill('\0');
                session.status = "Added fork " + id;
            } else {
                session.status = "Could not add fork (invalid or duplicate id)";
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(selected_fork_id.empty());
        if (ImGui::Button("Delete fork##WorldForgeDeleteFork")) {
            if (remove_quest_fork(*quest, selected_fork_id)) {
                session.dirty = true;
                session.status = "Removed fork " + selected_fork_id;
                selected_fork_id.clear();
            }
        }
        ImGui::EndDisabled();
        for (std::size_t i = 0; i < quest->forks.size(); ++i) {
            auto& fork = quest->forks[i];
            ImGui::PushID(static_cast<int>(1000 + i));
            const bool fork_selected = selected_fork_id == fork.id;
            if (ImGui::Selectable(fork.id.c_str(), fork_selected)) selected_fork_id = fork.id;
            if (fork_selected) {
                if (draw_text_area("summary##fork", fork.summary, 64.0f, 2048)) session.dirty = true;
                if (draw_csv_field("outcomeFlags##fork", fork.outcome_flags)) session.dirty = true;
                if (draw_id_combo("dialogueId##fork", fork.dialogue_id, collect_dialogue_tree_ids(session.dialogues)))
                    session.dirty = true;
            }
            ImGui::PopID();
        }
        draw_quest_standing_section(session, *quest);
        ImGui::Separator();
        if (draw_csv_field("tags (comma-separated)", quest->tags)) session.dirty = true;
        if (draw_open_questions_field(quest->open_questions)) session.dirty = true;
    }
    ImGui::EndChild();
}

void push_dialogue_nav(WorldForgeEditorSession& session, const std::string& node_id) {
    if (node_id.empty() || session.dialogue_nav_suppress) return;
    if (session.dialogue_nav_history_index >= 0 &&
        session.dialogue_nav_history_index < static_cast<int>(session.dialogue_nav_history.size()) &&
        session.dialogue_nav_history[static_cast<std::size_t>(session.dialogue_nav_history_index)] == node_id)
        return;
    if (session.dialogue_nav_history_index + 1 < static_cast<int>(session.dialogue_nav_history.size()))
        session.dialogue_nav_history.resize(static_cast<std::size_t>(session.dialogue_nav_history_index + 1));
    session.dialogue_nav_history.push_back(node_id);
    session.dialogue_nav_history_index = static_cast<int>(session.dialogue_nav_history.size()) - 1;
}

void select_dialogue_node(WorldForgeEditorSession& session, const std::string& node_id, bool record_history) {
    session.dialogue_selected_node_id = node_id;
    if (record_history) push_dialogue_nav(session, node_id);
}

/// Place missing node positions without wiping the graph. New/orphan nodes sit near `near_node_id` when possible.
void place_dialogue_node_incremental(WorldForgeEditorSession& session, const WorldForgeDialogueTree& tree,
    const std::string& new_node_id, const std::string& near_node_id) {
    layout_dialogue_graph(tree, session.dialogue_graph_positions, true);
    auto it = session.dialogue_graph_positions.find(new_node_id);
    if (it == session.dialogue_graph_positions.end()) return;
    const auto near = session.dialogue_graph_positions.find(near_node_id);
    if (!near_node_id.empty() && near != session.dialogue_graph_positions.end() && near_node_id != new_node_id) {
        it->second[0] = near->second[0] + 260.0f;
        it->second[1] = near->second[1] + 40.0f;
    }
}

void push_dialogue_undo(WorldForgeEditorSession& session, const WorldForgeDialogueTree& tree) {
    session.dialogue_undo_stack.push_back(tree);
    if (session.dialogue_undo_stack.size() > 64) session.dialogue_undo_stack.erase(session.dialogue_undo_stack.begin());
    session.dialogue_redo_stack.clear();
}

bool apply_dialogue_undo(WorldForgeEditorSession& session, WorldForgeDialogueTree& tree) {
    if (session.dialogue_undo_stack.empty()) return false;
    session.dialogue_redo_stack.push_back(tree);
    tree = std::move(session.dialogue_undo_stack.back());
    session.dialogue_undo_stack.pop_back();
    session.dirty = true;
    // Keep camera; only fill missing positions for nodes that came back.
    layout_dialogue_graph(tree, session.dialogue_graph_positions, true);
    return true;
}

bool apply_dialogue_redo(WorldForgeEditorSession& session, WorldForgeDialogueTree& tree) {
    if (session.dialogue_redo_stack.empty()) return false;
    session.dialogue_undo_stack.push_back(tree);
    tree = std::move(session.dialogue_redo_stack.back());
    session.dialogue_redo_stack.pop_back();
    session.dirty = true;
    layout_dialogue_graph(tree, session.dialogue_graph_positions, true);
    return true;
}

void draw_dialogue_line_preview(ImDrawList* draw, const ImVec2& pad_min, const ImVec2& pad_max, const std::string& speaker,
    const std::string& line, DialogueGraphNodeDisplayMode mode, ImU32 speaker_col, ImU32 line_col) {
    const float max_w = (std::max)(8.0f, pad_max.x - pad_min.x);
    const float line_h = ImGui::GetTextLineHeight();
    const std::string speaker_text = ellipsize_fit(speaker.empty() ? "(no speaker)" : speaker, max_w);
    draw->AddText(ImVec2(pad_min.x, pad_min.y), speaker_col, speaker_text.c_str());
    if (mode == DialogueGraphNodeDisplayMode::Compact) return;
    const int max_lines = mode == DialogueGraphNodeDisplayMode::Expanded ? 3 : 2;
    float y = pad_min.y + line_h + 2.0f;
    std::string remaining = line;
    for (int i = 0; i < max_lines && y + line_h <= pad_max.y + 1.0f; ++i) {
        if (remaining.empty()) break;
        std::string chunk = ellipsize_fit(remaining, max_w);
        draw->AddText(ImVec2(pad_min.x, y), line_col, chunk.c_str());
        if (chunk.size() >= remaining.size()) break;
        if (chunk.size() > 3 && chunk.compare(chunk.size() - 3, 3, "...") == 0)
            remaining = remaining.substr((std::min)(chunk.size() - 3, remaining.size()));
        else
            remaining = remaining.substr((std::min)(chunk.size(), remaining.size()));
        while (!remaining.empty() && (remaining.front() == ' ' || remaining.front() == '\n')) remaining.erase(remaining.begin());
        y += line_h;
    }
}

void draw_dialogue_graph_toolbar(WorldForgeEditorSession& session, WorldForgeDialogueTree& tree) {
    WorldForgeDialogueNode* selected = nullptr;
    for (auto& n : tree.nodes) {
        if (n.id == session.dialogue_selected_node_id) {
            selected = &n;
            break;
        }
    }

    if (ImGui::SmallButton("New Node##DlgTb")) {
        push_dialogue_undo(session, tree);
        const auto near_id = session.dialogue_selected_node_id;
        const auto id = unique_dialogue_node_id(tree, "node");
        if (add_dialogue_node(tree, id, "narrator", "")) {
            session.dirty = true;
            place_dialogue_node_incremental(session, tree, id, near_id);
            select_dialogue_node(session, id, true);
            session.dialogue_graph_zoom_to_selected = true;
            session.status = "Added node " + id;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected);
    if (ImGui::SmallButton("New Choice##DlgTb")) {
        push_dialogue_undo(session, tree);
        const auto choice_id = unique_dialogue_choice_id(tree, selected->id + "_c");
        if (add_dialogue_choice(tree, selected->id, choice_id, "Choice", "")) {
            session.dirty = true;
            session.status = "Added choice " + choice_id;
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Delete##DlgTb")) {
        push_dialogue_undo(session, tree);
        const auto id = selected->id;
        if (remove_dialogue_node(tree, id)) {
            session.dirty = true;
            session.dialogue_selected_node_id.clear();
            session.dialogue_graph_positions.erase(id);
            session.status = "Removed node " + id;
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Duplicate##DlgTb")) {
        push_dialogue_undo(session, tree);
        const auto near_id = selected->id;
        if (const auto dup = duplicate_dialogue_node(tree, selected->id)) {
            session.dirty = true;
            place_dialogue_node_incremental(session, tree, dup.value(), near_id);
            select_dialogue_node(session, dup.value(), true);
            session.dialogue_graph_zoom_to_selected = true;
            session.status = "Duplicated " + dup.value();
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::SmallButton("Auto Layout##DlgTb")) {
        layout_dialogue_graph(tree, session.dialogue_graph_positions, false);
        session.dialogue_graph_fit_requested = true;
        session.status = "Auto layout applied";
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Validate##DlgTb")) {
        WorldForgeDialoguesAsset probe;
        probe.schema_version = session.dialogues.schema_version;
        probe.id = session.dialogues.id;
        probe.trees = {tree};
        if (const auto ok = probe.validate()) session.status = "Validate OK";
        else session.status = "Validate: " + ok.error().message;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(true);
    ImGui::SmallButton("Preview##DlgTb");
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Preview arrives in TICKET-0177");
    ImGui::SameLine();
    if (ImGui::SmallButton("Search##DlgTb")) session.dialogue_search_focus_requested = true;
    ImGui::SameLine();
    if (ImGui::SmallButton("Frame##DlgTb")) session.dialogue_graph_fit_requested = true;
    ImGui::SameLine();
    if (ImGui::SmallButton("Zoom Sel##DlgTb")) session.dialogue_graph_zoom_to_selected = true;
    ImGui::SameLine();
    ImGui::BeginDisabled(session.dialogue_undo_stack.empty());
    if (ImGui::SmallButton("Undo##DlgTb")) {
        if (apply_dialogue_undo(session, tree)) session.status = "Undo";
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(session.dialogue_redo_stack.empty());
    if (ImGui::SmallButton("Redo##DlgTb")) {
        if (apply_dialogue_redo(session, tree)) session.status = "Redo";
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(session.dialogue_nav_history_index <= 0);
    if (ImGui::SmallButton("<<##DlgNavBack")) {
        session.dialogue_nav_suppress = true;
        --session.dialogue_nav_history_index;
        session.dialogue_selected_node_id =
            session.dialogue_nav_history[static_cast<std::size_t>(session.dialogue_nav_history_index)];
        session.dialogue_graph_zoom_to_selected = true;
        session.dialogue_nav_suppress = false;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(session.dialogue_nav_history_index < 0 ||
                         session.dialogue_nav_history_index + 1 >= static_cast<int>(session.dialogue_nav_history.size()));
    if (ImGui::SmallButton(">>##DlgNavFwd")) {
        session.dialogue_nav_suppress = true;
        ++session.dialogue_nav_history_index;
        session.dialogue_selected_node_id =
            session.dialogue_nav_history[static_cast<std::size_t>(session.dialogue_nav_history_index)];
        session.dialogue_graph_zoom_to_selected = true;
        session.dialogue_nav_suppress = false;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(session.dialogue_selected_node_id.empty());
    const bool bookmarked = session.dialogue_bookmarks.count(session.dialogue_selected_node_id) > 0;
    if (ImGui::SmallButton(bookmarked ? "Unbookmark##DlgBm" : "Bookmark##DlgBm")) {
        if (bookmarked) session.dialogue_bookmarks.erase(session.dialogue_selected_node_id);
        else session.dialogue_bookmarks.insert(session.dialogue_selected_node_id);
    }
    ImGui::EndDisabled();

    ImGui::SetNextItemWidth(90.0f);
    int mode = static_cast<int>(session.dialogue_node_display_mode);
    if (ImGui::Combo("##DlgDisplayMode", &mode, "Compact\0Standard\0Expanded\0"))
        session.dialogue_node_display_mode = static_cast<DialogueGraphNodeDisplayMode>(mode);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180.0f);
    if (session.dialogue_search_focus_requested) {
        ImGui::SetKeyboardFocusHere();
        session.dialogue_search_focus_requested = false;
    }
    ImGui::InputTextWithHint("##DlgSearch", "Search speaker/line/id/flag", session.dialogue_search_text.data(),
        session.dialogue_search_text.size());
    const auto matches = dialogue_search_node_ids(tree, trim(session.dialogue_search_text.data()));
    if (!trim(session.dialogue_search_text.data()).empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%zu hit(s)", matches.size());
        const float hit_h =
            (std::min)(4.0f, static_cast<float>(matches.size()) + 0.5f) * ImGui::GetTextLineHeightWithSpacing();
        if (ImGui::BeginListBox("##DlgSearchHits", ImVec2(-1.0f, hit_h))) {
            for (const auto& id : matches) {
                if (ImGui::Selectable(id.c_str(), id == session.dialogue_selected_node_id)) {
                    select_dialogue_node(session, id, true);
                    session.dialogue_graph_zoom_to_selected = true;
                }
            }
            ImGui::EndListBox();
        }
    }
    if (!session.dialogue_bookmarks.empty()) {
        ImGui::TextDisabled("Bookmarks:");
        ImGui::SameLine();
        for (const auto& id : session.dialogue_bookmarks) {
            if (ImGui::SmallButton(id.c_str())) {
                select_dialogue_node(session, id, true);
                session.dialogue_graph_zoom_to_selected = true;
            }
            ImGui::SameLine();
        }
        ImGui::NewLine();
    }
    if (!session.dialogue_link_from.empty()) {
        if (ImGui::SmallButton("Cancel link##WorldForgeDlgCancelLink")) session.dialogue_link_from.clear();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.55f, 0.75f, 1.0f, 1.0f), "Click target to link from %s",
            session.dialogue_link_from.c_str());
    }
}

void draw_dialogue_graph_canvas(WorldForgeEditorSession& session, WorldForgeDialogueTree& tree, const ImVec2& size) {
    ImGui::BeginChild("WorldForgeDialogueGraphCanvas", size, true, ImGuiWindowFlags_NoScrollbar);

    draw_dialogue_graph_toolbar(session, tree);

    const ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    const ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    if (canvas_size.x < 32.0f || canvas_size.y < 32.0f) {
        ImGui::EndChild();
        return;
    }
    ImGui::InvisibleButton("WorldForgeDialogueGraphHit", canvas_size);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const bool canvas_focused = ImGui::IsItemFocused() || hovered;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const ImVec2 local{mouse.x - canvas_pos.x, mouse.y - canvas_pos.y};
    auto& cam = session.dialogue_graph_camera;
    cam.min_zoom = 0.2f;
    cam.max_zoom = 2.0f;

    if (session.dialogue_graph_full_relayout) {
        layout_dialogue_graph(tree, session.dialogue_graph_positions, false);
        session.dialogue_graph_full_relayout = false;
    } else {
        // Fill any missing positions (new nodes) without moving existing cards.
        layout_dialogue_graph(tree, session.dialogue_graph_positions, true);
    }

    const auto bounds = compute_graph_bounds(session.dialogue_graph_positions);
    if (session.dialogue_graph_fit_requested) {
        fit_graph_camera_to_bounds(cam, canvas_size.x, canvas_size.y, bounds);
        session.dialogue_graph_fit_requested = false;
    }
    if (session.dialogue_graph_zoom_to_selected) {
        const auto it = session.dialogue_graph_positions.find(session.dialogue_selected_node_id);
        if (it != session.dialogue_graph_positions.end())
            center_graph_camera_on(cam, canvas_size.x, canvas_size.y, it->second, (std::max)(cam.zoom, 1.0f));
        session.dialogue_graph_zoom_to_selected = false;
    }

    if (hovered) apply_graph_zoom_at_local(cam, local.x, local.y, ImGui::GetIO().MouseWheel);

    const bool want_pan = hovered && (ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
                                         (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::GetIO().KeyAlt));
    if (want_pan && !session.dialogue_graph_panning) {
        session.dialogue_graph_panning = true;
        session.dialogue_graph_pan_start_mouse = {mouse.x, mouse.y};
        session.dialogue_graph_pan_start_pan = cam.pan;
        session.dialogue_graph_drag_key.clear();
    }
    if (session.dialogue_graph_panning) {
        if (want_pan) {
            cam.pan[0] = session.dialogue_graph_pan_start_pan[0] + (mouse.x - session.dialogue_graph_pan_start_mouse[0]);
            cam.pan[1] = session.dialogue_graph_pan_start_pan[1] + (mouse.y - session.dialogue_graph_pan_start_mouse[1]);
        } else {
            session.dialogue_graph_panning = false;
        }
    }

    const auto card = dialogue_node_card_size(session.dialogue_node_display_mode);
    const float node_w = card[0] * cam.zoom;
    const float node_h = card[1] * cam.zoom;
    auto to_screen = [&](const std::array<float, 2>& world) {
        const auto local_pt = graph_world_to_screen_local(cam, world[0], world[1]);
        return ImVec2(canvas_pos.x + local_pt[0], canvas_pos.y + local_pt[1]);
    };
    auto to_world = [&](const ImVec2& screen_local) { return graph_screen_to_world(cam, screen_local.x, screen_local.y); };

    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
        IM_COL32(24, 26, 30, 255));
    draw->AddRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
        IM_COL32(70, 74, 82, 255));
    draw->PushClipRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), true);

    const std::string search_q = trim(session.dialogue_search_text.data());
    std::string hovered_choice;
    float best_edge_dist = 10.0f * cam.zoom;
    for (const auto& node : tree.nodes) {
        const auto from_it = session.dialogue_graph_positions.find(node.id);
        if (from_it == session.dialogue_graph_positions.end()) continue;
        const ImVec2 a = to_screen(from_it->second);
        for (const auto& choice : node.choices) {
            ImVec2 b = a;
            bool has_target = false;
            if (!choice.next_node_id.empty()) {
                const auto to_it = session.dialogue_graph_positions.find(choice.next_node_id);
                if (to_it != session.dialogue_graph_positions.end()) {
                    b = to_screen(to_it->second);
                    has_target = true;
                }
            }
            if (!has_target) b = ImVec2(a.x + 80.0f * cam.zoom, a.y + 40.0f * cam.zoom);
            const bool has_flags = !choice.set_flags.empty();
            const ImU32 color = !has_target ? IM_COL32(180, 120, 120, 230)
                                            : (has_flags ? IM_COL32(200, 170, 90, 230) : IM_COL32(160, 175, 200, 230));
            draw->AddLine(a, b, color, 2.0f);
            const ImVec2 mid{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
            std::string label = choice.text.empty() ? choice.id : choice.text;
            if (label.size() > 24) label = label.substr(0, 21) + "...";
            label = ellipsize_fit(label, 120.0f * cam.zoom);
            draw->AddText(ImVec2(mid.x + 4.0f, mid.y - 8.0f), IM_COL32(210, 215, 225, 240), label.c_str());
            if (hovered && !session.dialogue_graph_panning) {
                const float dist = graph_point_segment_distance(mouse.x, mouse.y, a.x, a.y, b.x, b.y);
                if (dist < best_edge_dist) {
                    best_edge_dist = dist;
                    hovered_choice = node.id + "|" + choice.id + "|" + choice.next_node_id;
                }
            }
        }
    }

    std::string hovered_node;
    for (const auto& node : tree.nodes) {
        const auto it = session.dialogue_graph_positions.find(node.id);
        if (it == session.dialogue_graph_positions.end()) continue;
        const ImVec2 center = to_screen(it->second);
        if (center.x + node_w < canvas_pos.x || center.x - node_w > canvas_pos.x + canvas_size.x ||
            center.y + node_h < canvas_pos.y || center.y - node_h > canvas_pos.y + canvas_size.y)
            continue;
        const ImVec2 min{center.x - node_w * 0.5f, center.y - node_h * 0.5f};
        const ImVec2 max{center.x + node_w * 0.5f, center.y + node_h * 0.5f};
        const bool selected = session.dialogue_selected_node_id == node.id;
        const bool is_entry = tree.entry_node_id == node.id;
        const bool link_from = session.dialogue_link_from == node.id;
        const bool search_hit = !search_q.empty() && dialogue_node_matches_query(node, search_q);
        const auto kind = infer_dialogue_node_kind(node);
        ImU32 fill = kind == DialogueGraphNodeKind::End ? IM_COL32(70, 55, 75, 255) : IM_COL32(50, 70, 95, 255);
        if (is_entry) fill = IM_COL32(55, 95, 70, 255);
        if (search_hit) fill = IM_COL32(70, 90, 120, 255);
        if (selected) fill = IM_COL32(210, 150, 60, 255);
        if (link_from) fill = IM_COL32(90, 140, 210, 255);
        draw->AddRectFilled(min, max, fill, 6.0f);
        draw->AddRect(min, max, selected ? IM_COL32(255, 240, 160, 255) : IM_COL32(230, 230, 235, 255), 6.0f, 0,
            selected ? 3.0f : 1.5f);
        const float pad = 6.0f * cam.zoom;
        const std::string speaker = node.speaker_id.empty() ? "(speaker)" : node.speaker_id;
        draw_dialogue_line_preview(draw, ImVec2(min.x + pad, min.y + pad),
            ImVec2(max.x - pad, max.y - pad - 14.0f * cam.zoom), speaker, node.line, session.dialogue_node_display_mode,
            IM_COL32(255, 255, 255, 255), IM_COL32(220, 225, 235, 240));

        const float footer_y = max.y - 14.0f * cam.zoom;
        char footer[64];
        std::snprintf(footer, sizeof(footer), "%s · %zu", to_string(kind), node.choices.size());
        draw->AddText(ImVec2(min.x + pad, footer_y), IM_COL32(190, 200, 210, 230), footer);
        float icon_x = max.x - pad - 8.0f * cam.zoom;
        bool has_flags = false;
        bool has_warn = node.speaker_id.empty() || node.line.empty();
        for (const auto& c : node.choices) {
            if (!c.set_flags.empty()) has_flags = true;
            if (c.text.empty()) has_warn = true;
            if (!c.next_node_id.empty()) {
                bool found = false;
                for (const auto& n2 : tree.nodes) {
                    if (n2.id == c.next_node_id) {
                        found = true;
                        break;
                    }
                }
                if (!found) has_warn = true;
            }
        }
        if (has_flags) {
            draw->AddCircleFilled(ImVec2(icon_x, footer_y + 6.0f * cam.zoom), 3.5f * cam.zoom, IM_COL32(220, 180, 70, 255));
            icon_x -= 10.0f * cam.zoom;
        }
        if (has_warn) {
            draw->AddCircleFilled(ImVec2(icon_x, footer_y + 6.0f * cam.zoom), 3.5f * cam.zoom, IM_COL32(220, 90, 80, 255));
            icon_x -= 10.0f * cam.zoom;
        }
        if (session.dialogue_bookmarks.count(node.id)) {
            draw->AddCircleFilled(ImVec2(icon_x, footer_y + 6.0f * cam.zoom), 3.5f * cam.zoom, IM_COL32(120, 180, 255, 255));
        }

        if (hovered && !session.dialogue_graph_panning && mouse.x >= min.x && mouse.x <= max.x && mouse.y >= min.y &&
            mouse.y <= max.y)
            hovered_node = node.id;
    }

    const bool minimap_clicked = draw_world_forge_graph_minimap(draw, canvas_pos.x, canvas_pos.y, canvas_size.x,
        canvas_size.y, cam, bounds, session.dialogue_graph_positions, mouse.x, mouse.y,
        hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().KeyAlt);
    draw->PopClipRect();

    if (canvas_focused && !ImGui::GetIO().WantTextInput) {
        const auto& io = ImGui::GetIO();
        if (ImGui::IsKeyPressed(ImGuiKey_F) && !io.KeyCtrl) session.dialogue_graph_fit_requested = true;
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F)) session.dialogue_search_focus_requested = true;
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
            if (apply_dialogue_undo(session, tree)) session.status = "Undo";
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
            if (apply_dialogue_redo(session, tree)) session.status = "Redo";
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D) && !session.dialogue_selected_node_id.empty()) {
            push_dialogue_undo(session, tree);
            const auto near_id = session.dialogue_selected_node_id;
            if (const auto dup = duplicate_dialogue_node(tree, near_id)) {
                session.dirty = true;
                place_dialogue_node_incremental(session, tree, dup.value(), near_id);
                select_dialogue_node(session, dup.value(), true);
                session.dialogue_graph_zoom_to_selected = true;
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete) && !session.dialogue_selected_node_id.empty()) {
            push_dialogue_undo(session, tree);
            const auto id = session.dialogue_selected_node_id;
            if (remove_dialogue_node(tree, id)) {
                session.dirty = true;
                session.dialogue_selected_node_id.clear();
                session.dialogue_graph_positions.erase(id);
            }
        }
    }

    if (!minimap_clicked && !session.dialogue_graph_panning && active && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !ImGui::GetIO().KeyAlt) {
        if (!hovered_node.empty()) {
            if (!session.dialogue_link_from.empty() && session.dialogue_link_from != hovered_node) {
                push_dialogue_undo(session, tree);
                const auto choice_id =
                    unique_dialogue_choice_id(tree, session.dialogue_link_from + "_to_" + hovered_node);
                const std::string text = trim(session.create_dialogue_choice_text.data()).empty()
                                             ? choice_id
                                             : trim(session.create_dialogue_choice_text.data());
                if (add_dialogue_choice(tree, session.dialogue_link_from, choice_id, text, hovered_node)) {
                    session.dirty = true;
                    session.status = "Linked choice " + choice_id;
                } else {
                    session.status = "Link failed";
                }
                session.dialogue_link_from.clear();
            } else {
                session.dialogue_graph_drag_key = hovered_node;
                select_dialogue_node(session, hovered_node, true);
            }
        } else if (!hovered_choice.empty()) {
            const auto first = hovered_choice.find('|');
            const auto second = first == std::string::npos ? std::string::npos : hovered_choice.find('|', first + 1);
            if (first != std::string::npos) {
                select_dialogue_node(session, hovered_choice.substr(0, first), true);
                session.dialogue_link_from.clear();
                if (second != std::string::npos) {
                    const auto next = hovered_choice.substr(second + 1);
                    if (!next.empty() && ImGui::GetIO().KeyCtrl) {
                        select_dialogue_node(session, next, true);
                        session.dialogue_graph_zoom_to_selected = true;
                        session.status = "Jumped to " + next;
                    }
                }
            }
        } else {
            session.dialogue_graph_drag_key.clear();
            session.dialogue_link_from.clear();
        }
    }
    if (!session.dialogue_graph_panning && active && !session.dialogue_graph_drag_key.empty() &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left) && !ImGui::GetIO().KeyAlt) {
        auto it = session.dialogue_graph_positions.find(session.dialogue_graph_drag_key);
        if (it != session.dialogue_graph_positions.end()) it->second = to_world(local);
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) session.dialogue_graph_drag_key.clear();

    if (!hovered_node.empty() && hovered) ImGui::SetTooltip("%s", hovered_node.c_str());
    else if (!hovered_choice.empty() && hovered) ImGui::SetTooltip("Choice edge (Ctrl+click to jump)");

    ImGui::TextDisabled("%zu nodes · zoom %.2f  (wheel zoom, Alt+drag pan, F frame, Ctrl+F search)", tree.nodes.size(),
        cam.zoom);
    ImGui::EndChild();
}

void draw_twee_import_controls(WorldForgeEditorSession& session, const std::filesystem::path& project_root) {
    if (!ImGui::TreeNode("Import Twine (.twee)##WorldForgeTweeImport")) return;
    if (session.twee_import_path[0] == '\0') {
        const char* defaults = "../../context/story/sources/wrathful-conquest-act0.twee";
        std::snprintf(session.twee_import_path.data(), session.twee_import_path.size(), "%s", defaults);
    }
    if (session.twee_import_tree_id[0] == '\0') {
        std::snprintf(session.twee_import_tree_id.data(), session.twee_import_tree_id.size(), "%s",
            "dlg_act0_wrathful_conquest");
    }
    ImGui::InputText("tweePath##WorldForgeTweePath", session.twee_import_path.data(),
        session.twee_import_path.size());
    ImGui::InputText("treeId##WorldForgeTweeTree", session.twee_import_tree_id.data(),
        session.twee_import_tree_id.size());
    ImGui::InputText("displayName##WorldForgeTweeName", session.twee_import_display_name.data(),
        session.twee_import_display_name.size());
    {
        std::string parent = trim(session.twee_import_parent_quest.data());
        if (draw_id_combo("parentQuestId##WorldForgeTweeQuest", parent, collect_quest_ids(session.quests))) {
            std::snprintf(session.twee_import_parent_quest.data(), session.twee_import_parent_quest.size(), "%s",
                parent.c_str());
        }
    }
    if (ImGui::Button("Import Twine##WorldForgeTweeDoImport")) {
        nlohmann::json params{{"action", "import_twee"}, {"kind", "dialogues"},
            {"tweePath", trim(session.twee_import_path.data())},
            {"treeId", trim(session.twee_import_tree_id.data())}};
        const auto display = trim(session.twee_import_display_name.data());
        const auto parent = trim(session.twee_import_parent_quest.data());
        if (!display.empty()) params["displayName"] = display;
        if (!parent.empty()) params["parentQuestId"] = parent;
        const auto response = apply_world_forge_operation(project_root, params);
        if (response.exit_code == ExitCode::Success) {
            if (const auto reloaded = session.reload(project_root); !reloaded) {
                session.status = "Import ok; reload failed: " + reloaded.error().message;
            } else {
                session.selected_id = trim(session.twee_import_tree_id.data());
                session.dialogue_graph_full_relayout = true;
                session.dialogue_graph_fit_requested = true;
                session.status = response.summary;
                if (const auto found = response.metadata.find("nodeCount"); found != response.metadata.end())
                    session.status += " (" + found->second + " nodes)";
            }
        } else {
            session.status = "Twine import failed: " + response.summary;
        }
    }
    ImGui::TextDisabled("Creates or replaces treeId in dialogues.worldforge.json");
    ImGui::TreePop();
}

void draw_dialogues_pane(WorldForgeEditorSession& session, const std::filesystem::path& project_root) {
    if (session.list_kind != ListKind::Dialogues && session.list_kind != ListKind::DialogueGraph)
        session.list_kind = ListKind::Dialogues;

    if (ImGui::RadioButton("Trees##WorldForgeDlgKind", session.list_kind == ListKind::Dialogues)) {
        session.list_kind = ListKind::Dialogues;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Graph##WorldForgeDlgKind", session.list_kind == ListKind::DialogueGraph)) {
        session.list_kind = ListKind::DialogueGraph;
        session.dialogue_graph_full_relayout = true;
        session.dialogue_graph_fit_requested = true;
    }
    ImGui::Separator();
    draw_add_dialogue_tree_controls(session);
    ImGui::Separator();
    draw_twee_import_controls(session, project_root);
    ImGui::Separator();

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float tree_list_w = (std::clamp)(avail.x * 0.18f, 140.0f, 220.0f);

    ImGui::BeginChild("WorldForgeDialoguesList", ImVec2(tree_list_w, avail.y), true);
    if (session.dialogues.trees.empty()) ImGui::TextDisabled("(no trees — use Add dialogue tree)");
    for (const auto& tree : session.dialogues.trees) {
        std::string label = tree.id;
        if (!tree.display_name.empty()) label += "  (" + tree.display_name + ")";
        const bool selected = session.selected_id == tree.id;
        if (ImGui::Selectable(label.c_str(), selected)) {
            if (session.selected_id != tree.id) {
                session.selected_id = tree.id;
                session.dialogue_selected_node_id.clear();
                session.dialogue_graph_full_relayout = true;
                session.dialogue_graph_fit_requested = true;
                session.dialogue_link_from.clear();
            }
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();

    auto* tree = find_dialogue_tree(session.dialogues, session.selected_id);
    if (!tree) {
        ImGui::BeginChild("WorldForgeDialoguesEmpty", ImVec2(0.0f, avail.y), true);
        ImGui::TextDisabled("Select a dialogue tree, create one above, or Import Twine.");
        ImGui::EndChild();
        return;
    }

    if (session.list_kind == ListKind::DialogueGraph) {
        const float detail_w = (std::clamp)(avail.x * 0.28f, 220.0f, 340.0f);
        const float canvas_w = avail.x - tree_list_w - detail_w - 16.0f;
        draw_dialogue_graph_canvas(session, *tree, ImVec2(canvas_w, avail.y));
        ImGui::SameLine();
        ImGui::BeginChild("WorldForgeDialogueGraphDetail", ImVec2(0.0f, avail.y), true);
        ImGui::Text("tree: %s", tree->id.c_str());
        if (draw_input_text("displayName##dlgTree", tree->display_name)) session.dirty = true;
        if (draw_id_combo("parentQuestId##dlgTree", tree->parent_quest_id, collect_quest_ids(session.quests)))
            session.dirty = true;
        if (draw_id_combo("entryNodeId##dlgTree", tree->entry_node_id, collect_dialogue_node_ids(*tree), false))
            session.dirty = true;
        ImGui::Separator();
        if (ImGui::TreeNode("Add node##WorldForgeDlgAddNode")) {
            ImGui::InputText("title##dlgNewNode", session.create_dialogue_node_id.data(),
                session.create_dialogue_node_id.size());
            const auto title = trim(session.create_dialogue_node_id.data());
            const auto preview = title.empty() ? std::string{} : unique_dialogue_node_id(*tree, title);
            if (!preview.empty()) ImGui::TextDisabled("id: %s", preview.c_str());
            if (ImGui::Button("Create node##WorldForgeDlgCreateNode")) {
                const auto near_id = session.dialogue_selected_node_id;
                if (title.empty()) {
                    session.status = "Enter a node title";
                } else if (add_dialogue_node(*tree, preview, "narrator", "")) {
                    session.dirty = true;
                    place_dialogue_node_incremental(session, *tree, preview, near_id);
                    select_dialogue_node(session, preview, true);
                    session.dialogue_graph_zoom_to_selected = true;
                    session.status = "Added node " + preview;
                    session.create_dialogue_node_id = {};
                }
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Add choice / link##WorldForgeDlgAddChoice")) {
            ImGui::InputText("choice text##dlgChoiceText", session.create_dialogue_choice_text.data(),
                session.create_dialogue_choice_text.size());
            ImGui::BeginDisabled(session.dialogue_selected_node_id.empty());
            if (ImGui::Button("Start link from selection##WorldForgeDlgStartLink")) {
                session.dialogue_link_from = session.dialogue_selected_node_id;
            }
            ImGui::SameLine();
            if (ImGui::Button("Add end choice##WorldForgeDlgEndChoice")) {
                const auto choice_id =
                    unique_dialogue_choice_id(*tree, session.dialogue_selected_node_id + "_end");
                const std::string text = trim(session.create_dialogue_choice_text.data()).empty()
                                             ? "End"
                                             : trim(session.create_dialogue_choice_text.data());
                if (add_dialogue_choice(*tree, session.dialogue_selected_node_id, choice_id, text, "")) {
                    session.dirty = true;
                    session.status = "Added end choice " + choice_id;
                }
            }
            ImGui::EndDisabled();
            ImGui::TreePop();
        }
        ImGui::Separator();
        WorldForgeDialogueNode* selected_node = nullptr;
        for (auto& candidate : tree->nodes) {
            if (candidate.id == session.dialogue_selected_node_id) {
                selected_node = &candidate;
                break;
            }
        }
        if (!selected_node) {
            ImGui::TextDisabled("Select a node on the graph");
        } else {
            ImGui::Text("node: %s", selected_node->id.c_str());
            if (tree->entry_node_id == selected_node->id)
                ImGui::TextColored(ImVec4(0.5f, 0.85f, 0.55f, 1.0f), "entry node");
            else if (ImGui::Button("Make entry##WorldForgeDlgMakeEntry")) {
                tree->entry_node_id = selected_node->id;
                session.dirty = true;
            }
            if (draw_id_combo("speakerId##dlgNode", selected_node->speaker_id,
                    collect_speaker_ids(session.relationships), false))
                session.dirty = true;
            if (draw_text_area("line##dlgNode", selected_node->line, 160.0f, 16384)) session.dirty = true;
            ImGui::Text("choices (%zu)", selected_node->choices.size());
            for (std::size_t i = 0; i < selected_node->choices.size(); ++i) {
                auto& choice = selected_node->choices[i];
                ImGui::PushID(static_cast<int>(i));
                ImGui::Text("%s", choice.id.c_str());
                if (draw_text_area("text##dlgChoice", choice.text, 56.0f, 2048)) session.dirty = true;
                if (draw_id_combo("nextNodeId##dlgChoice", choice.next_node_id, collect_dialogue_node_ids(*tree), true,
                        "(end)"))
                    session.dirty = true;
                if (draw_csv_field("setFlags##dlgChoice", choice.set_flags)) session.dirty = true;
                if (ImGui::SmallButton("Remove choice##dlgChoice")) {
                    if (remove_dialogue_choice(*tree, selected_node->id, choice.id)) {
                        session.dirty = true;
                        ImGui::PopID();
                        break;
                    }
                }
                ImGui::PopID();
            }
            ImGui::Separator();
            if (ImGui::Button("Delete node##WorldForgeDlgDeleteNode")) {
                const auto id = selected_node->id;
                if (remove_dialogue_node(*tree, id)) {
                    session.dirty = true;
                    session.dialogue_selected_node_id.clear();
                    session.dialogue_graph_positions.erase(id);
                    session.status = "Removed node " + id;
                } else {
                    session.status = "Could not remove node";
                }
            }
        }
        ImGui::EndChild();
        return;
    }

    ImGui::BeginChild("WorldForgeDialoguesDetail", ImVec2(0.0f, avail.y), true);
    ImGui::Text("id: %s", tree->id.c_str());
    if (draw_input_text("displayName", tree->display_name)) session.dirty = true;
    if (draw_enum_combo("canonStatus", tree->canon_status,
            {WorldForgeDialogueCanonStatus::Established, WorldForgeDialogueCanonStatus::Draft,
                WorldForgeDialogueCanonStatus::Proposal, WorldForgeDialogueCanonStatus::Open}))
        session.dirty = true;
    if (draw_id_combo("parentQuestId", tree->parent_quest_id, collect_quest_ids(session.quests))) session.dirty = true;
    if (draw_text_area("summary", tree->summary, 96.0f)) session.dirty = true;
    if (draw_input_text("storyRef", tree->story_ref)) session.dirty = true;
    if (draw_id_combo("entryNodeId", tree->entry_node_id, collect_dialogue_node_ids(*tree), false)) session.dirty = true;
    ImGui::Text("nodes: %zu — switch to Graph for canvas editing", tree->nodes.size());
    ImGui::Separator();
    ImGui::BeginChild("WorldForgeDialogueNodePreview", ImVec2(0.0f, 220.0f), true);
    for (const auto& node : tree->nodes) {
        ImGui::TextWrapped("[%s] %s — %zu choice(s)", node.id.c_str(), node.speaker_id.c_str(),
            node.choices.size());
    }
    ImGui::EndChild();
    if (draw_csv_field("tags (comma-separated)", tree->tags)) session.dirty = true;
    if (draw_open_questions_field(tree->open_questions)) session.dirty = true;
    ImGui::EndChild();
}

void draw_world_forge_toolbar(WorldForgeEditorSession& session, const std::filesystem::path& project_root) {
    if (ImGui::Button("Reload##WorldForge")) {
        if (const auto result = session.reload(project_root); !result)
            session.status = "Reload failed: " + result.error().message;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!session.dirty);
    if (ImGui::Button("Save##WorldForge")) {
        if (const auto result = session.save(project_root); !result)
            session.status = "Save failed: " + result.error().message;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextUnformatted(session.status.c_str());
    if (session.dirty) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.35f, 1.0f), "unsaved");
    }
}

void draw_world_forge_pane_tabs(WorldForgeEditorSession& session) {
    if (ImGui::BeginTabBar("WorldForgePaneTabs")) {
        if (ImGui::BeginTabItem("Factions##WorldForgePaneFactions")) {
            if (session.pane != WorldForgeEditorPane::Factions) {
                session.pane = WorldForgeEditorPane::Factions;
                session.list_kind = ListKind::Entities;
                session.selected_id.clear();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Relationships##WorldForgePaneRelationships")) {
            if (session.pane != WorldForgeEditorPane::Relationships) {
                session.pane = WorldForgeEditorPane::Relationships;
                session.list_kind = ListKind::Nodes;
                session.selected_id.clear();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Map##WorldForgePaneMap")) {
            if (session.pane != WorldForgeEditorPane::Map) {
                session.pane = WorldForgeEditorPane::Map;
                session.list_kind = ListKind::Regions;
                session.selected_id.clear();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Quests##WorldForgePaneQuests")) {
            if (session.pane != WorldForgeEditorPane::Quests) {
                session.pane = WorldForgeEditorPane::Quests;
                session.list_kind = ListKind::Quests;
                session.selected_id.clear();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Dialogues##WorldForgePaneDialogues")) {
            if (session.pane != WorldForgeEditorPane::Dialogues) {
                session.pane = WorldForgeEditorPane::Dialogues;
                session.list_kind = ListKind::Dialogues;
                session.selected_id.clear();
                session.dialogue_selected_node_id.clear();
                session.dialogue_link_from.clear();
                session.dialogue_graph_full_relayout = true;
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

} // namespace

Result<void> WorldForgeEditorSession::reload(const std::filesystem::path& project_root) {
    if (const auto result = reload_kind(project_root, default_world_forge_factions_path(project_root), "factions",
            "factions.worldforge.json", factions);
        !result)
        return result;
    if (const auto result =
            reload_kind(project_root, default_world_forge_relationships_path(project_root), "relationships",
                "relationships.worldforge.json", relationships);
        !result)
        return result;
    if (const auto result = reload_kind(
            project_root, default_world_forge_map_path(project_root), "map", "map.worldforge.json", map);
        !result)
        return result;
    if (const auto result = reload_kind(project_root, default_world_forge_quests_path(project_root), "quests",
            "quests.worldforge.json", quests);
        !result)
        return result;
    if (const auto result = reload_kind(project_root, default_world_forge_dialogues_path(project_root), "dialogues",
            "dialogues.worldforge.json", dialogues);
        !result)
        return result;

    bool selection_found = selected_id.empty();
    if (!selection_found) {
        switch (list_kind) {
        case ListKind::Entities: selection_found = find_faction(factions, selected_id) != nullptr; break;
        case ListKind::Nodes: selection_found = find_node(relationships, selected_id) != nullptr; break;
        case ListKind::Edges: selection_found = find_edge(relationships, selected_id) != nullptr; break;
        case ListKind::Graph:
            selection_found = find_node(relationships, selected_id) != nullptr ||
                              find_edge(relationships, selected_id) != nullptr ||
                              selected_id.rfind("faction:", 0) == 0;
            break;
        case ListKind::Regions: selection_found = find_region(map, selected_id) != nullptr; break;
        case ListKind::Pois: selection_found = find_poi(map, selected_id) != nullptr; break;
        case ListKind::Links: selection_found = find_link(map, selected_id) != nullptr; break;
        case ListKind::Quests: selection_found = find_quest(quests, selected_id) != nullptr; break;
        case ListKind::Dialogues:
        case ListKind::DialogueGraph:
            selection_found = find_dialogue_tree(dialogues, selected_id) != nullptr;
            break;
        }
    }
    if (!selection_found) selected_id.clear();

    graph_positions.clear();
    graph_needs_layout = true;
    graph_drag_key.clear();
    graph_zoom = 1.0f;
    graph_pan = {0.0f, 0.0f};
    graph_panning = false;
    dialogue_graph_positions.clear();
    dialogue_graph_full_relayout = true;
    dialogue_selected_node_id.clear();
    dialogue_link_from.clear();
    dialogue_graph_camera = {};
    dialogue_graph_camera.min_zoom = 0.2f;
    dialogue_graph_camera.max_zoom = 2.0f;
    dialogue_graph_panning = false;
    dialogue_nav_history.clear();
    dialogue_nav_history_index = -1;
    dialogue_bookmarks.clear();
    dialogue_undo_stack.clear();
    dialogue_redo_stack.clear();
    dialogue_search_text = {};
    loaded = true;
    dirty = false;
    status = "Reloaded " + std::to_string(factions.entities.size()) + " factions, " +
        std::to_string(relationships.nodes.size()) + " nodes, " + std::to_string(relationships.edges.size()) +
        " edges, " + std::to_string(map.regions.size()) + " regions, " + std::to_string(map.pois.size()) +
        " pois, " + std::to_string(map.links.size()) + " links, " + std::to_string(quests.quests.size()) +
        " quests, " + std::to_string(dialogues.trees.size()) + " dialogue trees";
    return Result<void>::success();
}

Result<void> WorldForgeEditorSession::save(const std::filesystem::path& project_root) {
    if (!dirty) return Result<void>::success();

    std::vector<std::string> failures;
    if (const auto result = apply_kind(project_root, "factions", factions); !result)
        failures.push_back("factions: " + result.error().message);
    if (const auto result = apply_kind(project_root, "relationships", relationships); !result)
        failures.push_back("relationships: " + result.error().message);
    if (const auto result = apply_kind(project_root, "map", map); !result)
        failures.push_back("map: " + result.error().message);
    if (const auto result = apply_kind(project_root, "quests", quests); !result)
        failures.push_back("quests: " + result.error().message);
    if (const auto result = apply_kind(project_root, "dialogues", dialogues); !result)
        failures.push_back("dialogues: " + result.error().message);

    if (!failures.empty()) {
        std::string joined;
        for (std::size_t i = 0; i < failures.size(); ++i) {
            if (i) joined += "; ";
            joined += failures[i];
        }
        return Result<void>::failure(
            editor_error("WORLD-FORGE-EDITOR-SAVE", "World Forge save failed: " + joined,
                "Fix the reported validation errors and retry Save."));
    }

    dirty = false;
    status = "Saved factions/relationships/map/quests/dialogues";
    return Result<void>::success();
}

void draw_world_forge_viewport(WorldForgeEditorSession& session, const std::filesystem::path& project_root) {
    if (!session.loaded && !project_root.empty()) {
        if (const auto result = session.reload(project_root); !result)
            session.status = "Reload failed: " + result.error().message;
    }

    draw_world_forge_toolbar(session, project_root);
    ImGui::Separator();
    draw_world_forge_pane_tabs(session);

    switch (session.pane) {
    case WorldForgeEditorPane::Factions: draw_factions_pane(session); break;
    case WorldForgeEditorPane::Relationships: draw_relationships_pane(session); break;
    case WorldForgeEditorPane::Map: draw_map_pane(session); break;
    case WorldForgeEditorPane::Quests: draw_quests_pane(session); break;
    case WorldForgeEditorPane::Dialogues: draw_dialogues_pane(session, project_root); break;
    }
}

} // namespace engine
