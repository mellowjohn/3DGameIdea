#pragma once

#include "engine/core/result.h"
#include "engine/physics/collision_world.h"
#include "engine/world/world_partition.h"

#include <array>

namespace engine {

struct OrbitCameraConfig {
    float pivot_height = 1.6f;
    float min_distance = 1.5f;
    float max_distance = 8.0f;
    float default_distance = 5.0f;
    float collision_probe_radius = 0.2f;
    float collision_padding = 0.15f;
};

class OrbitCamera final {
public:
    explicit OrbitCamera(OrbitCameraConfig config = {});

    void apply_look(float mouse_dx, float mouse_dy);
    void set_orientation(float yaw, float pitch);
    void set_config(const OrbitCameraConfig& config);
    void set_sensitivity(float sensitivity);
    [[nodiscard]] const OrbitCameraConfig& config() const { return config_; }
    [[nodiscard]] Result<void> update(WorldPosition pivot, const CollisionWorld& world);
    [[nodiscard]] Result<void> set_perspective(float vertical_fov_radians, float aspect, float near_plane,
        float far_plane);
    [[nodiscard]] std::array<float, 3> position() const { return position_; }
    [[nodiscard]] WorldPosition pivot() const { return pivot_; }
    [[nodiscard]] float yaw() const { return yaw_; }
    [[nodiscard]] float pitch() const { return pitch_; }
    [[nodiscard]] float desired_distance() const { return desired_distance_; }
    [[nodiscard]] float resolved_distance() const { return resolved_distance_; }
    [[nodiscard]] bool collision_shortened() const { return collision_shortened_; }
    [[nodiscard]] std::array<float, 3> forward() const;
    [[nodiscard]] std::array<float, 16> view_projection() const;
    [[nodiscard]] std::array<float, 16> view_matrix() const;
    [[nodiscard]] std::array<float, 16> projection_matrix() const;

private:
    [[nodiscard]] std::array<float, 3> orbit_offset(float distance) const;

    OrbitCameraConfig config_;
    WorldPosition pivot_{};
    std::array<float, 3> position_{0, 3, -5};
    float yaw_ = 0.0f;
    float pitch_ = 0.35f;
    float desired_distance_ = 5.0f;
    float resolved_distance_ = 5.0f;
    bool collision_shortened_ = false;
    float fov_ = 1.04719755f;
    float aspect_ = 16.0f / 9.0f;
    float near_ = 0.1f;
    float far_ = 2000.0f;
    float sensitivity_ = 0.0025f;
};

} // namespace engine
