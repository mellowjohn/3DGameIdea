#include "engine/assets/world_forge_factions_asset.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace engine {
namespace {

EngineError faction_error(std::string code, ErrorCategory category, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, category, "world_forge_factions", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

Result<WorldForgeFactionKind> parse_kind(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "faction") return Result<WorldForgeFactionKind>::success(WorldForgeFactionKind::Faction);
    if (key == "culture") return Result<WorldForgeFactionKind>::success(WorldForgeFactionKind::Culture);
    if (key == "clan") return Result<WorldForgeFactionKind>::success(WorldForgeFactionKind::Clan);
    if (key == "warband") return Result<WorldForgeFactionKind>::success(WorldForgeFactionKind::Warband);
    return Result<WorldForgeFactionKind>::failure(faction_error("WORLD-FORGE-FACTION-KIND", ErrorCategory::Validation,
        "Unsupported World Forge faction kind: " + raw, "Use faction, culture, clan, or warband."));
}

Result<WorldForgeCanonStatus> parse_canon_status(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "established") return Result<WorldForgeCanonStatus>::success(WorldForgeCanonStatus::Established);
    if (key == "draft") return Result<WorldForgeCanonStatus>::success(WorldForgeCanonStatus::Draft);
    if (key == "proposal") return Result<WorldForgeCanonStatus>::success(WorldForgeCanonStatus::Proposal);
    if (key == "open") return Result<WorldForgeCanonStatus>::success(WorldForgeCanonStatus::Open);
    return Result<WorldForgeCanonStatus>::failure(faction_error("WORLD-FORGE-FACTION-CANON", ErrorCategory::Validation,
        "Unsupported canonStatus: " + raw, "Use established, draft, proposal, or open."));
}

Result<WorldForgePoliticalRole> parse_political_role(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "arena") return Result<WorldForgePoliticalRole>::success(WorldForgePoliticalRole::Arena);
    if (key == "faction") return Result<WorldForgePoliticalRole>::success(WorldForgePoliticalRole::Faction);
    if (key == "unknown") return Result<WorldForgePoliticalRole>::success(WorldForgePoliticalRole::Unknown);
    return Result<WorldForgePoliticalRole>::failure(
        faction_error("WORLD-FORGE-FACTION-POLITICAL-ROLE", ErrorCategory::Validation,
            "Unsupported politicalRole: " + raw, "Use arena, faction, or unknown."));
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

} // namespace

const char* to_string(WorldForgeFactionKind value) noexcept {
    switch (value) {
    case WorldForgeFactionKind::Faction: return "faction";
    case WorldForgeFactionKind::Culture: return "culture";
    case WorldForgeFactionKind::Clan: return "clan";
    case WorldForgeFactionKind::Warband: return "warband";
    }
    return "faction";
}

const char* to_string(WorldForgeCanonStatus value) noexcept {
    switch (value) {
    case WorldForgeCanonStatus::Established: return "established";
    case WorldForgeCanonStatus::Draft: return "draft";
    case WorldForgeCanonStatus::Proposal: return "proposal";
    case WorldForgeCanonStatus::Open: return "open";
    }
    return "draft";
}

const char* to_string(WorldForgePoliticalRole value) noexcept {
    switch (value) {
    case WorldForgePoliticalRole::Arena: return "arena";
    case WorldForgePoliticalRole::Faction: return "faction";
    case WorldForgePoliticalRole::Unknown: return "unknown";
    }
    return "unknown";
}

std::filesystem::path default_world_forge_factions_path(const std::filesystem::path& project_root) {
    return project_root / "assets" / "world-forge" / "factions.worldforge.json";
}

Result<void> WorldForgeFactionsAsset::validate() const {
    if (schema_version != 1) {
        return Result<void>::failure(faction_error("WORLD-FORGE-FACTION-SCHEMA", ErrorCategory::Validation,
            "Only World Forge factions schemaVersion 1 is supported", "Use schemaVersion 1."));
    }
    std::unordered_set<std::string> ids;
    ids.reserve(entities.size());
    for (const auto& entity : entities) {
        if (entity.id.empty()) {
            return Result<void>::failure(faction_error("WORLD-FORGE-FACTION-ID", ErrorCategory::Validation,
                "Faction entity id is required", "Set a unique non-empty id for each entity."));
        }
        if (!ids.insert(entity.id).second) {
            return Result<void>::failure(faction_error("WORLD-FORGE-FACTION-ID-DUP", ErrorCategory::Validation,
                "Duplicate faction entity id: " + entity.id, "Ensure every entity id is unique."));
        }
    }
    for (const auto& entity : entities) {
        if (entity.parent_id.empty()) continue;
        if (entity.parent_id == entity.id) {
            return Result<void>::failure(faction_error("WORLD-FORGE-FACTION-PARENT", ErrorCategory::Validation,
                "Faction entity parentId cannot reference itself: " + entity.id,
                "Point parentId at a different entity or leave it empty."));
        }
        if (ids.find(entity.parent_id) == ids.end()) {
            return Result<void>::failure(faction_error("WORLD-FORGE-FACTION-PARENT", ErrorCategory::Validation,
                "Unknown parentId '" + entity.parent_id + "' on entity '" + entity.id + "'",
                "parentId must match another entity id in the same file, or be empty."));
        }
    }
    for (const auto& entity : entities) {
        if (!entity.standing) continue;
        const auto& standing = *entity.standing;
        if (standing.min_score > standing.max_score) {
            return Result<void>::failure(faction_error("WORLD-FORGE-FACTION-STANDING-RANGE", ErrorCategory::Validation,
                "standing.min must be <= standing.max on entity '" + entity.id + "'",
                "Fix min/max score range."));
        }
        std::unordered_set<std::string> rank_ids;
        double previous_min = -std::numeric_limits<double>::infinity();
        for (const auto& rank : standing.ranks) {
            if (rank.id.empty()) {
                return Result<void>::failure(faction_error("WORLD-FORGE-FACTION-STANDING-RANK", ErrorCategory::Validation,
                    "standing.ranks[].id is required on entity '" + entity.id + "'", "Set a non-empty rank id."));
            }
            if (!rank_ids.insert(rank.id).second) {
                return Result<void>::failure(faction_error("WORLD-FORGE-FACTION-STANDING-RANK-DUP",
                    ErrorCategory::Validation, "Duplicate standing rank id '" + rank.id + "' on '" + entity.id + "'",
                    "Ensure rank ids are unique per faction."));
            }
            if (rank.min_score + 1e-9 < previous_min) {
                return Result<void>::failure(faction_error("WORLD-FORGE-FACTION-STANDING-RANK-ORDER",
                    ErrorCategory::Validation,
                    "standing.ranks must be ordered by non-decreasing minScore on '" + entity.id + "'",
                    "Sort ranks by minScore ascending."));
            }
            previous_min = rank.min_score;
        }
        if (standing.lock_in) {
            for (const auto& exclusive : standing.lock_in->exclusive_faction_ids) {
                if (exclusive.empty()) {
                    return Result<void>::failure(faction_error("WORLD-FORGE-FACTION-STANDING-LOCK",
                        ErrorCategory::Validation, "lockIn.exclusiveFactionIds cannot contain empty ids on '" +
                            entity.id + "'",
                        "Remove empty entries."));
                }
                if (exclusive == entity.id) {
                    return Result<void>::failure(faction_error("WORLD-FORGE-FACTION-STANDING-LOCK",
                        ErrorCategory::Validation, "lockIn.exclusiveFactionIds cannot include self on '" + entity.id + "'",
                        "List opposing factions only."));
                }
                if (ids.find(exclusive) == ids.end()) {
                    return Result<void>::failure(faction_error("WORLD-FORGE-FACTION-STANDING-LOCK",
                        ErrorCategory::Validation,
                        "Unknown exclusiveFactionId '" + exclusive + "' on '" + entity.id + "'",
                        "exclusiveFactionIds must reference entities in this file."));
                }
            }
        }
    }
    return Result<void>::success();
}

const WorldForgeFactionEntity* WorldForgeFactionsAsset::find_entity(const std::string& entity_id) const {
    for (const auto& entity : entities) {
        if (entity.id == entity_id) return &entity;
    }
    return nullptr;
}

WorldForgeFactionEntity* WorldForgeFactionsAsset::find_entity(const std::string& entity_id) {
    for (auto& entity : entities) {
        if (entity.id == entity_id) return &entity;
    }
    return nullptr;
}

Result<WorldForgeFactionsAsset> WorldForgeFactionsAsset::parse(const std::string& text, const std::string& source_name) {
    try {
        const auto json = nlohmann::json::parse(text);
        if (!json.is_object()) {
            return Result<WorldForgeFactionsAsset>::failure(faction_error("WORLD-FORGE-FACTION-ROOT",
                ErrorCategory::Serialization, source_name + " must be a JSON object",
                "Wrap entities in an object with schemaVersion and id."));
        }
        WorldForgeFactionsAsset asset;
        asset.schema_version = json.value("schemaVersion", 0);
        if (asset.schema_version != 1) {
            return Result<WorldForgeFactionsAsset>::failure(faction_error("WORLD-FORGE-FACTION-SCHEMA",
                ErrorCategory::Validation, "Unsupported World Forge factions schemaVersion",
                "Use schemaVersion 1."));
        }
        asset.id = json.value("id", std::string{});
        const auto entities = json.value("entities", nlohmann::json::array());
        if (!entities.is_array()) {
            return Result<WorldForgeFactionsAsset>::failure(faction_error("WORLD-FORGE-FACTION-ENTITIES",
                ErrorCategory::Validation, "entities must be an array", "Provide an entities array."));
        }
        for (const auto& node : entities) {
            if (!node.is_object()) {
                return Result<WorldForgeFactionsAsset>::failure(faction_error("WORLD-FORGE-FACTION-ENTITY",
                    ErrorCategory::Validation, "Each entity must be an object", "Fix entity entries."));
            }
            WorldForgeFactionEntity entity;
            entity.id = node.value("id", std::string{});
            if (entity.id.empty()) {
                return Result<WorldForgeFactionsAsset>::failure(faction_error("WORLD-FORGE-FACTION-ID",
                    ErrorCategory::Validation, "Faction entity id is required",
                    "Set a unique non-empty id for each entity."));
            }
            const auto kind = parse_kind(node.value("kind", std::string{}));
            if (!kind) return Result<WorldForgeFactionsAsset>::failure(kind.error());
            entity.kind = kind.value();
            entity.display_name = node.value("displayName", std::string{});
            const auto canon = parse_canon_status(node.value("canonStatus", std::string{}));
            if (!canon) return Result<WorldForgeFactionsAsset>::failure(canon.error());
            entity.canon_status = canon.value();
            entity.summary = node.value("summary", std::string{});
            entity.story_ref = node.value("storyRef", std::string{});
            entity.tags = read_string_array(node.value("tags", nlohmann::json::array()));
            if (node.contains("politicalRole") && !node["politicalRole"].is_null()) {
                const auto role = parse_political_role(node["politicalRole"].get<std::string>());
                if (!role) return Result<WorldForgeFactionsAsset>::failure(role.error());
                entity.political_role = role.value();
            }
            entity.parent_id = node.value("parentId", std::string{});
            entity.open_questions = read_string_array(node.value("openQuestions", nlohmann::json::array()));
            if (node.contains("standing") && node["standing"].is_object()) {
                const auto& standing_node = node["standing"];
                WorldForgeFactionStandingConfig standing;
                standing.tracks_player = standing_node.value("tracksPlayer", false);
                standing.min_score = standing_node.value("min", -100.0);
                standing.max_score = standing_node.value("max", 100.0);
                const auto ranks = standing_node.value("ranks", nlohmann::json::array());
                if (!ranks.is_array()) {
                    return Result<WorldForgeFactionsAsset>::failure(faction_error("WORLD-FORGE-FACTION-STANDING-RANK",
                        ErrorCategory::Validation, "standing.ranks must be an array on '" + entity.id + "'",
                        "Provide a ranks array (may be empty)."));
                }
                for (const auto& rank_node : ranks) {
                    if (!rank_node.is_object()) {
                        return Result<WorldForgeFactionsAsset>::failure(faction_error(
                            "WORLD-FORGE-FACTION-STANDING-RANK", ErrorCategory::Validation,
                            "Each standing rank must be an object", "Fix ranks entries."));
                    }
                    WorldForgeFactionStandingRank rank;
                    rank.id = rank_node.value("id", std::string{});
                    rank.min_score = rank_node.value("minScore", 0.0);
                    rank.display_name = rank_node.value("displayName", std::string{});
                    standing.ranks.push_back(std::move(rank));
                }
                if (standing_node.contains("lockIn") && standing_node["lockIn"].is_object()) {
                    const auto& lock_node = standing_node["lockIn"];
                    WorldForgeFactionStandingLockIn lock_in;
                    lock_in.threshold = lock_node.value("threshold", 0.0);
                    lock_in.exclusive_faction_ids =
                        read_string_array(lock_node.value("exclusiveFactionIds", nlohmann::json::array()));
                    standing.lock_in = std::move(lock_in);
                }
                entity.standing = std::move(standing);
            }
            asset.entities.push_back(std::move(entity));
        }
        if (const auto valid = asset.validate(); !valid) {
            return Result<WorldForgeFactionsAsset>::failure(valid.error());
        }
        return Result<WorldForgeFactionsAsset>::success(std::move(asset));
    } catch (const std::exception& exception) {
        auto error = faction_error("WORLD-FORGE-FACTION-PARSE", ErrorCategory::Serialization,
            "Failed to parse " + source_name, "Fix JSON syntax.");
        error.causes.push_back(exception.what());
        return Result<WorldForgeFactionsAsset>::failure(std::move(error));
    }
}

Result<WorldForgeFactionsAsset> WorldForgeFactionsAsset::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Result<WorldForgeFactionsAsset>::failure(faction_error("WORLD-FORGE-FACTION-READ", ErrorCategory::Io,
            "Could not read World Forge factions: " + path.generic_string(),
            "Check the path and file permissions."));
    }
    std::ostringstream text;
    text << input.rdbuf();
    return parse(text.str(), path.filename().string());
}

std::string WorldForgeFactionsAsset::to_json() const {
    nlohmann::ordered_json json;
    json["schemaVersion"] = schema_version;
    json["id"] = id;
    auto entities_json = nlohmann::ordered_json::array();
    for (const auto& entity : this->entities) {
        nlohmann::ordered_json node;
        node["id"] = entity.id;
        node["kind"] = to_string(entity.kind);
        node["displayName"] = entity.display_name;
        node["canonStatus"] = to_string(entity.canon_status);
        node["summary"] = entity.summary;
        node["storyRef"] = entity.story_ref;
        node["tags"] = write_string_array(entity.tags);
        if (entity.political_role) node["politicalRole"] = to_string(*entity.political_role);
        node["parentId"] = entity.parent_id;
        node["openQuestions"] = write_string_array(entity.open_questions);
        if (entity.standing) {
            nlohmann::ordered_json standing_json;
            standing_json["tracksPlayer"] = entity.standing->tracks_player;
            standing_json["min"] = entity.standing->min_score;
            standing_json["max"] = entity.standing->max_score;
            auto ranks_json = nlohmann::ordered_json::array();
            for (const auto& rank : entity.standing->ranks) {
                ranks_json.push_back(nlohmann::ordered_json{{"id", rank.id}, {"minScore", rank.min_score},
                    {"displayName", rank.display_name}});
            }
            standing_json["ranks"] = std::move(ranks_json);
            if (entity.standing->lock_in) {
                standing_json["lockIn"] = nlohmann::ordered_json{
                    {"threshold", entity.standing->lock_in->threshold},
                    {"exclusiveFactionIds", write_string_array(entity.standing->lock_in->exclusive_faction_ids)}};
            }
            node["standing"] = std::move(standing_json);
        }
        entities_json.push_back(std::move(node));
    }
    json["entities"] = std::move(entities_json);
    return json.dump(2) + "\n";
}

Result<void> WorldForgeFactionsAsset::save_atomic(const std::filesystem::path& path) const {
    const auto valid = validate();
    if (!valid) return Result<void>::failure(valid.error());
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    const auto temporary = path.string() + ".tmp";
    const auto backup = path.string() + ".bak";
    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) {
            return Result<void>::failure(faction_error("WORLD-FORGE-FACTION-IO", ErrorCategory::Io,
                "Could not write World Forge factions: " + path.generic_string(),
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
        return Result<void>::failure(faction_error("WORLD-FORGE-FACTION-IO", ErrorCategory::Io,
            "Could not replace World Forge factions: " + path.generic_string(),
            "Check file permissions and disk space."));
    }
    if (std::filesystem::exists(backup)) std::filesystem::remove(backup, ignored);
    return Result<void>::success();
}

Result<void> WorldForgeFactionsAsset::validate_file(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return Result<void>::success();
    const auto loaded = load(path);
    if (!loaded) return Result<void>::failure(loaded.error());
    return Result<void>::success();
}

} // namespace engine
