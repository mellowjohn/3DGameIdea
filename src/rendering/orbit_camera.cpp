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
} // namespace

OrbitCamera::OrbitCamera(OrbitCameraConfig config)
    : config_(config), desired_distance_(config.default_distance), resolved_distance_(config.default_distance) {}

void OrbitCamera::apply_look(float mouse_dx, float mouse_dy) {
    yaw_ += mouse_dx * sensitivity_;
    pitch_ = std::clamp(pitch_ - mouse_dy * sensitivity_, -1.55334f, 1.55334f);
}

void OrbitCamera::set_orientation(float yaw, float pitch) {
    yaw_ = yaw;
    pitch_ = std::clamp(pitch, -1.55334f, 1.55334f);
}

void OrbitCamera::set_config(const OrbitCameraConfig& config) {
    config_ = config;
    desired_distance_ = std::clamp(desired_distance_, config_.min_distance, config_.max_distance);
    if (std::abs(desired_distance_ - config_.default_distance) < 0.001f)
        desired_distance_ = config_.default_distance;
}

void OrbitCamera::set_sensitivity(float sensitivity) {
    if (std::isfinite(sensitivity) && sensitivity > 0.0f) sensitivity_ = sensitivity;
}

std::array<float, 3> OrbitCamera::orbit_offset(float distance) const {
    const float cp = std::cos(pitch_);
    const float sp = std::sin(pitch_);
    const float sy = std::sin(yaw_);
    const float cy = std::cos(yaw_);
    return {-sy * cp * distance, sp * distance, -cy * cp * distance};
}

Result<void> OrbitCamera::update(WorldPosition pivot, const CollisionWorld& world) {
    pivot_ = pivot;
    collision_shortened_ = false;
    resolved_distance_ = std::clamp(desired_distance_, config_.min_distance, config_.max_distance);

    const auto offset = orbit_offset(resolved_distance_);
    const WorldPosition pivot_center{pivot.x, pivot.y + static_cast<double>(config_.pivot_height), pivot.z};
    const WorldPosition desired_eye{pivot_center.x + offset[0], pivot_center.y + offset[1], pivot_center.z + offset[2]};

    const LocalPosition sweep{
        static_cast<float>(desired_eye.x - pivot_center.x),
        static_cast<float>(desired_eye.y - pivot_center.y),
        static_cast<float>(desired_eye.z - pivot_center.z)};
    const float sweep_length = vector_length(sweep.x, sweep.y, sweep.z);
    if (sweep_length > 0.001f) {
        float hit_fraction = 1.0f;
        for (const auto layer : {CollisionLayer::StaticWorld, CollisionLayer::Dynamic}) {
            CollisionQueryFilter filter;
            filter.layer = layer;
            const auto hit = world.sweep_sphere(pivot_center, sweep, config_.collision_probe_radius, filter);
            if (hit && hit.value().has_value())
                hit_fraction = std::min(hit_fraction, hit.value().value().fraction);
        }
        if (hit_fraction < 1.0f) {
            const float shortened = resolved_distance_ * hit_fraction - config_.collision_padding;
            resolved_distance_ = std::max(config_.min_distance, shortened);
            collision_shortened_ = resolved_distance_ + config_.collision_padding < desired_distance_;
        }
    }

    const auto resolved_offset = orbit_offset(resolved_distance_);
    position_[0] = static_cast<float>(pivot_center.x + resolved_offset[0]);
    position_[1] = static_cast<float>(pivot_center.y + resolved_offset[1]);
    position_[2] = static_cast<float>(pivot_center.z + resolved_offset[2]);
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
    const float target_x = static_cast<float>(pivot_.x);
    const float target_y = static_cast<float>(pivot_.y + static_cast<double>(config_.pivot_height));
    const float target_z = static_cast<float>(pivot_.z);
    const float dx = target_x - position_[0];
    const float dy = target_y - position_[1];
    const float dz = target_z - position_[2];
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
