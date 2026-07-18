#pragma once

#include "engine/physics/character_controller.h"
#include "engine/physics/collision_world.h"

#include <array>
#include <optional>

namespace engine {

/**
 * Drives a dynamic CollisionWorld body with CharacterController-like wish/accel/friction (DEC-0038 / TICKET-0198).
 * Does not use CharacterVirtual — gravity integration comes from Jolt when use_gravity is on the body.
 */
class RigidbodyLocomotion final {
public:
    RigidbodyLocomotion() = default;
    RigidbodyLocomotion(CollisionWorld& world, CollisionBody body, CharacterControllerConfig config,
        float capsule_radius, float capsule_half_height);

    [[nodiscard]] bool valid() const { return world_ != nullptr && body_.valid(); }
    [[nodiscard]] CollisionBody body() const { return body_; }
    [[nodiscard]] CharacterControllerConfig config() const { return config_; }
    [[nodiscard]] float capsule_radius() const { return capsule_radius_; }
    [[nodiscard]] float capsule_half_height() const { return capsule_half_height_; }

    [[nodiscard]] Result<void> move(const LocalPosition& wish_velocity, float yaw_radians, float seconds);
    [[nodiscard]] Result<bool> jump();
    [[nodiscard]] bool on_ground() const;
    [[nodiscard]] WorldPosition feet_position() const;
    [[nodiscard]] WorldPosition body_center() const;
    [[nodiscard]] std::array<float, 3> linear_velocity() const;
    [[nodiscard]] CollisionDebugBody debug_body() const;

private:
    CollisionWorld* world_ = nullptr;
    CollisionBody body_{};
    CharacterControllerConfig config_{};
    float capsule_radius_ = 0.35f;
    float capsule_half_height_ = 0.85f;
    bool jump_requested_ = false;
    mutable bool grounded_cache_ = false;
    mutable LocalPosition ground_normal_{0.0f, 1.0f, 0.0f};

    [[nodiscard]] bool refresh_ground_state() const;
};

} // namespace engine
