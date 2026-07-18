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
bool finite_number(float value) { return std::isfinite(value); }

} // namespace

Result<void> CameraAsset::validate() const {
    if (schema_version != 1)
        return Result<void>::failure(
            camera_asset_error("CAMERA-SCHEMA-UNSUPPORTED", "Only camera schema version 1 is supported"));
    if (!positive(pivot_height) || !positive(min_distance) || !positive(max_distance) || !positive(default_distance) ||
        !positive(collision_probe_radius) || !positive(collision_padding) || !positive(look_sensitivity) ||
        !positive(zoom_sensitivity) || !positive(vertical_fov_radians) || !positive(near_plane) || !positive(far_plane) ||
        far_plane <= near_plane)
        return Result<void>::failure(camera_asset_error("CAMERA-VALUES-INVALID", "Camera values are out of range"));
    if (!finite_number(shoulder_offset) || !finite_number(default_pitch) || !finite_number(min_pitch) ||
        !finite_number(max_pitch))
        return Result<void>::failure(camera_asset_error("CAMERA-VALUES-INVALID", "Camera pitch/offset values must be finite"));
    if (min_distance > max_distance || default_distance < min_distance || default_distance > max_distance)
        return Result<void>::failure(
            camera_asset_error("CAMERA-DISTANCE-INVALID", "Camera distance limits must satisfy min <= default <= max"));
    if (min_pitch > max_pitch)
        return Result<void>::failure(
            camera_asset_error("CAMERA-PITCH-INVALID", "minPitch must be <= maxPitch"));
    if (default_pitch < min_pitch || default_pitch > max_pitch)
        return Result<void>::failure(
            camera_asset_error("CAMERA-PITCH-INVALID", "defaultPitch must lie within minPitch..maxPitch"));
    if (vertical_fov_radians <= 0.1f || vertical_fov_radians >= 3.0f)
        return Result<void>::failure(
            camera_asset_error("CAMERA-FOV-INVALID", "verticalFovRadians must be within (0.1, 3.0)"));
    return Result<void>::success();
}

std::string CameraAsset::to_json() const {
    const auto round4 = [](float v) {
        return std::round(static_cast<double>(v) * 10000.0) / 10000.0;
    };
    nlohmann::ordered_json root{{"schemaVersion", schema_version},
        {"pivotHeight", round4(pivot_height)},
        {"minDistance", round4(min_distance)},
        {"maxDistance", round4(max_distance)},
        {"defaultDistance", round4(default_distance)},
        {"collisionProbeRadius", round4(collision_probe_radius)},
        {"collisionPadding", round4(collision_padding)},
        {"shoulderOffset", round4(shoulder_offset)},
        {"defaultPitch", round4(default_pitch)},
        {"minPitch", round4(min_pitch)},
        {"maxPitch", round4(max_pitch)},
        {"lookSensitivity", round4(look_sensitivity)},
        {"zoomSensitivity", round4(zoom_sensitivity)},
        {"verticalFovRadians", round4(vertical_fov_radians)},
        {"nearPlane", round4(near_plane)},
        {"farPlane", round4(far_plane)}};
    return root.dump(2) + "\n";
}

Result<CameraAsset> CameraAsset::from_json(const std::string& text) {
    try {
        const auto root = nlohmann::json::parse(text);
        CameraAsset value;
        value.schema_version = root.at("schemaVersion").get<std::uint32_t>();
        value.pivot_height = root.value("pivotHeight", 1.75f);
        value.min_distance = root.value("minDistance", 1.5f);
        value.max_distance = root.value("maxDistance", 28.0f);
        value.default_distance = root.value("defaultDistance", 10.5f);
        value.collision_probe_radius = root.value("collisionProbeRadius", 0.25f);
        value.collision_padding = root.value("collisionPadding", 0.2f);
        value.shoulder_offset = root.value("shoulderOffset", 0.45f);
        value.default_pitch = root.value("defaultPitch", 0.32f);
        value.min_pitch = root.value("minPitch", -0.15f);
        value.max_pitch = root.value("maxPitch", 1.25f);
        value.look_sensitivity = root.value("lookSensitivity", 0.003f);
        value.zoom_sensitivity = root.value("zoomSensitivity", 1.5f);
        value.vertical_fov_radians = root.value("verticalFovRadians", 1.134464f);
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
    config.shoulder_offset = shoulder_offset;
    config.default_pitch = default_pitch;
    config.min_pitch = min_pitch;
    config.max_pitch = max_pitch;
    return config;
}

} // namespace engine
