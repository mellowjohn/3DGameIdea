#pragma once

#include "engine/animation/animator_runtime.h"
#include "engine/core/error.h"
#include "engine/physics/character_controller.h"
#include "engine/physics/collision_world.h"
#include "engine/world/world_partition.h"

#include <cmath>

namespace engine {

/** Rotate clip-space root delta (+Z forward) into world XZ by character yaw. */
[[nodiscard]] inline LocalPosition rotate_root_delta_by_yaw(const std::array<float, 3>& clip_delta, float yaw_radians) {
    const float forward_x = std::sin(yaw_radians);
    const float forward_z = std::cos(yaw_radians);
    const float right_x = std::cos(yaw_radians);
    const float right_z = -std::sin(yaw_radians);
    return {
        right_x * clip_delta[0] + forward_x * clip_delta[2],
        clip_delta[1],
        right_z * clip_delta[0] + forward_z * clip_delta[2],
    };
}

/**
 * Apply a world-space root translation delta to a dynamic CollisionWorld body (DEC-0030 / TICKET-0199).
 * Horizontal velocity is delta/seconds. Vertical is preserved (gravity) unless `apply_vertical`.
 */
[[nodiscard]] inline Result<void> apply_rigidbody_root_motion(CollisionWorld& world, CollisionBody body,
    const LocalPosition& world_delta, float seconds, bool apply_vertical = false) {
    if (!body.valid())
        return Result<void>::failure(EngineError{"ROOT-MOTION-BODY", Severity::Error, ErrorCategory::Physics,
            "root-motion", "Root motion body is invalid", ENGINE_SOURCE_CONTEXT, {},
            "Pass a valid dynamic CollisionBody from PlacementCollisionTracker.", make_correlation_id()});
    if (!(seconds > 0) || seconds > 0.25f)
        return Result<void>::failure(EngineError{"ROOT-MOTION-STEP", Severity::Error, ErrorCategory::Physics,
            "root-motion", "Step must be within (0, 0.25] seconds", ENGINE_SOURCE_CONTEXT, {},
            "Use the frame delta used for animator.tick.", make_correlation_id()});
    auto velocity = world.linear_velocity(body);
    if (!velocity) return Result<void>::failure(velocity.error());
    auto v = velocity.value();
    const float inv_dt = 1.0f / seconds;
    v[0] = world_delta.x * inv_dt;
    v[2] = world_delta.z * inv_dt;
    if (apply_vertical) v[1] = world_delta.y * inv_dt;
    return world.set_linear_velocity(body, v);
}

/**
 * Tick animator and apply animation-driven root motion to a Rigidbody-backed CollisionBody (DEC-0030 / TICKET-0199).
 * Returns true if root motion was applied; false if the instance uses input-driven move instead.
 */
[[nodiscard]] inline Result<bool> sync_rigidbody_root_motion(CollisionWorld& world, CollisionBody body,
    AnimatorRuntime& animator, const std::string& entity_id, float yaw_radians, float dt_seconds) {
    animator.tick(dt_seconds);
    const auto apply = animator.apply_root_motion(entity_id);
    if (!apply) return Result<bool>::failure(apply.error());
    if (!apply.value()) return Result<bool>::success(false);
    const auto delta = animator.root_motion_delta(entity_id);
    if (!delta) return Result<bool>::failure(delta.error());
    const LocalPosition world_delta = rotate_root_delta_by_yaw(delta.value().translation, yaw_radians);
    const auto moved =
        apply_rigidbody_root_motion(world, body, world_delta, dt_seconds, delta.value().include_y);
    if (!moved) return Result<bool>::failure(moved.error());
    return Result<bool>::success(true);
}

/**
 * Tick animator and apply animation-driven root motion to CharacterVirtual (DEC-0030).
 * Prefer `sync_rigidbody_root_motion` for Rigidbody-backed player/NPC placements (TICKET-0199).
 */
[[nodiscard]] inline Result<bool> sync_character_root_motion(CharacterController& character,
    AnimatorRuntime& animator, const std::string& entity_id, float yaw_radians, float dt_seconds) {
    animator.tick(dt_seconds);
    const auto apply = animator.apply_root_motion(entity_id);
    if (!apply) return Result<bool>::failure(apply.error());
    if (!apply.value()) return Result<bool>::success(false);
    const auto delta = animator.root_motion_delta(entity_id);
    if (!delta) return Result<bool>::failure(delta.error());
    const LocalPosition world = rotate_root_delta_by_yaw(delta.value().translation, yaw_radians);
    const auto moved = character.move_root_motion(world, dt_seconds, delta.value().include_y);
    if (!moved) return Result<bool>::failure(moved.error());
    return Result<bool>::success(true);
}

} // namespace engine
