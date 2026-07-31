#include "engine/animation/bone_attachment.h"

#include <DirectXMath.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace engine {
namespace {

using namespace DirectX;

constexpr float k_deg_to_rad = 0.01745329252f;
constexpr float k_rad_to_deg = 57.2957795131f;

XMMATRIX matrix_from_transform(const TransformComponent& transform) {
    return XMMatrixScaling(transform.scale[0], transform.scale[1], transform.scale[2]) *
        XMMatrixRotationQuaternion(XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(transform.rotation.data()))) *
        XMMatrixTranslation(transform.position[0], transform.position[1], transform.position[2]);
}

/// XMMatrixDecompose gives up on mirrored or near-degenerate joint globals, which the RH-authored rig produces
/// often enough that a silent failure would snap welds to the origin. Recover TRS by hand in that case.
TransformComponent transform_from_matrix(FXMMATRIX matrix) {
    TransformComponent result;
    XMVECTOR scale;
    XMVECTOR rotation;
    XMVECTOR translation;
    if (XMMatrixDecompose(&scale, &rotation, &translation, matrix)) {
        XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(result.position.data()), translation);
        XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(result.rotation.data()), rotation);
        XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(result.scale.data()), scale);
        return result;
    }

    XMFLOAT4X4 m;
    XMStoreFloat4x4(&m, matrix);
    result.position = {m._41, m._42, m._43};
    XMVECTOR basis_x = XMVectorSet(m._11, m._12, m._13, 0.0f);
    XMVECTOR basis_y = XMVectorSet(m._21, m._22, m._23, 0.0f);
    XMVECTOR basis_z = XMVectorSet(m._31, m._32, m._33, 0.0f);
    const float sx = XMVectorGetX(XMVector3Length(basis_x));
    const float sy = XMVectorGetX(XMVector3Length(basis_y));
    const float sz = XMVectorGetX(XMVector3Length(basis_z));
    result.scale = {sx > 1e-8f ? sx : 1.0f, sy > 1e-8f ? sy : 1.0f, sz > 1e-8f ? sz : 1.0f};
    basis_x = sx > 1e-8f ? XMVectorScale(basis_x, 1.0f / sx) : XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    basis_y = sy > 1e-8f ? XMVectorScale(basis_y, 1.0f / sy) : XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    basis_z = sz > 1e-8f ? XMVectorScale(basis_z, 1.0f / sz) : XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    XMMATRIX orthonormal = XMMatrixIdentity();
    orthonormal.r[0] = basis_x;
    orthonormal.r[1] = basis_y;
    orthonormal.r[2] = basis_z;
    XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(result.rotation.data()), XMQuaternionRotationMatrix(orthonormal));
    return result;
}

} // namespace

std::array<float, 4> quaternion_from_euler_deg(const std::array<float, 3>& euler_deg) {
    const XMVECTOR q = XMQuaternionRotationRollPitchYaw(euler_deg[0] * k_deg_to_rad, euler_deg[1] * k_deg_to_rad,
        euler_deg[2] * k_deg_to_rad);
    std::array<float, 4> out{};
    XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(out.data()), q);
    return out;
}

std::array<float, 3> euler_deg_from_quaternion(const std::array<float, 4>& rotation) {
    const XMVECTOR q =
        XMQuaternionNormalize(XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(rotation.data())));
    XMFLOAT4X4 m;
    XMStoreFloat4x4(&m, XMMatrixRotationQuaternion(q));

    // XMQuaternionRotationRollPitchYaw applies roll (Z), then pitch (X), then yaw (Y), so the composed
    // row-vector matrix is Rz * Rx * Ry. Solve that product back for the three angles.
    const float sin_pitch = std::clamp(-m._32, -1.0f, 1.0f);
    const float pitch = std::asin(sin_pitch);
    float yaw = 0.0f;
    float roll = 0.0f;
    if (std::fabs(std::cos(pitch)) > 1e-5f) {
        roll = std::atan2(m._12, m._22);
        yaw = std::atan2(m._31, m._33);
    } else {
        // Gimbal lock: pitch is ±90°, roll and yaw share an axis. Fold everything into yaw.
        roll = 0.0f;
        yaw = std::atan2(-m._13, m._11);
    }
    return {pitch * k_rad_to_deg, yaw * k_rad_to_deg, roll * k_rad_to_deg};
}

TransformComponent weld_local_transform(const BoneWeld& weld) {
    TransformComponent local;
    local.position = weld.offset;
    local.rotation = quaternion_from_euler_deg(weld.euler_deg);
    local.scale = weld.scale;
    return local;
}

TransformComponent bone_socket_world(const BoneSocketChain& chain) {
    // The skinned mesh is drawn at owner_world * visual_local, and joint globals are in that mesh's model
    // space, so the joint frame needs all three links.
    const XMMATRIX joint = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(chain.joint_model.data()));
    return transform_from_matrix(
        joint * matrix_from_transform(chain.visual_local) * matrix_from_transform(chain.owner_world));
}

TransformComponent weld_world_transform(const TransformComponent& socket_world, const BoneWeld& weld) {
    return transform_from_matrix(
        matrix_from_transform(weld_local_transform(weld)) * matrix_from_transform(socket_world));
}

BoneWeld weld_from_world_transform(const TransformComponent& socket_world, const TransformComponent& target_world,
    std::string joint) {
    const XMMATRIX socket = matrix_from_transform(socket_world);
    XMVECTOR determinant;
    const XMMATRIX inverse_socket = XMMatrixInverse(&determinant, socket);
    BoneWeld weld;
    weld.joint = std::move(joint);
    if (std::fabs(XMVectorGetX(determinant)) < 1e-12f) return weld;

    const TransformComponent local = transform_from_matrix(matrix_from_transform(target_world) * inverse_socket);
    weld.offset = local.position;
    weld.euler_deg = euler_deg_from_quaternion(local.rotation);
    weld.scale = local.scale;
    return weld;
}

} // namespace engine
