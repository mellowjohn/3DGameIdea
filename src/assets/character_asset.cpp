#include "engine/assets/character_asset.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
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

std::string lower_copy(std::string_view text) {
    std::string out(text);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

bool option_matches(const AppearanceOption& option, std::string_view value) {
    const auto needle = lower_copy(value);
    return needle == lower_copy(option.id) || needle == lower_copy(option.display);
}

std::array<float, 3> read_rgb(const nlohmann::json& root, const char* key, std::array<float, 3> fallback) {
    if (!root.contains(key) || !root[key].is_array() || root[key].size() < 3) return fallback;
    return {root[key][0].get<float>(), root[key][1].get<float>(), root[key][2].get<float>()};
}

nlohmann::json rgb_json(const std::array<float, 3>& rgb) { return nlohmann::json::array({rgb[0], rgb[1], rgb[2]}); }

} // namespace

const std::vector<AppearanceOption>& appearance_hair_options() {
    static const std::vector<AppearanceOption> k_options{
        {"short", "Short Brown"},
        {"cropped", "Cropped Black"},
        {"blonde", "Ash Blonde"},
        {"shaved", "Shaved"},
    };
    return k_options;
}

const std::vector<AppearanceOption>& appearance_skin_options() {
    static const std::vector<AppearanceOption> k_options{
        {"warm_tan", "Warm Tan"},
        {"fair", "Fair"},
        {"olive", "Olive"},
        {"deep_brown", "Deep Brown"},
    };
    return k_options;
}

const std::vector<AppearanceOption>& appearance_eye_options() {
    static const std::vector<AppearanceOption> k_options{
        {"brown", "Brown"},
        {"green", "Green"},
        {"blue", "Blue"},
        {"hazel", "Hazel"},
    };
    return k_options;
}

std::string appearance_option_id(const std::vector<AppearanceOption>& options, std::string_view value) {
    for (const auto& option : options) {
        if (option_matches(option, value)) return option.id;
    }
    return options.empty() ? std::string{} : options.front().id;
}

CharacterAppearance appearance_from_option_ids(std::string_view hair, std::string_view skin, std::string_view eyes) {
    CharacterAppearance appearance;
    auto hair_id = appearance_option_id(appearance_hair_options(), hair);
    if (hair_id == "spikes")
        hair_id = "short";
    if (hair_id == "shaved") {
        appearance.hair_mesh.clear();
        appearance.hair_tint = {1.0f, 1.0f, 1.0f};
    } else {
        appearance.hair_mesh = "assets/models/test_hair_spikes.gltf";
        if (hair_id == "cropped")
            appearance.hair_tint = {0.22f, 0.16f, 0.12f};
        else if (hair_id == "blonde")
            appearance.hair_tint = {1.35f, 1.15f, 0.62f};
        else
            appearance.hair_tint = {1.0f, 0.78f, 0.42f};
    }

    const auto skin_id = appearance_option_id(appearance_skin_options(), skin);
    if (skin_id == "fair")
        appearance.skin_tint = {1.12f, 0.96f, 0.92f};
    else if (skin_id == "olive")
        appearance.skin_tint = {0.78f, 0.82f, 0.52f};
    else if (skin_id == "deep_brown")
        appearance.skin_tint = {0.42f, 0.28f, 0.18f};
    else
        appearance.skin_tint = {1.0f, 0.90f, 0.78f};

    const auto eye_id = appearance_option_id(appearance_eye_options(), eyes);
    if (eye_id == "green")
        appearance.eye_tint = {0.22f, 0.55f, 0.18f};
    else if (eye_id == "blue")
        appearance.eye_tint = {0.18f, 0.32f, 0.72f};
    else if (eye_id == "hazel")
        appearance.eye_tint = {0.42f, 0.28f, 0.10f};
    else
        appearance.eye_tint = {0.22f, 0.12f, 0.06f};
    return appearance;
}

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
    nlohmann::ordered_json appearance_json{{"hairMesh", appearance.hair_mesh},
                                           {"hairTint", rgb_json(appearance.hair_tint)},
                                           {"skinTint", rgb_json(appearance.skin_tint)},
                                           {"eyeTint", rgb_json(appearance.eye_tint)}};
    root["appearance"] = std::move(appearance_json);
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
        value.appearance = appearance_from_option_ids("short", "warm_tan", "brown");
        if (root.contains("appearance") && root["appearance"].is_object()) {
            const auto& appearance = root["appearance"];
            value.appearance.hair_mesh = appearance.value("hairMesh", value.appearance.hair_mesh);
            value.appearance.hair_tint = read_rgb(appearance, "hairTint", value.appearance.hair_tint);
            value.appearance.skin_tint = read_rgb(appearance, "skinTint", value.appearance.skin_tint);
            value.appearance.eye_tint = read_rgb(appearance, "eyeTint", value.appearance.eye_tint);
        }
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
