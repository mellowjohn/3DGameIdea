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

} // namespace engine
