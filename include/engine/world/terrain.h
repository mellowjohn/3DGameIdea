#pragma once

#include "engine/core/result.h"
#include "engine/world/world_partition.h"

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

namespace engine {

inline constexpr std::uint32_t k_default_terrain_resolution = 33;
inline constexpr float k_default_terrain_cell_size = 40.0f;

class TerrainEditStore;
class TerrainPaintStore;
struct MaterialAsset;

struct TerrainVertex {
    float x = 0, y = 0, z = 0;
    float r = 0, g = 0, b = 0;
    bool painted = false;
};

struct TerrainMesh {
    CellCoord coordinate{};
    std::uint32_t resolution = 0;
    float cell_size = 0;
    std::vector<float> heights;
    std::vector<TerrainVertex> triangles;

    [[nodiscard]] float sample(std::uint32_t x, std::uint32_t z) const;
};

[[nodiscard]] Result<TerrainMesh> generate_stylized_terrain(
    CellCoord coordinate, std::uint32_t resolution = k_default_terrain_resolution,
    float cell_size = k_default_terrain_cell_size, const TerrainEditStore* edits = nullptr,
    const TerrainPaintStore* paint = nullptr,
    const std::function<const MaterialAsset*(const std::string&)>& lookup_material = {});

[[nodiscard]] float procedural_terrain_height(float world_x, float world_z);
[[nodiscard]] float sample_terrain_height(float world_x, float world_z);

[[nodiscard]] CellCoord terrain_cell_for_position(float x, float z, float cell_size = k_default_terrain_cell_size);
[[nodiscard]] std::vector<CellCoord> terrain_cells_in_radius(CellCoord center, std::uint32_t radius);

} // namespace engine
