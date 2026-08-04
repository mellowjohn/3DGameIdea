#include "engine/physics/rigidbody_locomotion.h"

#include <algorithm>
#include <cmath>

namespace engine {
namespace {

EngineError loco_error(std::string code, std::string message) {
    return {std::move(code), Severity::Error, ErrorCategory::Physics, "rigidbody-locomotion", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, "Check body handle, capsule size, and collision world state.",
        make_correlation_id()};
}

// Sticky under-foot gap that still counts as physics support (crest/trough seams).
constexpr float k_ground_stick_distance = 0.28f;
// Animator + jump stay grounded briefly after leave so Fall does not flash on hills.
// Does NOT zero vertical velocity (that would look like floating).
constexpr float k_ground_coyote_time = 0.12f;
// Residual crest bounce vs deliberate jump: clear support only near jump-scale ascent.
// Fixed low thresholds (~1 m/s) treated post-land bounces as air and stuck Fall / failed ground asserts.
constexpr float k_leave_ground_up_velocity_min = 2.5f;

[[nodiscard]] bool hit_is_support(const OverlapHit& hit, CollisionBody self) {
    if (hit.body.value == self.value) return false;
    if (hit.layer == CollisionLayer::Trigger) return false;
    return true;
}

[[nodiscard]] bool hit_is_support(const SweepHit& hit, CollisionBody self) {
    if (hit.body.value == self.value) return false;
    if (hit.layer == CollisionLayer::Trigger) return false;
    return true;
}

} // namespace

RigidbodyLocomotion::RigidbodyLocomotion(CollisionWorld& world, CollisionBody body, CharacterControllerConfig config,
    float capsule_radius, float capsule_half_height)
    : world_(&world), body_(body), config_(config), capsule_radius_(capsule_radius),
      capsule_half_height_(capsule_half_height) {}

RigidbodyLocomotion::GroundProbe RigidbodyLocomotion::probe_ground_contact() const {
    GroundProbe result;
    if (!world_ || !body_.valid()) return result;
    const auto center = world_->position(body_);
    if (!center) return result;

    const float feet_offset = capsule_half_height_ + capsule_radius_;
    const WorldPosition feet{center.value().x, center.value().y - static_cast<double>(feet_offset), center.value().z};
    const float probe_radius = std::max(capsule_radius_ * 0.9f, 0.22f);

    // Primary: generous sphere just above the sole so flat ground and gentle slopes register.
    {
        const WorldPosition probe_a{feet.x, feet.y + 0.14, feet.z};
        if (const auto overlaps = world_->overlap_sphere(probe_a, probe_radius)) {
            for (const auto& hit : overlaps.value()) {
                if (hit_is_support(hit, body_)) {
                    result.contact = true;
                    return result;
                }
            }
        }
    }

    // Secondary: slightly lower sphere catches downhill micro-seams without waiting for gravity.
    {
        const WorldPosition probe_b{feet.x, feet.y + 0.02, feet.z};
        if (const auto overlaps = world_->overlap_sphere(probe_b, probe_radius * 0.95f)) {
            for (const auto& hit : overlaps.value()) {
                if (hit_is_support(hit, body_)) {
                    result.contact = true;
                    return result;
                }
            }
        }
    }

    // Sticky down-sweep: surface within stick range still counts as grounded (slope crests).
    const float stick = std::max(config_.step_height * 0.8f, k_ground_stick_distance);
    const float sweep_radius = std::max(probe_radius * 0.75f, 0.18f);
    const float start_lift = sweep_radius + 0.05f;
    const float sweep_len = stick + start_lift;
    const WorldPosition sweep_origin{feet.x, feet.y + static_cast<double>(start_lift), feet.z};
    if (const auto sweep = world_->sweep_sphere(sweep_origin, {0.0f, -sweep_len, 0.0f}, sweep_radius)) {
        if (sweep.value() && hit_is_support(sweep.value().value(), body_)) {
            const float travel = sweep_len * sweep.value()->fraction;
            // Distance from sole to contact after accounting for the start offset above the feet.
            result.snap_gap = std::max(0.0f, travel - start_lift);
            result.contact = true;
            return result;
        }
    }

    return result;
}

void RigidbodyLocomotion::refresh_ground_state(float dt_seconds) const {
    physics_supported_cache_ = false;
    grounded_cache_ = false;
    ground_normal_ = {0.0f, 1.0f, 0.0f};
    ground_snap_gap_ = 0.0f;
    if (!world_ || !body_.valid()) return;

    const GroundProbe probe = probe_ground_contact();
    ground_snap_gap_ = probe.snap_gap;

    auto velocity_result = world_->linear_velocity(body_);
    const float vy = velocity_result ? velocity_result.value()[1] : 0.0f;
    const float leave_up = std::max(k_leave_ground_up_velocity_min, config_.jump_velocity * 0.8f);
    // Jump-scale rise while feet still clip ground for a frame: treat as air so Fall/Jump anim works.
    const bool launch_rise = vy >= leave_up;

    if (probe.contact && !launch_rise) {
        air_time_ = 0.0f;
        physics_supported_cache_ = true;
        grounded_cache_ = true;

        // Soft-snap when only the sticky under-foot probe found ground a short way below
        // (classic hill crest "run while floating" case). Not while intentionally leaping.
        if (dt_seconds > 0.0f && probe.snap_gap > 0.02f && probe.snap_gap <= k_ground_stick_distance &&
            vy < 0.35f) {
            if (auto pos = world_->position(body_)) {
                WorldPosition snapped = pos.value();
                // Soft snap: partial pull so stairs do not hard-teleport every frame.
                const float pull = std::min(probe.snap_gap, std::max(probe.snap_gap * 0.7f, 5.0f * dt_seconds));
                snapped.y -= static_cast<double>(pull);
                if (const auto rot = world_->rotation(body_)) {
                    (void)world_->set_transform(body_, snapped, rot.value());
                } else {
                    (void)world_->set_transform(body_, snapped, {0.0f, 0.0f, 0.0f, 1.0f});
                }
                if (velocity_result) {
                    auto vel = velocity_result.value();
                    if (vel[1] > -0.25f) vel[1] = std::min(vel[1], 0.0f);
                    (void)world_->set_linear_velocity(body_, vel);
                }
            }
        }
        return;
    }

    if (dt_seconds > 0.0f) {
        air_time_ += dt_seconds;
    }

    if (launch_rise) {
        // Leave ground cleanly on jump so animator enters Fall/Jump and coyote does not re-stick.
        air_time_ = std::max(air_time_, k_ground_coyote_time + 0.05f);
        physics_supported_cache_ = false;
        grounded_cache_ = false;
        return;
    }

    // Coyote only keeps anim/jump grounded — physics falls with gravity so hills do not float.
    physics_supported_cache_ = false;
    grounded_cache_ = air_time_ <= k_ground_coyote_time;
}

bool RigidbodyLocomotion::on_ground() const {
    refresh_ground_state(0.0f);
    return grounded_cache_;
}

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

    refresh_ground_state(seconds);
    const bool physics_supported = physics_supported_cache_;
    // Coyote only for jump acceptance / residual stick feel — not for air-control accel.
    const bool friction_supported = physics_supported;

    auto velocity_result = world_->linear_velocity(body_);
    if (!velocity_result) return Result<void>::failure(velocity_result.error());
    auto velocity = velocity_result.value();

    const bool has_wish = wish_horizontal > 1.0e-6f;
    const float accel = friction_supported ? config_.ground_acceleration : config_.air_acceleration;
    const float friction = friction_supported ? config_.ground_friction : 0.0f;

    if (!has_wish) {
        if (friction_supported) {
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
        if (!friction_supported) target.y = 0.0f;
        LocalPosition delta{target.x - velocity[0], friction_supported ? (target.y - velocity[1]) : 0.0f,
            target.z - velocity[2]};
        if (!friction_supported) delta.y = 0.0f;
        const float delta_len = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        const float max_delta = accel * seconds;
        if (delta_len > max_delta && delta_len > 0.0f) {
            const float scale = max_delta / delta_len;
            delta.x *= scale;
            delta.y *= scale;
            delta.z *= scale;
        }
        velocity[0] += delta.x;
        if (friction_supported) velocity[1] += delta.y;
        velocity[2] += delta.z;
    }

    // Jump while physics-supported OR still in coyote (anim on_ground window).
    if (grounded_cache_ && jump_requested_) {
        velocity[1] = config_.jump_velocity;
        jump_requested_ = false;
        air_time_ = k_ground_coyote_time + 0.1f;
        physics_supported_cache_ = false;
        grounded_cache_ = false;
    } else if (!grounded_cache_) {
        jump_requested_ = false;
    }

    // Stick to surface while truly supported so downhill gravity does not micro-launch the capsule.
    // Coyote-only must NOT zero vertical velocity or falls feel floaty.
    if (physics_supported && velocity[1] < 0.0f) velocity[1] = 0.0f;
    // Kill residual lift on walkable ground (crest bounce / post-land micro hop). Full jump vy is higher.
    const float leave_up = std::max(k_leave_ground_up_velocity_min, config_.jump_velocity * 0.8f);
    if (physics_supported && !jump_requested_ && velocity[1] > 0.0f && velocity[1] < leave_up) {
        velocity[1] = 0.0f;
    }

    // Idle on ground: pin horizontal drift after friction (ice-slide guard).
    if (physics_supported && !has_wish && !jump_requested_) {
        velocity[0] = 0.0f;
        velocity[2] = 0.0f;
    }

    return world_->set_linear_velocity(body_, velocity);
}

Result<bool> RigidbodyLocomotion::jump() {
    if (!valid())
        return Result<bool>::failure(loco_error("LOCO-NOT-READY", "Rigidbody locomotion is not bound to a body"));
    refresh_ground_state(0.0f);
    if (!grounded_cache_) return Result<bool>::success(false);
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
