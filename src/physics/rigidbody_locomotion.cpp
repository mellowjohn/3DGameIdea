#include "engine/physics/rigidbody_locomotion.h"

#include <cmath>

namespace engine {
namespace {

EngineError loco_error(std::string code, std::string message) {
    return {std::move(code), Severity::Error, ErrorCategory::Physics, "rigidbody-locomotion", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, "Check body handle, capsule size, and collision world state.",
        make_correlation_id()};
}

} // namespace

RigidbodyLocomotion::RigidbodyLocomotion(CollisionWorld& world, CollisionBody body, CharacterControllerConfig config,
    float capsule_radius, float capsule_half_height)
    : world_(&world), body_(body), config_(config), capsule_radius_(capsule_radius),
      capsule_half_height_(capsule_half_height) {}

bool RigidbodyLocomotion::refresh_ground_state() const {
    grounded_cache_ = false;
    ground_normal_ = {0.0f, 1.0f, 0.0f};
    if (!world_ || !body_.valid()) return false;
    const auto center = world_->position(body_);
    if (!center) return false;
    const float feet_offset = capsule_half_height_ + capsule_radius_;
    const WorldPosition feet{center.value().x, center.value().y - static_cast<double>(feet_offset), center.value().z};
    const auto overlaps = world_->overlap_sphere({feet.x, feet.y + 0.05, feet.z}, 0.2f);
    if (!overlaps) return false;
    for (const auto& hit : overlaps.value()) {
        if (hit.body.value == body_.value) continue;
        if (hit.layer == CollisionLayer::Trigger) continue;
        grounded_cache_ = true;
        return true;
    }
    return false;
}

bool RigidbodyLocomotion::on_ground() const { return refresh_ground_state(); }

Result<void> RigidbodyLocomotion::move(const LocalPosition& wish_velocity, float yaw_radians, float seconds) {
    if (!valid())
        return Result<void>::failure(loco_error("LOCO-NOT-READY", "Rigidbody locomotion is not bound to a body"));
    if (!(seconds > 0) || seconds > 0.25f)
        return Result<void>::failure(loco_error("LOCO-STEP-INVALID", "Step must be within (0, 0.25] seconds"));

    const float forward_x = std::sin(yaw_radians);
    const float forward_z = std::cos(yaw_radians);
    const float right_x = std::cos(yaw_radians);
    const float right_z = -std::sin(yaw_radians);
    LocalPosition wish{
        right_x * wish_velocity.x + forward_x * wish_velocity.z,
        0.0f,
        right_z * wish_velocity.x + forward_z * wish_velocity.z};

    const float wish_horizontal = std::sqrt(wish.x * wish.x + wish.z * wish.z);
    if (wish_horizontal > 0.0f) {
        const float target_speed =
            wish_horizontal <= 1.0f ? wish_horizontal * config_.max_speed : config_.max_speed;
        const float scale = target_speed / wish_horizontal;
        wish.x *= scale;
        wish.z *= scale;
    }

    const bool supported = refresh_ground_state();
    auto velocity_result = world_->linear_velocity(body_);
    if (!velocity_result) return Result<void>::failure(velocity_result.error());
    auto velocity = velocity_result.value();

    const bool has_wish = wish_horizontal > 1.0e-6f;
    const float accel = supported ? config_.ground_acceleration : config_.air_acceleration;
    const float friction = supported ? config_.ground_friction : 0.0f;

    if (!has_wish) {
        if (supported) {
            const float speed = std::sqrt(velocity[0] * velocity[0] + velocity[1] * velocity[1] + velocity[2] * velocity[2]);
            const float drop = friction * seconds;
            if (speed <= drop) {
                velocity = {0.0f, 0.0f, 0.0f};
            } else {
                const float scale = (speed - drop) / speed;
                velocity[0] *= scale;
                velocity[1] *= scale;
                velocity[2] *= scale;
            }
        } else {
            const float speed = std::sqrt(velocity[0] * velocity[0] + velocity[2] * velocity[2]);
            const float drop = config_.air_acceleration * 0.25f * seconds;
            if (speed > 0.0f) {
                const float new_speed = speed > drop ? speed - drop : 0.0f;
                const float scale = new_speed / speed;
                velocity[0] *= scale;
                velocity[2] *= scale;
            }
        }
    } else {
        LocalPosition target = wish;
        if (!supported) target.y = 0.0f;
        LocalPosition delta{target.x - velocity[0], supported ? (target.y - velocity[1]) : 0.0f, target.z - velocity[2]};
        if (!supported) delta.y = 0.0f;
        const float delta_len = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        const float max_delta = accel * seconds;
        if (delta_len > max_delta && delta_len > 0.0f) {
            const float scale = max_delta / delta_len;
            delta.x *= scale;
            delta.y *= scale;
            delta.z *= scale;
        }
        velocity[0] += delta.x;
        if (supported) velocity[1] += delta.y;
        velocity[2] += delta.z;
    }

    if (supported && jump_requested_) {
        velocity[1] = config_.jump_velocity;
        jump_requested_ = false;
    } else if (!supported) {
        jump_requested_ = false;
    }

    // Idle on ground: pin horizontal drift after friction (ice-slide guard).
    if (supported && !has_wish && !jump_requested_) {
        velocity[0] = 0.0f;
        velocity[2] = 0.0f;
        if (velocity[1] < 0.0f) velocity[1] = 0.0f;
    }

    return world_->set_linear_velocity(body_, velocity);
}

Result<bool> RigidbodyLocomotion::jump() {
    if (!valid())
        return Result<bool>::failure(loco_error("LOCO-NOT-READY", "Rigidbody locomotion is not bound to a body"));
    if (!refresh_ground_state()) return Result<bool>::success(false);
    jump_requested_ = true;
    return Result<bool>::success(true);
}

WorldPosition RigidbodyLocomotion::body_center() const {
    if (!world_ || !body_.valid()) return {};
    if (const auto p = world_->position(body_)) return p.value();
    return {};
}

WorldPosition RigidbodyLocomotion::feet_position() const {
    const auto center = body_center();
    return {center.x, center.y - static_cast<double>(capsule_half_height_ + capsule_radius_), center.z};
}

std::array<float, 3> RigidbodyLocomotion::linear_velocity() const {
    if (!world_ || !body_.valid()) return {};
    if (const auto v = world_->linear_velocity(body_)) return v.value();
    return {};
}

CollisionDebugBody RigidbodyLocomotion::debug_body() const {
    CollisionDebugBody debug;
    debug.body = body_;
    debug.layer = CollisionLayer::Dynamic;
    debug.shape = CollisionDebugShape::Capsule;
    debug.position = body_center();
    debug.half_extent = {capsule_half_height_, 0.0f, 0.0f};
    debug.radius = capsule_radius_;
    return debug;
}

} // namespace engine

