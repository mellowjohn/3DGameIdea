#pragma once

#include "engine/animation/animator_runtime.h"
#include "engine/physics/character_controller.h"
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
 * Tick animator and apply animation-driven root motion to the capsule when enabled (DEC-0030).
 * Returns true if root motion was applied; false if the instance uses input-driven move instead.
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
