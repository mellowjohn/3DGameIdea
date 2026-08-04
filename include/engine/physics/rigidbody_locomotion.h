#pragma once

#include "engine/physics/character_controller.h"
#include "engine/physics/collision_world.h"

#include <array>

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
    /**
     * Animator / jump gated contact: true while feet touch, sticky slope snap, or brief coyote after leave.
     * Intentional launch velocity clears this immediately.
     */
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
    /** Seconds since last physics support; start airborne until a real contact sticks. */
    mutable float air_time_ = 1.0f;
    mutable float ground_snap_gap_ = 0.0f;
    /** Last physics support (overlap or sticky under-foot surface) — not coyote-only. */
    mutable bool physics_supported_cache_ = false;
    /** Last anim/jump grounded (physics support or coyote). */
    mutable bool grounded_cache_ = false;
    mutable LocalPosition ground_normal_{0.0f, 1.0f, 0.0f};

    struct GroundProbe {
        bool contact = false;
        float snap_gap = 0.0f;
    };

    /** Overlap + short down-sweep so hill crest/trough micro-gaps stay supported. */
    [[nodiscard]] GroundProbe probe_ground_contact() const;
    /**
     * Recompute physics + anim grounded caches.
     * When `dt_seconds` > 0, advances air_time and may soft-snap the body onto sticky ground.
     */
    void refresh_ground_state(float dt_seconds = 0.0f) const;
};

} // namespace engine
