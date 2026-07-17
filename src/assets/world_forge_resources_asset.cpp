#include "engine/assets/world_forge_resources_asset.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace engine {
namespace {

EngineError resource_error(std::string code, ErrorCategory category, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, category, "world_forge_resources", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

Result<WorldForgeResourceKind> parse_kind(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "mineral") return Result<WorldForgeResourceKind>::success(WorldForgeResourceKind::Mineral);
    if (key == "herb") return Result<WorldForgeResourceKind>::success(WorldForgeResourceKind::Herb);
    if (key == "food") return Result<WorldForgeResourceKind>::success(WorldForgeResourceKind::Food);
    if (key == "craft") return Result<WorldForgeResourceKind>::success(WorldForgeResourceKind::Craft);
    if (key == "quest") return Result<WorldForgeResourceKind>::success(WorldForgeResourceKind::Quest);
    if (key == "other") return Result<WorldForgeResourceKind>::success(WorldForgeResourceKind::Other);
    return Result<WorldForgeResourceKind>::failure(resource_error("WORLD-FORGE-RESOURCE-KIND",
        ErrorCategory::Validation, "Unsupported World Forge resource kind: " + raw,
        "Use mineral, herb, food, craft, quest, or other."));
}

Result<WorldForgeResourceRarity> parse_rarity(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "common") return Result<WorldForgeResourceRarity>::success(WorldForgeResourceRarity::Common);
    if (key == "uncommon") return Result<WorldForgeResourceRarity>::success(WorldForgeResourceRarity::Uncommon);
    if (key == "rare") return Result<WorldForgeResourceRarity>::success(WorldForgeResourceRarity::Rare);
    if (key == "legendary") return Result<WorldForgeResourceRarity>::success(WorldForgeResourceRarity::Legendary);
    if (key == "unique") return Result<WorldForgeResourceRarity>::success(WorldForgeResourceRarity::Unique);
    return Result<WorldForgeResourceRarity>::failure(resource_error("WORLD-FORGE-RESOURCE-RARITY",
        ErrorCategory::Validation, "Unsupported World Forge resource rarity: " + raw,
        "Use common, uncommon, rare, legendary, or unique."));
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

const char* to_string(WorldForgeResourceKind value) noexcept {
    switch (value) {
    case WorldForgeResourceKind::Mineral: return "mineral";
    case WorldForgeResourceKind::Herb: return "herb";
    case WorldForgeResourceKind::Food: return "food";
    case WorldForgeResourceKind::Craft: return "craft";
    case WorldForgeResourceKind::Quest: return "quest";
    case WorldForgeResourceKind::Other: return "other";
    }
    return "other";
}

const char* to_string(WorldForgeResourceRarity value) noexcept {
    switch (value) {
    case WorldForgeResourceRarity::Common: return "common";
    case WorldForgeResourceRarity::Uncommon: return "uncommon";
    case WorldForgeResourceRarity::Rare: return "rare";
    case WorldForgeResourceRarity::Legendary: return "legendary";
    case WorldForgeResourceRarity::Unique: return "unique";
    }
    return "common";
}

std::filesystem::path default_world_forge_resources_path(const std::filesystem::path& project_root) {
    return project_root / "assets" / "world-forge" / "resources.worldforge.json";
}

Result<void> WorldForgeResourcesAsset::validate() const {
    if (schema_version != 1) {
        return Result<void>::failure(resource_error("WORLD-FORGE-RESOURCE-SCHEMA", ErrorCategory::Validation,
            "Only World Forge resources schemaVersion 1 is supported", "Use schemaVersion 1."));
    }
    std::unordered_set<std::string> ids;
    ids.reserve(entities.size());
    for (const auto& entity : entities) {
        if (entity.id.empty()) {
            return Result<void>::failure(resource_error("WORLD-FORGE-RESOURCE-ID", ErrorCategory::Validation,
                "Resource entity id is required", "Set a unique non-empty id for each entity."));
        }
        if (!ids.insert(entity.id).second) {
            return Result<void>::failure(resource_error("WORLD-FORGE-RESOURCE-ID-DUP", ErrorCategory::Validation,
                "Duplicate resource entity id: " + entity.id, "Ensure every entity id is unique."));
        }
    }
    return Result<void>::success();
}

Result<void> WorldForgeResourcesAsset::validate_region_refs(
    const std::unordered_set<std::string>& known_region_ids) const {
    if (known_region_ids.empty()) return Result<void>::success();
    for (const auto& entity : entities) {
        for (const auto& region_id : entity.region_ids) {
            if (region_id.empty()) continue;
            if (known_region_ids.find(region_id) == known_region_ids.end()) {
                return Result<void>::failure(resource_error("WORLD-FORGE-RESOURCE-REGION", ErrorCategory::Validation,
                    "Unknown regionId '" + region_id + "' on resource '" + entity.id + "'",
                    "Point regionIds at map.worldforge.json region ids, or remove the entry."));
            }
        }
    }
    return Result<void>::success();
}

const WorldForgeResourceEntity* WorldForgeResourcesAsset::find_entity(const std::string& entity_id) const {
    for (const auto& entity : entities) {
        if (entity.id == entity_id) return &entity;
    }
    return nullptr;
}

WorldForgeResourceEntity* WorldForgeResourcesAsset::find_entity(const std::string& entity_id) {
    for (auto& entity : entities) {
        if (entity.id == entity_id) return &entity;
    }
    return nullptr;
}

Result<WorldForgeResourcesAsset> WorldForgeResourcesAsset::parse(const std::string& text,
    const std::string& source_name) {
    try {
        const auto json = nlohmann::json::parse(text);
        if (!json.is_object()) {
            return Result<WorldForgeResourcesAsset>::failure(resource_error("WORLD-FORGE-RESOURCE-ROOT",
                ErrorCategory::Serialization, source_name + " must be a JSON object",
                "Wrap entities in an object with schemaVersion and id."));
        }
        WorldForgeResourcesAsset asset;
        asset.schema_version = json.value("schemaVersion", 0);
        if (asset.schema_version != 1) {
            return Result<WorldForgeResourcesAsset>::failure(resource_error("WORLD-FORGE-RESOURCE-SCHEMA",
                ErrorCategory::Validation, "Unsupported World Forge resources schemaVersion",
                "Use schemaVersion 1."));
        }
        asset.id = json.value("id", std::string{});
        const auto entities = json.value("entities", nlohmann::json::array());
        if (!entities.is_array()) {
            return Result<WorldForgeResourcesAsset>::failure(resource_error("WORLD-FORGE-RESOURCE-ENTITIES",
                ErrorCategory::Validation, "entities must be an array", "Provide an entities array."));
        }
        for (const auto& node : entities) {
            if (!node.is_object()) {
                return Result<WorldForgeResourcesAsset>::failure(resource_error("WORLD-FORGE-RESOURCE-ENTITY",
                    ErrorCategory::Validation, "Each entity must be an object", "Fix entity entries."));
            }
            WorldForgeResourceEntity entity;
            entity.id = node.value("id", std::string{});
            if (entity.id.empty()) {
                return Result<WorldForgeResourcesAsset>::failure(resource_error("WORLD-FORGE-RESOURCE-ID",
                    ErrorCategory::Validation, "Resource entity id is required",
                    "Set a unique non-empty id for each entity."));
            }
            const auto kind = parse_kind(node.value("kind", std::string{"other"}));
            if (!kind) return Result<WorldForgeResourcesAsset>::failure(kind.error());
            entity.kind = kind.value();
            entity.display_name = node.value("displayName", std::string{});
            entity.summary = node.value("summary", std::string{});
            entity.obtain_notes = node.value("obtainNotes", std::string{});
            entity.story_ref = node.value("storyRef", std::string{});
            entity.region_ids = read_string_array(node.value("regionIds", nlohmann::json::array()));
            entity.tags = read_string_array(node.value("tags", nlohmann::json::array()));
            if (node.contains("rarity") && !node["rarity"].is_null()) {
                if (!node["rarity"].is_string()) {
                    return Result<WorldForgeResourcesAsset>::failure(
                        resource_error("WORLD-FORGE-RESOURCE-RARITY", ErrorCategory::Validation,
                            "rarity must be a string on resource '" + entity.id + "'",
                            "Use common, uncommon, rare, legendary, or unique."));
                }
                const auto rarity = parse_rarity(node["rarity"].get<std::string>());
                if (!rarity) return Result<WorldForgeResourcesAsset>::failure(rarity.error());
                entity.rarity = rarity.value();
            }
            asset.entities.push_back(std::move(entity));
        }
        if (const auto valid = asset.validate(); !valid) {
            return Result<WorldForgeResourcesAsset>::failure(valid.error());
        }
        return Result<WorldForgeResourcesAsset>::success(std::move(asset));
    } catch (const std::exception& exception) {
        auto error = resource_error("WORLD-FORGE-RESOURCE-PARSE", ErrorCategory::Serialization,
            "Failed to parse " + source_name, "Fix JSON syntax.");
        error.causes.push_back(exception.what());
        return Result<WorldForgeResourcesAsset>::failure(std::move(error));
    }
}

Result<WorldForgeResourcesAsset> WorldForgeResourcesAsset::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Result<WorldForgeResourcesAsset>::failure(resource_error("WORLD-FORGE-RESOURCE-READ",
            ErrorCategory::Io, "Could not read World Forge resources: " + path.generic_string(),
            "Check the path and file permissions."));
    }
    std::ostringstream text;
    text << input.rdbuf();
    return parse(text.str(), path.filename().string());
}

std::string WorldForgeResourcesAsset::to_json() const {
    nlohmann::ordered_json json;
    json["schemaVersion"] = schema_version;
    json["id"] = id;
    auto entities_json = nlohmann::ordered_json::array();
    for (const auto& entity : this->entities) {
        nlohmann::ordered_json node;
        node["id"] = entity.id;
        node["kind"] = to_string(entity.kind);
        node["displayName"] = entity.display_name;
        node["summary"] = entity.summary;
        node["obtainNotes"] = entity.obtain_notes;
        node["storyRef"] = entity.story_ref;
        if (entity.rarity) node["rarity"] = to_string(*entity.rarity);
        node["regionIds"] = write_string_array(entity.region_ids);
        node["tags"] = write_string_array(entity.tags);
        entities_json.push_back(std::move(node));
    }
    json["entities"] = std::move(entities_json);
    return json.dump(2) + "\n";
}

Result<void> WorldForgeResourcesAsset::save_atomic(const std::filesystem::path& path) const {
    const auto valid = validate();
    if (!valid) return Result<void>::failure(valid.error());
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    const auto temporary = path.string() + ".tmp";
    const auto backup = path.string() + ".bak";
    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) {
            return Result<void>::failure(resource_error("WORLD-FORGE-RESOURCE-IO", ErrorCategory::Io,
                "Could not write World Forge resources: " + path.generic_string(),
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
        return Result<void>::failure(resource_error("WORLD-FORGE-RESOURCE-IO", ErrorCategory::Io,
            "Could not replace World Forge resources: " + path.generic_string(),
            "Check file permissions and disk space."));
    }
    if (std::filesystem::exists(backup)) std::filesystem::remove(backup, ignored);
    return Result<void>::success();
}

Result<void> WorldForgeResourcesAsset::validate_file(const std::filesystem::path& path) {
    return validate_file(path, {});
}

Result<void> WorldForgeResourcesAsset::validate_file(const std::filesystem::path& path,
    const std::unordered_set<std::string>& known_region_ids) {
    if (!std::filesystem::exists(path)) return Result<void>::success();
    const auto loaded = load(path);
    if (!loaded) return Result<void>::failure(loaded.error());
    if (const auto refs = loaded.value().validate_region_refs(known_region_ids); !refs) {
        return Result<void>::failure(refs.error());
    }
    return Result<void>::success();
}

} // namespace engine
