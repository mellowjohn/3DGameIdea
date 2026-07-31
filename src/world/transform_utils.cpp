#include "engine/world/transform_utils.h"

#include <DirectXMath.h>

namespace engine {

TransformComponent multiply_transforms(const TransformComponent& parent, const TransformComponent& child) {
    using namespace DirectX;
    const auto parent_matrix =
        XMMatrixScaling(parent.scale[0], parent.scale[1], parent.scale[2]) *
        XMMatrixRotationQuaternion(XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(parent.rotation.data()))) *
        XMMatrixTranslation(parent.position[0], parent.position[1], parent.position[2]);
    const auto child_matrix =
        XMMatrixScaling(child.scale[0], child.scale[1], child.scale[2]) *
        XMMatrixRotationQuaternion(XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(child.rotation.data()))) *
        XMMatrixTranslation(child.position[0], child.position[1], child.position[2]);
    const auto world_matrix = child_matrix * parent_matrix;
    TransformComponent result;
    XMVECTOR scale;
    XMVECTOR rotation;
    XMVECTOR translation;
    XMMatrixDecompose(&scale, &rotation, &translation, world_matrix);
    XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(result.position.data()), translation);
    XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(result.rotation.data()), rotation);
    XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(result.scale.data()), scale);
    return result;
}

TransformComponent inverse_transform(const TransformComponent& transform) {
    using namespace DirectX;
    const auto matrix =
        XMMatrixScaling(transform.scale[0], transform.scale[1], transform.scale[2]) *
        XMMatrixRotationQuaternion(XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(transform.rotation.data()))) *
        XMMatrixTranslation(transform.position[0], transform.position[1], transform.position[2]);
    const auto inverse = XMMatrixInverse(nullptr, matrix);
    TransformComponent result;
    XMVECTOR scale;
    XMVECTOR rotation;
    XMVECTOR translation;
    XMMatrixDecompose(&scale, &rotation, &translation, inverse);
    XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(result.position.data()), translation);
    XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(result.rotation.data()), rotation);
    XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(result.scale.data()), scale);
    return result;
}

TransformComponent transform_from_column_major(const std::array<float, 16>& matrix) {
    using namespace DirectX;
    // Column-major-with-column-vectors and DirectXMath's row-major-with-row-vectors share a memory layout, so
    // the array loads straight into an XMMATRIX. Transposing it here instead put translation in the wrong
    // column (decompose then reported the origin) and handed back the inverse rotation.
    const XMMATRIX xm = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(matrix.data()));
    TransformComponent result;
    result.position = {matrix[12], matrix[13], matrix[14]};
    result.scale = {1.0f, 1.0f, 1.0f};
    result.rotation = {0.0f, 0.0f, 0.0f, 1.0f};

    XMVECTOR scale;
    XMVECTOR rotation;
    XMVECTOR translation;
    if (XMMatrixDecompose(&scale, &rotation, &translation, xm)) {
        XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(result.position.data()), translation);
        XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(result.rotation.data()), rotation);
        XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(result.scale.data()), scale);
        return result;
    }

    // Decompose often fails on near-reflected / poorly scaled joint globals; still recover TRS so
    // grip offset / euler authoring can move attached meshes.
    XMVECTOR basis_x = XMVectorSet(matrix[0], matrix[1], matrix[2], 0.0f);
    XMVECTOR basis_y = XMVectorSet(matrix[4], matrix[5], matrix[6], 0.0f);
    XMVECTOR basis_z = XMVectorSet(matrix[8], matrix[9], matrix[10], 0.0f);
    const float sx = XMVectorGetX(XMVector3Length(basis_x));
    const float sy = XMVectorGetX(XMVector3Length(basis_y));
    const float sz = XMVectorGetX(XMVector3Length(basis_z));
    result.scale = {sx > 1e-8f ? sx : 1.0f, sy > 1e-8f ? sy : 1.0f, sz > 1e-8f ? sz : 1.0f};
    basis_x = sx > 1e-8f ? XMVectorScale(basis_x, 1.0f / sx) : XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    basis_y = sy > 1e-8f ? XMVectorScale(basis_y, 1.0f / sy) : XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    basis_z = sz > 1e-8f ? XMVectorScale(basis_z, 1.0f / sz) : XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    XMMATRIX rot = XMMatrixIdentity();
    rot.r[0] = basis_x;
    rot.r[1] = basis_y;
    rot.r[2] = basis_z;
    rot.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(result.rotation.data()), XMQuaternionRotationMatrix(rot));
    return result;
}

} // namespace engine
