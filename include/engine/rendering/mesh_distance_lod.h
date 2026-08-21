#pragma once

#include <cstdint>

// Mesh / foliage view-distance LOD ladder (TICKET-0220 / TICKET-0277).
// Complements scatter falloff + fog; not a second streamer.

namespace engine {
namespace mesh_lod {

/// LOD0 (full authored mesh) until this distance when LOD1 exists.
inline constexpr float k_lod1_start_m = 140.0f;
/// LOD1 until this distance when LOD2 exists; otherwise LOD1 holds until far cull.
inline constexpr float k_lod2_start_m = 240.0f;
/// Near band when no LOD meshes: always draw full placed mesh through this range.
inline constexpr float k_near_full_end_m = 280.0f;
/// Far band: skip draw once beyond this (enter cull).
inline constexpr float k_far_cull_start_m = 360.0f;
/// Hysteresis: once culled, stay culled until closer than this (exit cull).
inline constexpr float k_far_cull_exit_m = k_near_full_end_m;
/// Hysteresis slack when stepping down LOD bands (meters).
inline constexpr float k_lod_hysteresis_m = 20.0f;

enum class Level : std::uint8_t { Lod0 = 0, Lod1 = 1, Lod2 = 2, Cull = 3 };

/// Pick draw level from distance + sticky prior level. Empty lod paths skip that band.
[[nodiscard]] inline Level select_level(
    float distance_m, Level prior, bool has_lod1, bool has_lod2) {
    if (prior == Level::Cull && distance_m > k_far_cull_exit_m)
        return Level::Cull;

    if (!has_lod1 && !has_lod2) {
        if (distance_m >= k_far_cull_start_m)
            return Level::Cull;
        return Level::Lod0;
    }

    if (distance_m >= k_far_cull_start_m)
        return Level::Cull;

    const float lod2_exit = k_lod2_start_m - k_lod_hysteresis_m;
    const float lod1_exit = k_lod1_start_m - k_lod_hysteresis_m;

    if (has_lod2 && (distance_m >= k_lod2_start_m
            || (prior == Level::Lod2 && distance_m > lod2_exit)))
        return Level::Lod2;

    if (has_lod1 && (distance_m >= k_lod1_start_m
            || (prior == Level::Lod1 && distance_m > lod1_exit)
            || (prior == Level::Lod2 && distance_m > lod1_exit)))
        return Level::Lod1;

    return Level::Lod0;
}

} // namespace mesh_lod
} // namespace engine
