#pragma once

#include "engine/core/result.h"
#include "engine/physics/collision_world.h"
#include "engine/world/world_partition.h"

#include <array>
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
    float swim_max_speed = 3.0f;
    float swim_fatigue_drain = 0.18f;
    float swim_damage_per_second = 8.0f;
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
    [[nodiscard]] bool swimming() const;
    [[nodiscard]] float swim_fatigue() const;
    [[nodiscard]] float pending_swim_damage() const;
    void clear_pending_swim_damage();
    void set_position(WorldPosition position);
private:
    explicit CharacterController(CollisionWorld& world);
    struct Impl;
    CollisionWorld* world_ = nullptr;
    std::unique_ptr<Impl> impl_;
};

} // namespace engine
