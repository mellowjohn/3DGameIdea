#include "engine/assets/world_forge_pantheon_asset.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace engine {
namespace {

EngineError pantheon_error(std::string code, ErrorCategory category, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, category, "world_forge_pantheon", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

Result<WorldForgePantheonKind> parse_kind(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "deity") return Result<WorldForgePantheonKind>::success(WorldForgePantheonKind::Deity);
    if (key == "aspect") return Result<WorldForgePantheonKind>::success(WorldForgePantheonKind::Aspect);
    if (key == "force") return Result<WorldForgePantheonKind>::success(WorldForgePantheonKind::Force);
    return Result<WorldForgePantheonKind>::failure(pantheon_error("WORLD-FORGE-PANTHEON-KIND",
        ErrorCategory::Validation, "Unsupported World Forge pantheon kind: " + raw,
        "Use deity, aspect, or force."));
}

Result<WorldForgePantheonCanonStatus> parse_canon_status(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "established")
        return Result<WorldForgePantheonCanonStatus>::success(WorldForgePantheonCanonStatus::Established);
    if (key == "draft") return Result<WorldForgePantheonCanonStatus>::success(WorldForgePantheonCanonStatus::Draft);
    if (key == "proposal")
        return Result<WorldForgePantheonCanonStatus>::success(WorldForgePantheonCanonStatus::Proposal);
    if (key == "open") return Result<WorldForgePantheonCanonStatus>::success(WorldForgePantheonCanonStatus::Open);
    return Result<WorldForgePantheonCanonStatus>::failure(pantheon_error("WORLD-FORGE-PANTHEON-CANON",
        ErrorCategory::Validation, "Unsupported canonStatus: " + raw,
        "Use established, draft, proposal, or open."));
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

const char* to_string(WorldForgePantheonKind value) noexcept {
    switch (value) {
    case WorldForgePantheonKind::Deity: return "deity";
    case WorldForgePantheonKind::Aspect: return "aspect";
    case WorldForgePantheonKind::Force: return "force";
    }
    return "deity";
}

const char* to_string(WorldForgePantheonCanonStatus value) noexcept {
    switch (value) {
    case WorldForgePantheonCanonStatus::Established: return "established";
    case WorldForgePantheonCanonStatus::Draft: return "draft";
    case WorldForgePantheonCanonStatus::Proposal: return "proposal";
    case WorldForgePantheonCanonStatus::Open: return "open";
    }
    return "draft";
}

std::filesystem::path default_world_forge_pantheon_path(const std::filesystem::path& project_root) {
    return project_root / "assets" / "world-forge" / "pantheon.worldforge.json";
}

Result<void> WorldForgePantheonAsset::validate() const {
    if (schema_version != 1) {
        return Result<void>::failure(pantheon_error("WORLD-FORGE-PANTHEON-SCHEMA", ErrorCategory::Validation,
            "Only World Forge pantheon schemaVersion 1 is supported", "Use schemaVersion 1."));
    }
    std::unordered_set<std::string> ids;
    std::unordered_map<std::string, std::string> parent_of;
    ids.reserve(entities.size());
    for (const auto& entity : entities) {
        if (entity.id.empty()) {
            return Result<void>::failure(pantheon_error("WORLD-FORGE-PANTHEON-ID", ErrorCategory::Validation,
                "Pantheon entity id is required", "Set a unique non-empty id for each entity."));
        }
        if (!ids.insert(entity.id).second) {
            return Result<void>::failure(pantheon_error("WORLD-FORGE-PANTHEON-ID-DUP", ErrorCategory::Validation,
                "Duplicate pantheon entity id: " + entity.id, "Ensure every entity id is unique."));
        }
        parent_of[entity.id] = entity.parent_id;
    }
    for (const auto& entity : entities) {
        if (entity.parent_id.empty()) continue;
        if (entity.parent_id == entity.id) {
            return Result<void>::failure(pantheon_error("WORLD-FORGE-PANTHEON-PARENT", ErrorCategory::Validation,
                "Pantheon entity parentId cannot reference itself: " + entity.id,
                "Point parentId at a different entity or leave it empty."));
        }
        if (ids.find(entity.parent_id) == ids.end()) {
            return Result<void>::failure(pantheon_error("WORLD-FORGE-PANTHEON-PARENT", ErrorCategory::Validation,
                "Unknown parentId '" + entity.parent_id + "' on entity '" + entity.id + "'",
                "parentId must match another entity id in the same file, or be empty."));
        }
        std::unordered_set<std::string> seen;
        std::string walk = entity.parent_id;
        while (!walk.empty()) {
            if (walk == entity.id || !seen.insert(walk).second) {
                return Result<void>::failure(pantheon_error("WORLD-FORGE-PANTHEON-PARENT-CYCLE",
                    ErrorCategory::Validation, "parentId cycle involving entity '" + entity.id + "'",
                    "Break the parentId cycle."));
            }
            const auto it = parent_of.find(walk);
            if (it == parent_of.end()) break;
            walk = it->second;
        }
    }
    return Result<void>::success();
}

const WorldForgePantheonEntity* WorldForgePantheonAsset::find_entity(const std::string& entity_id) const {
    for (const auto& entity : entities) {
        if (entity.id == entity_id) return &entity;
    }
    return nullptr;
}

WorldForgePantheonEntity* WorldForgePantheonAsset::find_entity(const std::string& entity_id) {
    for (auto& entity : entities) {
        if (entity.id == entity_id) return &entity;
    }
    return nullptr;
}

Result<WorldForgePantheonAsset> WorldForgePantheonAsset::parse(const std::string& text,
    const std::string& source_name) {
    try {
        const auto json = nlohmann::json::parse(text);
        if (!json.is_object()) {
            return Result<WorldForgePantheonAsset>::failure(pantheon_error("WORLD-FORGE-PANTHEON-ROOT",
                ErrorCategory::Serialization, source_name + " must be a JSON object",
                "Wrap entities in an object with schemaVersion and id."));
        }
        WorldForgePantheonAsset asset;
        asset.schema_version = json.value("schemaVersion", 0);
        if (asset.schema_version != 1) {
            return Result<WorldForgePantheonAsset>::failure(pantheon_error("WORLD-FORGE-PANTHEON-SCHEMA",
                ErrorCategory::Validation, "Unsupported World Forge pantheon schemaVersion",
                "Use schemaVersion 1."));
        }
        asset.id = json.value("id", std::string{});
        const auto entities = json.value("entities", nlohmann::json::array());
        if (!entities.is_array()) {
            return Result<WorldForgePantheonAsset>::failure(pantheon_error("WORLD-FORGE-PANTHEON-ENTITIES",
                ErrorCategory::Validation, "entities must be an array", "Provide an entities array."));
        }
        for (const auto& node : entities) {
            if (!node.is_object()) {
                return Result<WorldForgePantheonAsset>::failure(pantheon_error("WORLD-FORGE-PANTHEON-ENTITY",
                    ErrorCategory::Validation, "Each entity must be an object", "Fix entity entries."));
            }
            WorldForgePantheonEntity entity;
            entity.id = node.value("id", std::string{});
            if (entity.id.empty()) {
                return Result<WorldForgePantheonAsset>::failure(pantheon_error("WORLD-FORGE-PANTHEON-ID",
                    ErrorCategory::Validation, "Pantheon entity id is required",
                    "Set a unique non-empty id for each entity."));
            }
            const auto kind = parse_kind(node.value("kind", std::string{}));
            if (!kind) return Result<WorldForgePantheonAsset>::failure(kind.error());
            entity.kind = kind.value();
            entity.display_name = node.value("displayName", std::string{});
            const auto canon = parse_canon_status(node.value("canonStatus", std::string{}));
            if (!canon) return Result<WorldForgePantheonAsset>::failure(canon.error());
            entity.canon_status = canon.value();
            entity.summary = node.value("summary", std::string{});
            entity.story_ref = node.value("storyRef", std::string{});
            entity.tags = read_string_array(node.value("tags", nlohmann::json::array()));
            entity.parent_id = node.value("parentId", std::string{});
            entity.open_questions = read_string_array(node.value("openQuestions", nlohmann::json::array()));
            asset.entities.push_back(std::move(entity));
        }
        if (const auto valid = asset.validate(); !valid) {
            return Result<WorldForgePantheonAsset>::failure(valid.error());
        }
        return Result<WorldForgePantheonAsset>::success(std::move(asset));
    } catch (const std::exception& exception) {
        auto error = pantheon_error("WORLD-FORGE-PANTHEON-PARSE", ErrorCategory::Serialization,
            "Failed to parse " + source_name, "Fix JSON syntax.");
        error.causes.push_back(exception.what());
        return Result<WorldForgePantheonAsset>::failure(std::move(error));
    }
}

Result<WorldForgePantheonAsset> WorldForgePantheonAsset::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Result<WorldForgePantheonAsset>::failure(pantheon_error("WORLD-FORGE-PANTHEON-READ",
            ErrorCategory::Io, "Could not read World Forge pantheon: " + path.generic_string(),
            "Check the path and file permissions."));
    }
    std::ostringstream text;
    text << input.rdbuf();
    return parse(text.str(), path.filename().string());
}

std::string WorldForgePantheonAsset::to_json() const {
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
        node["parentId"] = entity.parent_id;
        node["openQuestions"] = write_string_array(entity.open_questions);
        entities_json.push_back(std::move(node));
    }
    json["entities"] = std::move(entities_json);
    return json.dump(2) + "\n";
}

Result<void> WorldForgePantheonAsset::save_atomic(const std::filesystem::path& path) const {
    const auto valid = validate();
    if (!valid) return Result<void>::failure(valid.error());
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    const auto temporary = path.string() + ".tmp";
    const auto backup = path.string() + ".bak";
    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) {
            return Result<void>::failure(pantheon_error("WORLD-FORGE-PANTHEON-IO", ErrorCategory::Io,
                "Could not write World Forge pantheon: " + path.generic_string(),
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
        return Result<void>::failure(pantheon_error("WORLD-FORGE-PANTHEON-IO", ErrorCategory::Io,
            "Could not replace World Forge pantheon: " + path.generic_string(),
            "Check file permissions and disk space."));
    }
    if (std::filesystem::exists(backup)) std::filesystem::remove(backup, ignored);
    return Result<void>::success();
}

Result<void> WorldForgePantheonAsset::validate_file(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return Result<void>::success();
    const auto loaded = load(path);
    if (!loaded) return Result<void>::failure(loaded.error());
    return Result<void>::success();
}

} // namespace engine
