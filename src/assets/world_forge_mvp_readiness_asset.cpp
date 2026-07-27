#include "engine/assets/world_forge_mvp_readiness_asset.h"
#include "engine/assets/world_forge_acts.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace engine {
namespace {

EngineError mvp_error(std::string code, ErrorCategory category, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, category, "world_forge_mvp_readiness", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

Result<WorldForgeMvpItemStatus> parse_status(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "todo") return Result<WorldForgeMvpItemStatus>::success(WorldForgeMvpItemStatus::Todo);
    if (key == "wip") return Result<WorldForgeMvpItemStatus>::success(WorldForgeMvpItemStatus::Wip);
    if (key == "done") return Result<WorldForgeMvpItemStatus>::success(WorldForgeMvpItemStatus::Done);
    if (key == "blocked") return Result<WorldForgeMvpItemStatus>::success(WorldForgeMvpItemStatus::Blocked);
    return Result<WorldForgeMvpItemStatus>::failure(mvp_error("WORLD-FORGE-MVP-STATUS", ErrorCategory::Validation,
        "Unsupported MVP item status: " + raw, "Use todo, wip, done, or blocked."));
}

Result<WorldForgeMvpItemPriority> parse_priority(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "p0" || key == "0" || key == "critical" || key == "now")
        return Result<WorldForgeMvpItemPriority>::success(WorldForgeMvpItemPriority::P0);
    if (key == "p1" || key == "1" || key == "high" || key == "next")
        return Result<WorldForgeMvpItemPriority>::success(WorldForgeMvpItemPriority::P1);
    if (key == "p2" || key == "2" || key == "later" || key == "low")
        return Result<WorldForgeMvpItemPriority>::success(WorldForgeMvpItemPriority::P2);
    return Result<WorldForgeMvpItemPriority>::failure(mvp_error("WORLD-FORGE-MVP-PRIORITY",
        ErrorCategory::Validation, "Unsupported MVP item priority: " + raw, "Use p0, p1, or p2."));
}

Result<WorldForgeMvpWorkstream> parse_workstream(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "art") return Result<WorldForgeMvpWorkstream>::success(WorldForgeMvpWorkstream::Art);
    if (key == "effects") return Result<WorldForgeMvpWorkstream>::success(WorldForgeMvpWorkstream::Effects);
    if (key == "coding") return Result<WorldForgeMvpWorkstream>::success(WorldForgeMvpWorkstream::Coding);
    if (key == "project" || key == "project_management" || key == "pm")
        return Result<WorldForgeMvpWorkstream>::success(WorldForgeMvpWorkstream::Project);
    if (key == "storyline" || key == "story")
        return Result<WorldForgeMvpWorkstream>::success(WorldForgeMvpWorkstream::Storyline);
    if (key == "gameplay") return Result<WorldForgeMvpWorkstream>::success(WorldForgeMvpWorkstream::Gameplay);
    if (key == "combat" || key == "combat_mechanics")
        return Result<WorldForgeMvpWorkstream>::success(WorldForgeMvpWorkstream::Combat);
    if (key == "archetype" || key == "archetypes")
        return Result<WorldForgeMvpWorkstream>::success(WorldForgeMvpWorkstream::Archetype);
    if (key == "cinematics" || key == "cinematic" || key == "events" || key == "theatrical")
        return Result<WorldForgeMvpWorkstream>::success(WorldForgeMvpWorkstream::Cinematics);
    if (key == "ui_ux" || key == "uiux" || key == "ui" || key == "ux" || key == "user_interface" ||
        key == "user_experience")
        return Result<WorldForgeMvpWorkstream>::success(WorldForgeMvpWorkstream::UiUx);
    return Result<WorldForgeMvpWorkstream>::failure(mvp_error("WORLD-FORGE-MVP-WORKSTREAM",
        ErrorCategory::Validation, "Unsupported MVP workstream: " + raw,
        "Use art, effects, coding, project, storyline, gameplay, combat, archetype, cinematics, or ui_ux."));
}

std::vector<std::string> read_string_array(const nlohmann::json& node) {
    std::vector<std::string> out;
    if (!node.is_array()) return out;
    for (const auto& entry : node) {
        if (entry.is_string()) out.push_back(entry.get<std::string>());
    }
    return out;
}

nlohmann::json write_string_array(const std::vector<std::string>& values) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& value : values) out.push_back(value);
    return out;
}

WorldForgeMvpItemRefs read_refs(const nlohmann::json& node) {
    WorldForgeMvpItemRefs refs;
    if (!node.is_object()) return refs;
    refs.story_ref = node.value("storyRef", std::string{});
    refs.quest_id = node.value("questId", std::string{});
    refs.dialogue_id = node.value("dialogueId", std::string{});
    refs.asset_path = node.value("assetPath", std::string{});
    refs.ticket_id = node.value("ticketId", std::string{});
    return refs;
}

nlohmann::ordered_json write_refs(const WorldForgeMvpItemRefs& refs) {
    nlohmann::ordered_json node;
    if (!refs.story_ref.empty()) node["storyRef"] = refs.story_ref;
    if (!refs.quest_id.empty()) node["questId"] = refs.quest_id;
    if (!refs.dialogue_id.empty()) node["dialogueId"] = refs.dialogue_id;
    if (!refs.asset_path.empty()) node["assetPath"] = refs.asset_path;
    if (!refs.ticket_id.empty()) node["ticketId"] = refs.ticket_id;
    return node;
}

bool refs_empty(const WorldForgeMvpItemRefs& refs) {
    return refs.story_ref.empty() && refs.quest_id.empty() && refs.dialogue_id.empty() &&
           refs.asset_path.empty() && refs.ticket_id.empty();
}

} // namespace

const char* to_string(WorldForgeMvpItemStatus value) noexcept {
    switch (value) {
    case WorldForgeMvpItemStatus::Todo: return "todo";
    case WorldForgeMvpItemStatus::Wip: return "wip";
    case WorldForgeMvpItemStatus::Done: return "done";
    case WorldForgeMvpItemStatus::Blocked: return "blocked";
    }
    return "todo";
}

const char* to_string(WorldForgeMvpItemPriority value) noexcept {
    switch (value) {
    case WorldForgeMvpItemPriority::P0: return "p0";
    case WorldForgeMvpItemPriority::P1: return "p1";
    case WorldForgeMvpItemPriority::P2: return "p2";
    }
    return "p1";
}

const char* to_string(WorldForgeMvpWorkstream value) noexcept {
    switch (value) {
    case WorldForgeMvpWorkstream::Art: return "art";
    case WorldForgeMvpWorkstream::Effects: return "effects";
    case WorldForgeMvpWorkstream::Coding: return "coding";
    case WorldForgeMvpWorkstream::Project: return "project";
    case WorldForgeMvpWorkstream::Storyline: return "storyline";
    case WorldForgeMvpWorkstream::Gameplay: return "gameplay";
    case WorldForgeMvpWorkstream::Combat: return "combat";
    case WorldForgeMvpWorkstream::Archetype: return "archetype";
    case WorldForgeMvpWorkstream::Cinematics: return "cinematics";
    case WorldForgeMvpWorkstream::UiUx: return "ui_ux";
    }
    return "coding";
}

const char* world_forge_mvp_workstream_label(WorldForgeMvpWorkstream value) noexcept {
    switch (value) {
    case WorldForgeMvpWorkstream::Art: return "Art";
    case WorldForgeMvpWorkstream::Effects: return "Effects";
    case WorldForgeMvpWorkstream::Coding: return "Coding";
    case WorldForgeMvpWorkstream::Project: return "Project";
    case WorldForgeMvpWorkstream::Storyline: return "Storyline";
    case WorldForgeMvpWorkstream::Gameplay: return "Gameplay";
    case WorldForgeMvpWorkstream::Combat: return "Combat";
    case WorldForgeMvpWorkstream::Archetype: return "Archetype";
    case WorldForgeMvpWorkstream::Cinematics: return "Cinematics";
    case WorldForgeMvpWorkstream::UiUx: return "UI / UX";
    }
    return "Coding";
}

const char* world_forge_mvp_priority_label(WorldForgeMvpItemPriority value) noexcept {
    switch (value) {
    case WorldForgeMvpItemPriority::P0: return "P0 Now";
    case WorldForgeMvpItemPriority::P1: return "P1 Next";
    case WorldForgeMvpItemPriority::P2: return "P2 Later";
    }
    return "P1 Next";
}

std::filesystem::path default_world_forge_mvp_readiness_path(const std::filesystem::path& project_root) {
    return project_root / "assets" / "world-forge" / "act0_mvp_readiness.worldforge.json";
}

int WorldForgeMvpReadinessAsset::count_items() const {
    int total = 0;
    for (const auto& category : categories) total += static_cast<int>(category.items.size());
    return total;
}

int WorldForgeMvpReadinessAsset::count_done() const {
    int done = 0;
    for (const auto& category : categories) {
        for (const auto& item : category.items) {
            if (item.status == WorldForgeMvpItemStatus::Done) ++done;
        }
    }
    return done;
}

float WorldForgeMvpReadinessAsset::done_fraction() const {
    const int total = count_items();
    if (total <= 0) return 0.0f;
    return static_cast<float>(count_done()) / static_cast<float>(total);
}

WorldForgeMvpChecklistItem* WorldForgeMvpReadinessAsset::find_item(const std::string& item_id) {
    for (auto& category : categories) {
        for (auto& item : category.items) {
            if (item.id == item_id) return &item;
        }
    }
    return nullptr;
}

const WorldForgeMvpChecklistItem* WorldForgeMvpReadinessAsset::find_item(const std::string& item_id) const {
    for (const auto& category : categories) {
        for (const auto& item : category.items) {
            if (item.id == item_id) return &item;
        }
    }
    return nullptr;
}

Result<void> WorldForgeMvpReadinessAsset::validate() const {
    if (schema_version != 1) {
        return Result<void>::failure(mvp_error("WORLD-FORGE-MVP-SCHEMA", ErrorCategory::Validation,
            "Only World Forge MVP readiness schemaVersion 1 is supported", "Use schemaVersion 1."));
    }
    if (id.empty()) {
        return Result<void>::failure(mvp_error("WORLD-FORGE-MVP-ID", ErrorCategory::Validation,
            "MVP readiness asset id is required", "Set id (e.g. act0_mvp_readiness)."));
    }
    if (!is_world_forge_act_id(act_id)) {
        return Result<void>::failure(mvp_error("WORLD-FORGE-MVP-ACT", ErrorCategory::Validation,
            "Unsupported actId: " + act_id, "Use act0, act1, act2, act3, or act4."));
    }
    std::unordered_set<std::string> category_ids;
    std::unordered_set<std::string> item_ids;
    category_ids.reserve(categories.size());
    for (const auto& category : categories) {
        if (category.id.empty()) {
            return Result<void>::failure(mvp_error("WORLD-FORGE-MVP-CATEGORY-ID", ErrorCategory::Validation,
                "Category id is required", "Set a unique non-empty id for each category."));
        }
        if (!category_ids.insert(category.id).second) {
            return Result<void>::failure(mvp_error("WORLD-FORGE-MVP-CATEGORY-ID-DUP", ErrorCategory::Validation,
                "Duplicate category id: " + category.id, "Ensure every category id is unique."));
        }
        for (const auto& item : category.items) {
            if (item.id.empty()) {
                return Result<void>::failure(mvp_error("WORLD-FORGE-MVP-ITEM-ID", ErrorCategory::Validation,
                    "Item id is required in category '" + category.id + "'",
                    "Set a unique non-empty id for each item."));
            }
            if (!item_ids.insert(item.id).second) {
                return Result<void>::failure(mvp_error("WORLD-FORGE-MVP-ITEM-ID-DUP", ErrorCategory::Validation,
                    "Duplicate item id: " + item.id, "Ensure every checklist item id is unique across categories."));
            }
            if (item.title.empty()) {
                return Result<void>::failure(mvp_error("WORLD-FORGE-MVP-ITEM-TITLE", ErrorCategory::Validation,
                    "Item title is required for '" + item.id + "'", "Set a non-empty title."));
            }
        }
    }
    return Result<void>::success();
}

Result<WorldForgeMvpReadinessAsset> WorldForgeMvpReadinessAsset::parse(const std::string& text,
    const std::string& source_name) {
    try {
        const auto json = nlohmann::json::parse(text);
        if (!json.is_object()) {
            return Result<WorldForgeMvpReadinessAsset>::failure(mvp_error("WORLD-FORGE-MVP-ROOT",
                ErrorCategory::Serialization, source_name + " must be a JSON object",
                "Wrap categories in an object with schemaVersion, id, and actId."));
        }
        WorldForgeMvpReadinessAsset asset;
        asset.schema_version = json.value("schemaVersion", 0);
        if (asset.schema_version != 1) {
            return Result<WorldForgeMvpReadinessAsset>::failure(mvp_error("WORLD-FORGE-MVP-SCHEMA",
                ErrorCategory::Validation, "Unsupported World Forge MVP readiness schemaVersion",
                "Use schemaVersion 1."));
        }
        asset.id = json.value("id", std::string{});
        asset.act_id = json.value("actId", std::string{"act0"});
        const auto categories = json.value("categories", nlohmann::json::array());
        if (!categories.is_array()) {
            return Result<WorldForgeMvpReadinessAsset>::failure(mvp_error("WORLD-FORGE-MVP-CATEGORIES",
                ErrorCategory::Validation, "categories must be an array", "Provide a categories array."));
        }
        for (const auto& cat_node : categories) {
            if (!cat_node.is_object()) {
                return Result<WorldForgeMvpReadinessAsset>::failure(mvp_error("WORLD-FORGE-MVP-CATEGORY",
                    ErrorCategory::Validation, "Each category must be an object", "Fix category entries."));
            }
            WorldForgeMvpCategory category;
            category.id = cat_node.value("id", std::string{});
            category.title = cat_node.value("title", std::string{});
            const auto items = cat_node.value("items", nlohmann::json::array());
            if (!items.is_array()) {
                return Result<WorldForgeMvpReadinessAsset>::failure(mvp_error("WORLD-FORGE-MVP-ITEMS",
                    ErrorCategory::Validation, "items must be an array on category '" + category.id + "'",
                    "Provide an items array."));
            }
            // Default workstream from category id when item omits it.
            WorldForgeMvpWorkstream default_stream = WorldForgeMvpWorkstream::Coding;
            if (const auto parsed_cat = parse_workstream(category.id); parsed_cat) {
                default_stream = parsed_cat.value();
            }
            for (const auto& item_node : items) {
                if (!item_node.is_object()) {
                    return Result<WorldForgeMvpReadinessAsset>::failure(mvp_error("WORLD-FORGE-MVP-ITEM",
                        ErrorCategory::Validation, "Each item must be an object", "Fix item entries."));
                }
                WorldForgeMvpChecklistItem item;
                item.id = item_node.value("id", std::string{});
                item.title = item_node.value("title", std::string{});
                item.notes = item_node.value("notes", std::string{});
                const auto status = parse_status(item_node.value("status", std::string{"todo"}));
                if (!status) return Result<WorldForgeMvpReadinessAsset>::failure(status.error());
                item.status = status.value();
                const auto priority = parse_priority(item_node.value("priority", std::string{"p1"}));
                if (!priority) return Result<WorldForgeMvpReadinessAsset>::failure(priority.error());
                item.priority = priority.value();
                if (item_node.contains("workstream") && item_node["workstream"].is_string()) {
                    const auto stream = parse_workstream(item_node["workstream"].get<std::string>());
                    if (!stream) return Result<WorldForgeMvpReadinessAsset>::failure(stream.error());
                    item.workstream = stream.value();
                } else {
                    item.workstream = default_stream;
                }
                item.description = item_node.value("description", std::string{});
                item.image_paths = read_string_array(item_node.value("imagePaths", nlohmann::json::array()));
                item.depends_on = read_string_array(item_node.value("dependsOn", nlohmann::json::array()));
                item.goal = item_node.value("goal", false);
                item.refs = read_refs(item_node.value("refs", nlohmann::json::object()));
                category.items.push_back(std::move(item));
            }
            asset.categories.push_back(std::move(category));
        }
        if (const auto valid = asset.validate(); !valid) {
            return Result<WorldForgeMvpReadinessAsset>::failure(valid.error());
        }
        return Result<WorldForgeMvpReadinessAsset>::success(std::move(asset));
    } catch (const std::exception& exception) {
        auto error = mvp_error("WORLD-FORGE-MVP-PARSE", ErrorCategory::Serialization,
            "Failed to parse " + source_name, "Fix JSON syntax.");
        error.causes.push_back(exception.what());
        return Result<WorldForgeMvpReadinessAsset>::failure(std::move(error));
    }
}

Result<WorldForgeMvpReadinessAsset> WorldForgeMvpReadinessAsset::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Result<WorldForgeMvpReadinessAsset>::failure(mvp_error("WORLD-FORGE-MVP-READ", ErrorCategory::Io,
            "Could not read World Forge MVP readiness: " + path.generic_string(),
            "Check the path and file permissions."));
    }
    std::ostringstream text;
    text << input.rdbuf();
    return parse(text.str(), path.filename().string());
}

std::string WorldForgeMvpReadinessAsset::to_json() const {
    nlohmann::ordered_json json;
    json["schemaVersion"] = schema_version;
    json["id"] = id;
    json["actId"] = act_id;
    auto categories_json = nlohmann::ordered_json::array();
    for (const auto& category : categories) {
        nlohmann::ordered_json cat_node;
        cat_node["id"] = category.id;
        cat_node["title"] = category.title;
        auto items_json = nlohmann::ordered_json::array();
        for (const auto& item : category.items) {
            nlohmann::ordered_json item_node;
            item_node["id"] = item.id;
            item_node["title"] = item.title;
            item_node["status"] = to_string(item.status);
            item_node["priority"] = to_string(item.priority);
            item_node["workstream"] = to_string(item.workstream);
            if (!item.notes.empty()) item_node["notes"] = item.notes;
            if (!item.description.empty()) item_node["description"] = item.description;
            if (!item.image_paths.empty()) item_node["imagePaths"] = write_string_array(item.image_paths);
            if (!item.depends_on.empty()) item_node["dependsOn"] = write_string_array(item.depends_on);
            if (item.goal) item_node["goal"] = true;
            if (!refs_empty(item.refs)) item_node["refs"] = write_refs(item.refs);
            items_json.push_back(std::move(item_node));
        }
        cat_node["items"] = std::move(items_json);
        categories_json.push_back(std::move(cat_node));
    }
    json["categories"] = std::move(categories_json);
    return json.dump(2) + "\n";
}

Result<void> WorldForgeMvpReadinessAsset::save_atomic(const std::filesystem::path& path) const {
    const auto valid = validate();
    if (!valid) return Result<void>::failure(valid.error());
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    const auto temporary = path.string() + ".tmp";
    const auto backup = path.string() + ".bak";
    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) {
            return Result<void>::failure(mvp_error("WORLD-FORGE-MVP-IO", ErrorCategory::Io,
                "Could not write World Forge MVP readiness: " + path.generic_string(),
                "Check file permissions and disk space."));
        }
        output << to_json();
    }
    std::error_code ignored;
    if (std::filesystem::exists(path)) {
        std::filesystem::copy_file(path, backup, std::filesystem::copy_options::overwrite_existing, ignored);
    }
    std::filesystem::rename(temporary, path, ignored);
    if (ignored) {
        return Result<void>::failure(mvp_error("WORLD-FORGE-MVP-IO", ErrorCategory::Io,
            "Could not replace World Forge MVP readiness: " + path.generic_string(),
            "Check file permissions and disk space."));
    }
    if (std::filesystem::exists(backup)) std::filesystem::remove(backup, ignored);
    return Result<void>::success();
}

Result<void> WorldForgeMvpReadinessAsset::validate_file(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return Result<void>::success();
    const auto loaded = load(path);
    if (!loaded) return Result<void>::failure(loaded.error());
    return Result<void>::success();
}

} // namespace engine
