#include "engine/assets/character_asset.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace engine {
namespace {

EngineError character_asset_error(std::string code, std::string message) {
    return {std::move(code), Severity::Error, ErrorCategory::Validation, "character-assets", std::move(message),
            ENGINE_SOURCE_CONTEXT, {}, "Correct the character asset JSON values and paths.", make_correlation_id()};
}

bool positive(float value) { return std::isfinite(value) && value > 0.0f; }

} // namespace

Result<void> CharacterAsset::validate() const {
    if (schema_version != 1)
        return Result<void>::failure(
            character_asset_error("CHARACTER-SCHEMA-UNSUPPORTED", "Only character schema version 1 is supported"));
    if (visual_prefab.empty())
        return Result<void>::failure(
            character_asset_error("CHARACTER-PREFAB-MISSING", "visualPrefab must reference a prefab asset"));
    if (!positive(capsule_radius) || !positive(capsule_half_height))
        return Result<void>::failure(
            character_asset_error("CHARACTER-SHAPE-INVALID", "Capsule radius and half height must be positive"));
    if (!positive(max_slope_ratio) || !positive(step_height) || !positive(max_speed) || !positive(gravity) ||
        !positive(jump_velocity))
        return Result<void>::failure(
            character_asset_error("CHARACTER-MOVEMENT-INVALID", "Movement values must be positive"));
    return Result<void>::success();
}

std::string CharacterAsset::to_json() const {
    nlohmann::ordered_json root{{"schemaVersion", schema_version},
                                {"visualPrefab", visual_prefab},
                                {"capsuleRadius", capsule_radius},
                                {"capsuleHalfHeight", capsule_half_height},
                                {"maxSlopeRatio", max_slope_ratio},
                                {"stepHeight", step_height},
                                {"maxSpeed", max_speed},
                                {"gravity", gravity},
                                {"jumpVelocity", jump_velocity}};
    if (!rig.empty()) root["rig"] = rig;
    return root.dump(2) + "\n";
}

Result<CharacterAsset> CharacterAsset::from_json(const std::string& text) {
    try {
        const auto root = nlohmann::json::parse(text);
        CharacterAsset value;
        value.schema_version = root.at("schemaVersion").get<std::uint32_t>();
        value.visual_prefab = root.at("visualPrefab").get<std::string>();
        value.rig = root.value("rig", std::string{});
        value.capsule_radius = root.value("capsuleRadius", 0.35f);
        value.capsule_half_height = root.value("capsuleHalfHeight", 0.85f);
        value.max_slope_ratio = root.value("maxSlopeRatio", 0.45f);
        value.step_height = root.value("stepHeight", 0.35f);
        value.max_speed = root.value("maxSpeed", 6.0f);
        value.gravity = root.value("gravity", 9.81f);
        value.jump_velocity = root.value("jumpVelocity", 5.0f);
        if (const auto valid = value.validate(); !valid) return Result<CharacterAsset>::failure(valid.error());
        return Result<CharacterAsset>::success(std::move(value));
    } catch (const std::exception& exception) {
        auto error = character_asset_error("CHARACTER-PARSE-FAILED", "Character asset JSON is malformed");
        error.causes.push_back(exception.what());
        return Result<CharacterAsset>::failure(std::move(error));
    }
}

Result<CharacterAsset> CharacterAsset::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return Result<CharacterAsset>::failure(
            character_asset_error("CHARACTER-READ-FAILED", "Could not read character asset: " + path.generic_string()));
    std::ostringstream text;
    text << input.rdbuf();
    return from_json(text.str());
}

CharacterControllerConfig CharacterAsset::controller_config() const {
    CharacterControllerConfig config;
    config.capsule_radius = capsule_radius;
    config.capsule_half_height = capsule_half_height;
    config.max_slope_ratio = max_slope_ratio;
    config.step_height = step_height;
    config.max_speed = max_speed;
    config.gravity = gravity;
    config.jump_velocity = jump_velocity;
    return config;
}

} // namespace engine
