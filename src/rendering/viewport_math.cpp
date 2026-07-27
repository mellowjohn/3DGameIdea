#include "engine/rendering/viewport_math.h"

#include <DirectXMath.h>

namespace engine {

bool project_world_to_screen(const std::array<float, 16>& view_projection, const ViewportRect& viewport, float world_x,
    float world_y, float world_z, float& screen_x, float& screen_y, float& depth) {
    using namespace DirectX;
    const float width = viewport.width();
    const float height = viewport.height();
    if (!(width > 0.0f) || !(height > 0.0f)) return false;

    const auto matrix = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(view_projection.data()));
    const auto ndc = XMVector3TransformCoord(XMVectorSet(world_x, world_y, world_z, 1.0f), matrix);
    depth = XMVectorGetZ(ndc);
    if (depth < 0.0f || depth > 1.0f) return false;
    screen_x = viewport.min_x + (XMVectorGetX(ndc) + 1.0f) * 0.5f * width;
    screen_y = viewport.min_y + (1.0f - XMVectorGetY(ndc)) * 0.5f * height;
    return true;
}

} // namespace engine
