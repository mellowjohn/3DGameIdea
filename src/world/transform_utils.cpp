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

} // namespace engine
