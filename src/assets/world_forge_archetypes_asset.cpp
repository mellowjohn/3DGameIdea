#include "engine/assets/world_forge_archetypes_asset.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace engine {
namespace {

EngineError archetype_error(std::string code, ErrorCategory category, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, category, "world_forge_archetypes", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

Result<WorldForgeArchetypeKind> parse_kind(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "starting") return Result<WorldForgeArchetypeKind>::success(WorldForgeArchetypeKind::Starting);
    if (key == "advanced") return Result<WorldForgeArchetypeKind>::success(WorldForgeArchetypeKind::Advanced);
    return Result<WorldForgeArchetypeKind>::failure(archetype_error("WORLD-FORGE-ARCHETYPE-KIND",
        ErrorCategory::Validation, "Unsupported World Forge archetype kind: " + raw,
        "Use starting or advanced."));
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

bool unlock_has_content(const WorldForgeArchetypeUnlock& unlock) {
    return unlock.morality_threshold.has_value() || !unlock.faction_id.empty() || !unlock.tags.empty();
}

} // namespace

const char* to_string(WorldForgeArchetypeKind value) noexcept {
    switch (value) {
    case WorldForgeArchetypeKind::Starting: return "starting";
    case WorldForgeArchetypeKind::Advanced: return "advanced";
    }
    return "starting";
}

std::filesystem::path default_world_forge_archetypes_path(const std::filesystem::path& project_root) {
    return project_root / "assets" / "world-forge" / "archetypes.worldforge.json";
}

Result<void> WorldForgeArchetypesAsset::validate() const {
    if (schema_version != 1) {
        return Result<void>::failure(archetype_error("WORLD-FORGE-ARCHETYPE-SCHEMA", ErrorCategory::Validation,
            "Only World Forge archetypes schemaVersion 1 is supported", "Use schemaVersion 1."));
    }
    std::unordered_set<std::string> ids;
    ids.reserve(entities.size());
    for (const auto& entity : entities) {
        if (entity.id.empty()) {
            return Result<void>::failure(archetype_error("WORLD-FORGE-ARCHETYPE-ID", ErrorCategory::Validation,
                "Archetype entity id is required", "Set a unique non-empty id for each entity."));
        }
        if (!ids.insert(entity.id).second) {
            return Result<void>::failure(archetype_error("WORLD-FORGE-ARCHETYPE-ID-DUP", ErrorCategory::Validation,
                "Duplicate archetype entity id: " + entity.id, "Ensure every entity id is unique."));
        }
    }
    return Result<void>::success();
}

Result<void> WorldForgeArchetypesAsset::validate_faction_refs(
    const std::unordered_set<std::string>& known_faction_ids) const {
    if (known_faction_ids.empty()) return Result<void>::success();
    for (const auto& entity : entities) {
        if (!entity.unlock || entity.unlock->faction_id.empty()) continue;
        if (known_faction_ids.find(entity.unlock->faction_id) == known_faction_ids.end()) {
            return Result<void>::failure(archetype_error("WORLD-FORGE-ARCHETYPE-FACTION", ErrorCategory::Validation,
                "Unknown unlock.factionId '" + entity.unlock->faction_id + "' on archetype '" + entity.id + "'",
                "Point unlock.factionId at a factions.worldforge.json entity id, or leave it empty."));
        }
    }
    return Result<void>::success();
}

const WorldForgeArchetypeEntity* WorldForgeArchetypesAsset::find_entity(const std::string& entity_id) const {
    for (const auto& entity : entities) {
        if (entity.id == entity_id) return &entity;
    }
    return nullptr;
}

WorldForgeArchetypeEntity* WorldForgeArchetypesAsset::find_entity(const std::string& entity_id) {
    for (auto& entity : entities) {
        if (entity.id == entity_id) return &entity;
    }
    return nullptr;
}

Result<WorldForgeArchetypesAsset> WorldForgeArchetypesAsset::parse(const std::string& text,
    const std::string& source_name) {
    try {
        const auto json = nlohmann::json::parse(text);
        if (!json.is_object()) {
            return Result<WorldForgeArchetypesAsset>::failure(archetype_error("WORLD-FORGE-ARCHETYPE-ROOT",
                ErrorCategory::Serialization, source_name + " must be a JSON object",
                "Wrap entities in an object with schemaVersion and id."));
        }
        WorldForgeArchetypesAsset asset;
        asset.schema_version = json.value("schemaVersion", 0);
        if (asset.schema_version != 1) {
            return Result<WorldForgeArchetypesAsset>::failure(archetype_error("WORLD-FORGE-ARCHETYPE-SCHEMA",
                ErrorCategory::Validation, "Unsupported World Forge archetypes schemaVersion",
                "Use schemaVersion 1."));
        }
        asset.id = json.value("id", std::string{});
        const auto entities = json.value("entities", nlohmann::json::array());
        if (!entities.is_array()) {
            return Result<WorldForgeArchetypesAsset>::failure(archetype_error("WORLD-FORGE-ARCHETYPE-ENTITIES",
                ErrorCategory::Validation, "entities must be an array", "Provide an entities array."));
        }
        for (const auto& node : entities) {
            if (!node.is_object()) {
                return Result<WorldForgeArchetypesAsset>::failure(archetype_error("WORLD-FORGE-ARCHETYPE-ENTITY",
                    ErrorCategory::Validation, "Each entity must be an object", "Fix entity entries."));
            }
            WorldForgeArchetypeEntity entity;
            entity.id = node.value("id", std::string{});
            if (entity.id.empty()) {
                return Result<WorldForgeArchetypesAsset>::failure(archetype_error("WORLD-FORGE-ARCHETYPE-ID",
                    ErrorCategory::Validation, "Archetype entity id is required",
                    "Set a unique non-empty id for each entity."));
            }
            const auto kind = parse_kind(node.value("kind", std::string{"starting"}));
            if (!kind) return Result<WorldForgeArchetypesAsset>::failure(kind.error());
            entity.kind = kind.value();
            entity.display_name = node.value("displayName", std::string{});
            entity.role = node.value("role", std::string{});
            entity.summary = node.value("summary", std::string{});
            entity.draft_advancement = node.value("draftAdvancement", std::string{});
            entity.starter_kit_prefab_id = node.value("starterKitPrefabId", std::string{});
            entity.story_ref = node.value("storyRef", std::string{});
            entity.tags = read_string_array(node.value("tags", nlohmann::json::array()));
            if (node.contains("unlock") && node["unlock"].is_object()) {
                WorldForgeArchetypeUnlock unlock;
                const auto& unlock_node = node["unlock"];
                if (unlock_node.contains("moralityThreshold") && !unlock_node["moralityThreshold"].is_null()) {
                    if (!unlock_node["moralityThreshold"].is_number()) {
                        return Result<WorldForgeArchetypesAsset>::failure(
                            archetype_error("WORLD-FORGE-ARCHETYPE-UNLOCK", ErrorCategory::Validation,
                                "unlock.moralityThreshold must be a number on archetype '" + entity.id + "'",
                                "Use a JSON number for moralityThreshold."));
                    }
                    unlock.morality_threshold = unlock_node["moralityThreshold"].get<double>();
                }
                unlock.faction_id = unlock_node.value("factionId", std::string{});
                unlock.tags = read_string_array(unlock_node.value("tags", nlohmann::json::array()));
                if (unlock_has_content(unlock)) entity.unlock = std::move(unlock);
            }
            asset.entities.push_back(std::move(entity));
        }
        if (const auto valid = asset.validate(); !valid) {
            return Result<WorldForgeArchetypesAsset>::failure(valid.error());
        }
        return Result<WorldForgeArchetypesAsset>::success(std::move(asset));
    } catch (const std::exception& exception) {
        auto error = archetype_error("WORLD-FORGE-ARCHETYPE-PARSE", ErrorCategory::Serialization,
            "Failed to parse " + source_name, "Fix JSON syntax.");
        error.causes.push_back(exception.what());
        return Result<WorldForgeArchetypesAsset>::failure(std::move(error));
    }
}

Result<WorldForgeArchetypesAsset> WorldForgeArchetypesAsset::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Result<WorldForgeArchetypesAsset>::failure(archetype_error("WORLD-FORGE-ARCHETYPE-READ",
            ErrorCategory::Io, "Could not read World Forge archetypes: " + path.generic_string(),
            "Check the path and file permissions."));
    }
    std::ostringstream text;
    text << input.rdbuf();
    return parse(text.str(), path.filename().string());
}

std::string WorldForgeArchetypesAsset::to_json() const {
    nlohmann::ordered_json json;
    json["schemaVersion"] = schema_version;
    json["id"] = id;
    auto entities_json = nlohmann::ordered_json::array();
    for (const auto& entity : this->entities) {
        nlohmann::ordered_json node;
        node["id"] = entity.id;
        node["kind"] = to_string(entity.kind);
        node["displayName"] = entity.display_name;
        node["role"] = entity.role;
        node["summary"] = entity.summary;
        node["draftAdvancement"] = entity.draft_advancement;
        node["starterKitPrefabId"] = entity.starter_kit_prefab_id;
        node["storyRef"] = entity.story_ref;
        node["tags"] = write_string_array(entity.tags);
        if (entity.unlock && unlock_has_content(*entity.unlock)) {
            nlohmann::ordered_json unlock_node;
            if (entity.unlock->morality_threshold) {
                unlock_node["moralityThreshold"] = *entity.unlock->morality_threshold;
            }
            unlock_node["factionId"] = entity.unlock->faction_id;
            unlock_node["tags"] = write_string_array(entity.unlock->tags);
            node["unlock"] = std::move(unlock_node);
        }
        entities_json.push_back(std::move(node));
    }
    json["entities"] = std::move(entities_json);
    return json.dump(2) + "\n";
}

Result<void> WorldForgeArchetypesAsset::save_atomic(const std::filesystem::path& path) const {
    const auto valid = validate();
    if (!valid) return Result<void>::failure(valid.error());
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    const auto temporary = path.string() + ".tmp";
    const auto backup = path.string() + ".bak";
    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) {
            return Result<void>::failure(archetype_error("WORLD-FORGE-ARCHETYPE-IO", ErrorCategory::Io,
                "Could not write World Forge archetypes: " + path.generic_string(),
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
        return Result<void>::failure(archetype_error("WORLD-FORGE-ARCHETYPE-IO", ErrorCategory::Io,
            "Could not replace World Forge archetypes: " + path.generic_string(),
            "Check file permissions and disk space."));
    }
    if (std::filesystem::exists(backup)) std::filesystem::remove(backup, ignored);
    return Result<void>::success();
}

Result<void> WorldForgeArchetypesAsset::validate_file(const std::filesystem::path& path) {
    return validate_file(path, {});
}

Result<void> WorldForgeArchetypesAsset::validate_file(const std::filesystem::path& path,
    const std::unordered_set<std::string>& known_faction_ids) {
    if (!std::filesystem::exists(path)) return Result<void>::success();
    const auto loaded = load(path);
    if (!loaded) return Result<void>::failure(loaded.error());
    if (const auto refs = loaded.value().validate_faction_refs(known_faction_ids); !refs) {
        return Result<void>::failure(refs.error());
    }
    return Result<void>::success();
}

} // namespace engine
