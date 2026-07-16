#pragma once

#include "engine/core/result.h"
#include "engine/rendering/orbit_camera.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace engine {

struct CameraAsset {
    std::uint32_t schema_version = 1;
    float pivot_height = 1.6f;
    float min_distance = 1.5f;
    float max_distance = 8.0f;
    float default_distance = 5.0f;
    float collision_probe_radius = 0.2f;
    float collision_padding = 0.15f;
    float look_sensitivity = 0.0025f;
    float vertical_fov_radians = 1.04719755f;
    float near_plane = 0.1f;
    float far_plane = 2000.0f;

    [[nodiscard]] Result<void> validate() const;
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static Result<CameraAsset> from_json(const std::string& text);
    [[nodiscard]] static Result<CameraAsset> load(const std::filesystem::path& path);
    [[nodiscard]] OrbitCameraConfig orbit_config() const;
};

} // namespace engine
