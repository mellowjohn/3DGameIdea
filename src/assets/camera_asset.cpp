#include "engine/assets/camera_asset.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <fstream>
#include <sstream>

namespace engine {
namespace {

EngineError camera_asset_error(std::string code, std::string message) {
    return {std::move(code), Severity::Error, ErrorCategory::Validation, "camera-assets", std::move(message),
            ENGINE_SOURCE_CONTEXT, {}, "Correct the camera asset JSON values.", make_correlation_id()};
}

bool positive(float value) { return std::isfinite(value) && value > 0.0f; }

} // namespace

Result<void> CameraAsset::validate() const {
    if (schema_version != 1)
        return Result<void>::failure(
            camera_asset_error("CAMERA-SCHEMA-UNSUPPORTED", "Only camera schema version 1 is supported"));
    if (!positive(pivot_height) || !positive(min_distance) || !positive(max_distance) || !positive(default_distance) ||
        !positive(collision_probe_radius) || !positive(collision_padding) || !positive(look_sensitivity) ||
        !positive(vertical_fov_radians) || !positive(near_plane) || !positive(far_plane) || far_plane <= near_plane)
        return Result<void>::failure(camera_asset_error("CAMERA-VALUES-INVALID", "Camera values are out of range"));
    if (min_distance > max_distance || default_distance < min_distance || default_distance > max_distance)
        return Result<void>::failure(
            camera_asset_error("CAMERA-DISTANCE-INVALID", "Camera distance limits must satisfy min <= default <= max"));
    if (vertical_fov_radians <= 0.1f || vertical_fov_radians >= 3.0f)
        return Result<void>::failure(
            camera_asset_error("CAMERA-FOV-INVALID", "verticalFovRadians must be within (0.1, 3.0)"));
    return Result<void>::success();
}

std::string CameraAsset::to_json() const {
    nlohmann::ordered_json root{{"schemaVersion", schema_version},
                                {"pivotHeight", pivot_height},
                                {"minDistance", min_distance},
                                {"maxDistance", max_distance},
                                {"defaultDistance", default_distance},
                                {"collisionProbeRadius", collision_probe_radius},
                                {"collisionPadding", collision_padding},
                                {"lookSensitivity", look_sensitivity},
                                {"verticalFovRadians", vertical_fov_radians},
                                {"nearPlane", near_plane},
                                {"farPlane", far_plane}};
    return root.dump(2) + "\n";
}

Result<CameraAsset> CameraAsset::from_json(const std::string& text) {
    try {
        const auto root = nlohmann::json::parse(text);
        CameraAsset value;
        value.schema_version = root.at("schemaVersion").get<std::uint32_t>();
        value.pivot_height = root.value("pivotHeight", 1.6f);
        value.min_distance = root.value("minDistance", 1.5f);
        value.max_distance = root.value("maxDistance", 8.0f);
        value.default_distance = root.value("defaultDistance", 5.0f);
        value.collision_probe_radius = root.value("collisionProbeRadius", 0.2f);
        value.collision_padding = root.value("collisionPadding", 0.15f);
        value.look_sensitivity = root.value("lookSensitivity", 0.0025f);
        value.vertical_fov_radians = root.value("verticalFovRadians", 1.04719755f);
        value.near_plane = root.value("nearPlane", 0.1f);
        value.far_plane = root.value("farPlane", 2000.0f);
        if (const auto valid = value.validate(); !valid) return Result<CameraAsset>::failure(valid.error());
        return Result<CameraAsset>::success(std::move(value));
    } catch (const std::exception& exception) {
        auto error = camera_asset_error("CAMERA-PARSE-FAILED", "Camera asset JSON is malformed");
        error.causes.push_back(exception.what());
        return Result<CameraAsset>::failure(std::move(error));
    }
}

Result<CameraAsset> CameraAsset::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return Result<CameraAsset>::failure(
            camera_asset_error("CAMERA-READ-FAILED", "Could not read camera asset: " + path.generic_string()));
    std::ostringstream text;
    text << input.rdbuf();
    return from_json(text.str());
}

OrbitCameraConfig CameraAsset::orbit_config() const {
    OrbitCameraConfig config;
    config.pivot_height = pivot_height;
    config.min_distance = min_distance;
    config.max_distance = max_distance;
    config.default_distance = default_distance;
    config.collision_probe_radius = collision_probe_radius;
    config.collision_padding = collision_padding;
    return config;
}

} // namespace engine
