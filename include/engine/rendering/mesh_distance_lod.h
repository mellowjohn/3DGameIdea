#pragma once

// Mesh / foliage view-distance LOD ladder (TICKET-0220). Complements scatter falloff + fog; not a second streamer.

namespace engine {
namespace mesh_lod {

/// Near band: always draw full placed mesh.
inline constexpr float k_near_full_end_m = 160.0f;
/// Far band: skip draw once beyond this (enter cull).
inline constexpr float k_far_cull_start_m = 210.0f;
/// Hysteresis: once culled, stay culled until closer than this (exit cull).
inline constexpr float k_far_cull_exit_m = k_near_full_end_m;

} // namespace mesh_lod
} // namespace engine
