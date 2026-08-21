#pragma once

// Cascaded directional sun shadows (TICKET-0219). Tunables live next to SSAO knobs in render_app.cpp.

#include <DirectXMath.h>

#include <array>
#include <cmath>
#include <cstring>
#include <cstdint>

namespace engine {
namespace csm {

inline constexpr std::uint32_t k_cascade_count = 3;
inline constexpr std::uint32_t k_map_resolution = 1024;
// Split distances (meters from camera) — stylized outdoor readability over precision.
inline constexpr float k_split_distances[k_cascade_count] = {28.0f, 95.0f, 350.0f};
// Constant depth bias in light clip space (0..1). Slope bias is applied on the rasterizer.
inline constexpr float k_depth_bias = 0.0018f;
inline constexpr float k_normal_bias_meters = 0.05f;
inline constexpr float k_pcf_soft_texels = 1.35f;
inline constexpr int k_raster_depth_bias = 0;
inline constexpr float k_raster_slope_scaled_depth_bias = 2.5f;
// Matches Frame CB sun travel direction (world → light) in draw_world_pass.
inline constexpr float k_sun_travel[3] = {-0.40f, -0.85f, -0.30f};

struct ShadowConstantBlock {
    std::array<float, 16> cascade_view_proj[k_cascade_count]{};
    float splits[4]{};  // xyz = split distances, w = cascade count
    float params[4]{};  // bias, normalBias, softTexels, mapResolution
};

/// Sphere-fit cascades centered on the camera (stable for stylized outdoor; snap to texel grid).
inline ShadowConstantBlock build_cascades(const std::array<float, 3>& camera_position) {
    using namespace DirectX;
    ShadowConstantBlock block{};
    block.splits[0] = k_split_distances[0];
    block.splits[1] = k_split_distances[1];
    block.splits[2] = k_split_distances[2];
    block.splits[3] = static_cast<float>(k_cascade_count);
    block.params[0] = k_depth_bias;
    block.params[1] = k_normal_bias_meters;
    block.params[2] = k_pcf_soft_texels;
    block.params[3] = static_cast<float>(k_map_resolution);

    XMVECTOR sun = XMVector3Normalize(XMVectorSet(k_sun_travel[0], k_sun_travel[1], k_sun_travel[2], 0.0f));
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    if (std::abs(XMVectorGetX(XMVector3Dot(sun, up))) > 0.95f) up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    const XMVECTOR cam = XMVectorSet(camera_position[0], camera_position[1], camera_position[2], 1.0f);

    for (std::uint32_t i = 0; i < k_cascade_count; ++i) {
        const float radius = k_split_distances[i];
        // Pull the light origin back along the sun so the sphere sits inside the ortho volume.
        const XMVECTOR light_pos = cam - sun * (radius * 2.0f);
        const XMMATRIX view = XMMatrixLookAtLH(light_pos, cam, up);
        // Snap ortho center to texel size to reduce shimmer when the camera moves.
        const float texel = (radius * 2.0f) / static_cast<float>(k_map_resolution);
        XMFLOAT3 center_light{};
        XMStoreFloat3(&center_light, XMVector3TransformCoord(cam, view));
        center_light.x = std::floor(center_light.x / texel) * texel;
        center_light.y = std::floor(center_light.y / texel) * texel;
        const XMMATRIX snap = XMMatrixTranslation(-center_light.x, -center_light.y, 0.0f);
        const XMMATRIX proj = XMMatrixOrthographicOffCenterLH(-radius, radius, -radius, radius, 0.5f, radius * 4.0f);
        XMFLOAT4X4 stored{};
        XMStoreFloat4x4(&stored, view * snap * proj);
        std::memcpy(block.cascade_view_proj[i].data(), &stored, sizeof(stored));
    }
    return block;
}

} // namespace csm
} // namespace engine
