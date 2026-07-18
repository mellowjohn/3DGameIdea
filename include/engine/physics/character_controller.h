#pragma once

#include "engine/core/result.h"
#include "engine/physics/collision_world.h"
#include "engine/world/world_partition.h"

#include <array>
#include <cmath>
#include <memory>

namespace engine {

struct CharacterControllerConfig {
    float capsule_radius = 0.35f;
    float capsule_half_height = 0.85f;
    float max_slope_ratio = 0.45f;
    float step_height = 0.35f;
    float max_speed = 6.0f;
    float gravity = 9.81f;
    float jump_velocity = 5.0f;
    /** Reach / leave wish speed on walkable ground (m/s²). */
    float ground_acceleration = 50.0f;
    /** Idle deceleration on walkable and steep ground (m/s²) — prevents ice sliding. */
    float ground_friction = 55.0f;
    /** Horizontal accel while airborne (m/s²). */
    float air_acceleration = 12.0f;
};

class CharacterController final {
public:
    CharacterController(const CharacterController&) = delete;
    CharacterController& operator=(const CharacterController&) = delete;
    CharacterController(CharacterController&&) noexcept;
    CharacterController& operator=(CharacterController&&) noexcept;
    ~CharacterController();

    [[nodiscard]] static Result<CharacterController> create(CollisionWorld& world, WorldPosition spawn,
        const CharacterControllerConfig& config = {});
    [[nodiscard]] Result<void> move(const LocalPosition& wish_velocity, float yaw_radians, float seconds);
    /**
     * Animation-driven step (DEC-0030): apply world-space root translation delta for this frame.
     * Horizontal velocity is delta/seconds (not max-speed clamped). Gravity/jump still apply unless
     * `apply_vertical` is true (then Y comes from the delta while grounded/airborne without gravity).
     */
    [[nodiscard]] Result<void> move_root_motion(const LocalPosition& world_delta, float seconds,
        bool apply_vertical = false);
    [[nodiscard]] Result<bool> jump();
    [[nodiscard]] WorldPosition position() const;
    [[nodiscard]] CellCoord owner_cell(const WorldPartition& partition) const;
    [[nodiscard]] bool on_ground() const;
    [[nodiscard]] bool on_steep_ground() const;
    [[nodiscard]] CollisionDebugBody debug_body() const;
    [[nodiscard]] CharacterControllerConfig config() const;
    [[nodiscard]] std::array<float, 3> linear_velocity() const;
    void set_position(WorldPosition position);
private:
    explicit CharacterController(CollisionWorld& world);
    struct Impl;
    CollisionWorld* world_ = nullptr;
    std::unique_ptr<Impl> impl_;
};

/** Horizontal walk facing in radians. Matches move() forward basis (yaw 0 → +Z). Keeps previous when slow. */
[[nodiscard]] inline float character_facing_yaw_from_velocity(float velocity_x, float velocity_z, float previous_yaw,
    float min_speed = 0.05f) {
    const float speed_sq = velocity_x * velocity_x + velocity_z * velocity_z;
    if (!(min_speed > 0) || speed_sq < min_speed * min_speed) return previous_yaw;
    return std::atan2(velocity_x, velocity_z);
}

/**
 * Face the camera-relative wish direction (same basis as CharacterController::move / RigidbodyLocomotion::move).
 * Prefer this while the player is steering so physics lateral drift does not yaw the mesh.
 */
[[nodiscard]] inline float character_facing_yaw_from_wish(float wish_x, float wish_z, float camera_yaw,
    float previous_yaw, float min_input = 0.01f) {
    const float input_sq = wish_x * wish_x + wish_z * wish_z;
    if (!(min_input > 0) || input_sq < min_input * min_input) return previous_yaw;
    const float forward_x = std::sin(camera_yaw);
    const float forward_z = std::cos(camera_yaw);
    const float right_x = std::cos(camera_yaw);
    const float right_z = -std::sin(camera_yaw);
    const float world_x = right_x * wish_x + forward_x * wish_z;
    const float world_z = right_z * wish_x + forward_z * wish_z;
    return std::atan2(world_x, world_z);
}

/** Face along the camera look vector (horizontal) so the mesh back faces the lens under shoulder offset. */
[[nodiscard]] inline float character_facing_yaw_from_camera_look(float eye_x, float eye_z, float target_x, float target_z,
    float previous_yaw) {
    const float dx = target_x - eye_x;
    const float dz = target_z - eye_z;
    if (dx * dx + dz * dz < 1.0e-8f) return previous_yaw;
    return std::atan2(dx, dz);
}

} // namespace engine
