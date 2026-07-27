#include "engine/rendering/orbit_camera.h"



#include <DirectXMath.h>



#include <algorithm>

#include <cmath>



namespace engine {

namespace {

EngineError orbit_camera_error(std::string code, std::string message) {

    return {std::move(code), Severity::Error, ErrorCategory::Validation, "orbit-camera", std::move(message),

            ENGINE_SOURCE_CONTEXT, {}, "Use finite perspective parameters with 0 < near < far.", make_correlation_id()};

}



float vector_length(float x, float y, float z) { return std::sqrt(x * x + y * y + z * z); }



float frame_seconds(float delta_seconds) {

    if (!(delta_seconds > 0.0f) || delta_seconds > 0.25f) return 1.0f / 60.0f;

    return delta_seconds;

}



float exp_smooth(float current, float target, float rate, float seconds) {

    if (rate <= 0.0f) return target;

    const float t = 1.0f - std::exp(-rate * seconds);

    return current + (target - current) * t;

}

} // namespace



OrbitCamera::OrbitCamera(OrbitCameraConfig config)

    : config_(config), desired_distance_(config.default_distance), resolved_distance_(config.default_distance),

      pitch_(config.default_pitch) {

    clamp_pitch();

}



void OrbitCamera::clamp_pitch() {

    const float lo = std::min(config_.min_pitch, config_.max_pitch);

    const float hi = std::max(config_.min_pitch, config_.max_pitch);

    pitch_ = std::clamp(pitch_, lo, hi);

}



void OrbitCamera::write_eye(float distance, float shoulder_scale) {

    const WorldPosition pivot_center = look_target();

    const auto radial = orbit_offset(distance);

    const auto right = shoulder_right();

    const float shoulder_applied = config_.shoulder_offset * shoulder_scale;

    position_[0] = static_cast<float>(pivot_center.x + radial[0] + right[0] * shoulder_applied);

    position_[1] = static_cast<float>(pivot_center.y + radial[1] + right[1] * shoulder_applied);

    position_[2] = static_cast<float>(pivot_center.z + radial[2] + right[2] * shoulder_applied);

}



void OrbitCamera::apply_look(float mouse_dx, float mouse_dy) {

    yaw_ += mouse_dx * sensitivity_;

    pitch_ -= mouse_dy * sensitivity_;

    clamp_pitch();

    // Keep the rendered eye aligned with yaw/pitch immediately so look does not wait on the next

    // collision update (Rigidbody play path defers the full update until after physics).

    if (pivot_initialized_) write_eye(resolved_distance_, shoulder_scale_);

}



void OrbitCamera::adjust_distance(float delta_meters) {

    if (!std::isfinite(delta_meters)) return;

    const float lo = std::min(config_.min_distance, config_.max_distance);

    const float hi = std::max(config_.min_distance, config_.max_distance);

    desired_distance_ = std::clamp(desired_distance_ - delta_meters, lo, hi);

}



void OrbitCamera::set_orientation(float yaw, float pitch) {

    yaw_ = yaw;

    pitch_ = pitch;

    clamp_pitch();

}



void OrbitCamera::set_desired_distance(float distance_meters) {

    if (!std::isfinite(distance_meters)) return;

    const float lo = std::min(config_.min_distance, config_.max_distance);

    const float hi = std::max(config_.min_distance, config_.max_distance);

    desired_distance_ = std::clamp(distance_meters, lo, hi);

}



void OrbitCamera::set_config(const OrbitCameraConfig& config) {

    config_ = config;

    const float lo = std::min(config_.min_distance, config_.max_distance);

    const float hi = std::max(config_.min_distance, config_.max_distance);

    desired_distance_ = std::clamp(desired_distance_, lo, hi);

    if (std::abs(desired_distance_ - config_.default_distance) < 0.001f)

        desired_distance_ = config_.default_distance;

    clamp_pitch();

}



void OrbitCamera::set_sensitivity(float sensitivity) {

    if (std::isfinite(sensitivity) && sensitivity > 0.0f) sensitivity_ = sensitivity;

}



std::array<float, 3> OrbitCamera::orbit_offset(float distance) const {

    const float cp = std::cos(pitch_);

    const float sp = std::sin(pitch_);

    const float sy = std::sin(yaw_);

    const float cy = std::cos(yaw_);

    // yaw 0 / pitch 0 → behind on −Z, looking toward +Z (character forward).

    return {-sy * cp * distance, sp * distance, -cy * cp * distance};

}



std::array<float, 3> OrbitCamera::shoulder_right() const {

    const float cy = std::cos(yaw_);

    const float sy = std::sin(yaw_);

    return {cy, 0.0f, -sy};

}



WorldPosition OrbitCamera::look_target() const {

    return {pivot_.x, pivot_.y + static_cast<double>(config_.pivot_height), pivot_.z};

}



Result<void> OrbitCamera::update(WorldPosition pivot, const CollisionWorld& world, float delta_seconds) {

    const float seconds = frame_seconds(delta_seconds);

    // Soft-follow the gameplay pivot so rigidbody feet micro-jitter does not shake the lens while looking.
    // Keep Y near-snapped: lagging height on slopes put the collision probe inside the terrain mesh and the
    // look-around capture framed the underside of the hill. Snap entirely on large jumps (spawn/teleport).
    if (!pivot_initialized_) {
        pivot_ = pivot;
        pivot_initialized_ = true;
    } else {
        const double dx = pivot.x - pivot_.x;
        const double dy = pivot.y - pivot_.y;
        const double dz = pivot.z - pivot_.z;
        const double err_sq = dx * dx + dy * dy + dz * dz;
        constexpr double k_snap_distance_m = 2.0;
        if (err_sq > k_snap_distance_m * k_snap_distance_m) {
            pivot_ = pivot;
        } else {
            constexpr float k_pivot_xz_follow_rate = 22.0f;
            constexpr float k_pivot_y_follow_rate = 80.0f;
            const float tx = 1.0f - std::exp(-k_pivot_xz_follow_rate * seconds);
            const float ty = 1.0f - std::exp(-k_pivot_y_follow_rate * seconds);
            pivot_.x += dx * static_cast<double>(tx);
            pivot_.y += dy * static_cast<double>(ty);
            pivot_.z += dz * static_cast<double>(tx);
        }
    }



    collision_shortened_ = false;

    const float lo = std::min(config_.min_distance, config_.max_distance);

    const float hi = std::max(config_.min_distance, config_.max_distance);

    float target_distance = std::clamp(desired_distance_, lo, hi);

    float target_shoulder_scale = 1.0f;



    const WorldPosition pivot_center = look_target();

    const auto radial = orbit_offset(target_distance);

    const auto right = shoulder_right();

    const float shoulder = config_.shoulder_offset;

    const WorldPosition desired_eye{pivot_center.x + radial[0] + right[0] * shoulder,

        pivot_center.y + radial[1] + right[1] * shoulder, pivot_center.z + radial[2] + right[2] * shoulder};



    const LocalPosition sweep{static_cast<float>(desired_eye.x - pivot_center.x),

        static_cast<float>(desired_eye.y - pivot_center.y),

        static_cast<float>(desired_eye.z - pivot_center.z)};

    const float sweep_length = vector_length(sweep.x, sweep.y, sweep.z);

    if (sweep_length > 0.001f) {

        // Only collide against world geometry — Dynamic/Character include the player body and

        // would pin the camera at min distance (self-hit), killing scroll zoom.

        CollisionQueryFilter filter;

        filter.layer = CollisionLayer::StaticWorld;

        const auto hit = world.sweep_sphere(pivot_center, sweep, config_.collision_probe_radius, filter);

        float hit_fraction = 1.0f;

        if (hit && hit.value().has_value()) hit_fraction = hit.value().value().fraction;

        if (hit_fraction < 1.0f) {

            const float padded = std::max(0.0f, hit_fraction - (config_.collision_padding / sweep_length));

            target_shoulder_scale = padded;

            collision_shortened_ = true;

            target_distance = std::max(config_.min_distance, target_distance * padded);

        }

    }



    // Fast pull-in when blocked (avoid clipping), slow ease-out when clear — stops look-around distance pop
    // as the probe grazes trees/terrain undulation. Snap the pull-in when the new aim is clearly buried
    // (instant look deltas in captures / fast flicks); soft rate alone left the eye inside hills for frames.
    constexpr float k_pull_in_rate = 48.0f;
    constexpr float k_recover_rate = 7.0f;
    constexpr float k_snap_pull_in_ratio = 0.85f;
    if (target_distance < resolved_distance_) {
        if (target_distance < resolved_distance_ * k_snap_pull_in_ratio)
            resolved_distance_ = target_distance;
        else
            resolved_distance_ = exp_smooth(resolved_distance_, target_distance, k_pull_in_rate, seconds);
    } else
        resolved_distance_ = exp_smooth(resolved_distance_, target_distance, k_recover_rate, seconds);
    resolved_distance_ = std::clamp(resolved_distance_, lo, hi);

    if (target_shoulder_scale < shoulder_scale_) {
        if (target_shoulder_scale < shoulder_scale_ * k_snap_pull_in_ratio)
            shoulder_scale_ = target_shoulder_scale;
        else
            shoulder_scale_ = exp_smooth(shoulder_scale_, target_shoulder_scale, k_pull_in_rate, seconds);
    } else
        shoulder_scale_ = exp_smooth(shoulder_scale_, target_shoulder_scale, k_recover_rate, seconds);
    shoulder_scale_ = std::clamp(shoulder_scale_, 0.0f, 1.0f);

    collision_shortened_ = collision_shortened_ || shoulder_scale_ < 0.999f;



    write_eye(resolved_distance_, shoulder_scale_);

    return Result<void>::success();

}



Result<void> OrbitCamera::set_perspective(float f, float a, float n, float z) {

    if (!std::isfinite(f) || !std::isfinite(a) || !std::isfinite(n) || !std::isfinite(z) || f <= 0.1f || f >= 3.0f ||

        a <= 0 || n <= 0 || z <= n)

        return Result<void>::failure(orbit_camera_error("CAMERA-PERSPECTIVE-INVALID", "Invalid perspective parameters"));

    fov_ = f;

    aspect_ = a;

    near_ = n;

    far_ = z;

    return Result<void>::success();

}



std::array<float, 3> OrbitCamera::forward() const {

    // Aim at the character pivot (not the shoulder-shifted eye), classic RPG framing.

    const auto target = look_target();

    const float dx = static_cast<float>(target.x) - position_[0];

    const float dy = static_cast<float>(target.y) - position_[1];

    const float dz = static_cast<float>(target.z) - position_[2];

    const float length = vector_length(dx, dy, dz);

    if (!(length > 0)) return {0, 0, 1};

    return {dx / length, dy / length, dz / length};

}



std::array<float, 16> OrbitCamera::view_matrix() const {

    using namespace DirectX;

    const auto f = forward();

    const XMVECTOR eye = XMVectorSet(position_[0], position_[1], position_[2], 1);

    std::array<float, 16> out{};

    XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(out.data()),

        XMMatrixLookToLH(eye, XMVectorSet(f[0], f[1], f[2], 0), XMVectorSet(0, 1, 0, 0)));

    return out;

}



std::array<float, 16> OrbitCamera::projection_matrix() const {

    using namespace DirectX;

    std::array<float, 16> out{};

    XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(out.data()), XMMatrixPerspectiveFovLH(fov_, aspect_, near_, far_));

    return out;

}



std::array<float, 16> OrbitCamera::view_projection() const {

    using namespace DirectX;

    const auto v = view_matrix();

    const auto p = projection_matrix();

    std::array<float, 16> out{};

    XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(out.data()),

        XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(v.data())) *

            XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(p.data())));

    return out;

}



} // namespace engine


