#pragma once

#include "engine/core/result.h"
#include "engine/rendering/orbit_camera.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace engine {

struct CameraAsset {
    std::uint32_t schema_version = 1;
    float pivot_height = 1.75f;
    float min_distance = 1.5f;
    float max_distance = 28.0f;
    float default_distance = 10.5f;
    float collision_probe_radius = 0.25f;
    float collision_padding = 0.2f;
    float shoulder_offset = 0.45f;
    float default_pitch = 0.32f;
    float min_pitch = -0.15f;
    float max_pitch = 1.25f;
    float look_sensitivity = 0.003f;
    /** Meters of orbit distance change per mouse-wheel notch. */
    float zoom_sensitivity = 1.5f;
    float vertical_fov_radians = 1.134464f; // ~65°
    float near_plane = 0.1f;
    float far_plane = 2000.0f;

    [[nodiscard]] Result<void> validate() const;
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static Result<CameraAsset> from_json(const std::string& text);
    [[nodiscard]] static Result<CameraAsset> load(const std::filesystem::path& path);
    [[nodiscard]] OrbitCameraConfig orbit_config() const;
};

} // namespace engine
