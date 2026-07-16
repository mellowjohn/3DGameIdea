#include "engine/world/terrain.h"

#include "engine/assets/material_asset.h"
#include "engine/world/terrain_edits.h"
#include "engine/world/terrain_paint.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>

namespace engine {
namespace {
EngineError terrain_error(std::string code, std::string message) {
    return {std::move(code), Severity::Error, ErrorCategory::Validation, "terrain",
            std::move(message), ENGINE_SOURCE_CONTEXT, {},
            "Use a finite positive cell size and a 2^n+1 resolution between 3 and 257.",
            make_correlation_id()};
}

float height_at(float x, float z) {
    const float broad = std::sin(x * 0.075f) * 1.9f + std::cos(z * 0.0625f) * 1.35f;
    const float ridge = std::sin((x + z) * 0.03125f) * 0.9f;
    const float hollow = -2.2f * std::exp(-(x * x + z * z) / 180.0f);
    return broad + ridge + hollow;
}

enum class TerrainSurface : std::uint8_t { Grass, Dirt, Rock, Snow, Corrupted };

TerrainSurface surface_at(float world_x, float world_z, float height, float slope) {
    const float corruption = std::sin(world_x * 0.11f) * std::cos(world_z * 0.09f);
    if (corruption > 0.72f) return TerrainSurface::Corrupted;
    if (height > 3.2f) return TerrainSurface::Snow;
    if (slope > 0.55f) return TerrainSurface::Rock;
    if (slope > 0.22f) return TerrainSurface::Dirt;
    return TerrainSurface::Grass;
}

std::array<float, 3> surface_color(TerrainSurface surface) {
    switch (surface) {
    case TerrainSurface::Grass: return {0.11f, 0.17f, 0.10f};
    case TerrainSurface::Dirt: return {0.14f, 0.12f, 0.09f};
    case TerrainSurface::Rock: return {0.10f, 0.10f, 0.11f};
    case TerrainSurface::Snow: return {0.20f, 0.21f, 0.22f};
    case TerrainSurface::Corrupted: return {0.16f, 0.12f, 0.20f};
    }
    return {0.12f, 0.14f, 0.11f};
}

float slope_at(const TerrainMesh& mesh, std::uint32_t x, std::uint32_t z, float step) {
    const auto sample = [&](std::uint32_t sx, std::uint32_t sz) {
        sx = std::min(sx, mesh.resolution - 1);
        sz = std::min(sz, mesh.resolution - 1);
        return mesh.sample(sx, sz);
    };
    const float dhdx = (sample(x + 1, z) - sample(x - 1, z)) / std::max(step * 2.0f, 0.001f);
    const float dhdz = (sample(x, z + 1) - sample(x, z - 1)) / std::max(step * 2.0f, 0.001f);
    return std::sqrt(dhdx * dhdx + dhdz * dhdz);
}

TerrainVertex vertex(float x, float y, float z, TerrainSurface surface, float minimum, float maximum,
    const std::optional<std::array<float, 3>>& painted_rgb) {
    const bool painted = painted_rgb.has_value();
    const auto base = painted_rgb ? *painted_rgb : surface_color(surface);
    const float range = std::max(maximum - minimum, 0.001f);
    const float t = std::clamp((y - minimum) / range, 0.0f, 1.0f);
    const float shade = 0.88f + 0.12f * t;
    return {x, y, z, base[0] * shade, base[1] * shade, base[2] * shade, painted};
}

std::optional<std::array<float, 3>> painted_sample_color(const TerrainPaintStore* paint, CellCoord cell,
    std::uint32_t x, std::uint32_t z,
    const std::function<const MaterialAsset*(const std::string&)>& lookup_material) {
    if (!paint || !lookup_material) return std::nullopt;
    const std::uint16_t index = paint->index_at(cell, x, z);
    if (index == 0) return std::nullopt;
    const std::string* path = paint->material_path_for_index(index);
    if (!path) return std::nullopt;
    const MaterialAsset* material = lookup_material(*path);
    if (!material) return std::nullopt;
    return std::array<float, 3>{material->base_color[0], material->base_color[1], material->base_color[2]};
}
} // namespace

float procedural_terrain_height(float world_x, float world_z) { return height_at(world_x, world_z); }

float sample_terrain_height(float world_x, float world_z) {
    const float base = height_at(world_x, world_z);
    if (const TerrainEditStore* edits = active_terrain_edits()) return base + edits->sample_delta_world(world_x, world_z);
    return base;
}

float TerrainMesh::sample(std::uint32_t x, std::uint32_t z) const {
    if (x >= resolution || z >= resolution || heights.size() != static_cast<std::size_t>(resolution) * resolution)
        return std::numeric_limits<float>::quiet_NaN();
    return heights[static_cast<std::size_t>(z) * resolution + x];
}

Result<TerrainMesh> generate_stylized_terrain(CellCoord coordinate, std::uint32_t resolution, float cell_size,
    const TerrainEditStore* edits, const TerrainPaintStore* paint,
    const std::function<const MaterialAsset*(const std::string&)>& lookup_material) {
    if (resolution < 3 || resolution > 257 || ((resolution - 1) & (resolution - 2)) != 0)
        return Result<TerrainMesh>::failure(terrain_error("TERRAIN-MESH-RESOLUTION", "Terrain mesh resolution must be 2^n+1"));
    if (!std::isfinite(cell_size) || cell_size <= 0.0f)
        return Result<TerrainMesh>::failure(terrain_error("TERRAIN-MESH-SIZE", "Terrain cell size must be finite and positive"));

    TerrainMesh mesh{coordinate, resolution, cell_size};
    mesh.heights.resize(static_cast<std::size_t>(resolution) * resolution);
    const float step = cell_size / static_cast<float>(resolution - 1);
    const float origin_x = static_cast<float>(coordinate.x) * cell_size - cell_size * 0.5f;
    const float origin_z = static_cast<float>(coordinate.z) * cell_size - cell_size * 0.5f;
    float minimum = std::numeric_limits<float>::max();
    float maximum = std::numeric_limits<float>::lowest();
    for (std::uint32_t z = 0; z < resolution; ++z) {
        for (std::uint32_t x = 0; x < resolution; ++x) {
            const float h = height_at(origin_x + x * step, origin_z + z * step) +
                (edits ? edits->delta_at(coordinate, x, z) : 0.0f);
            mesh.heights[static_cast<std::size_t>(z) * resolution + x] = h;
            minimum = std::min(minimum, h);
            maximum = std::max(maximum, h);
        }
    }
    mesh.triangles.reserve(static_cast<std::size_t>(resolution - 1) * (resolution - 1) * 6);
    for (std::uint32_t z = 0; z + 1 < resolution; ++z) {
        for (std::uint32_t x = 0; x + 1 < resolution; ++x) {
            const float x0 = origin_x + x * step, x1 = x0 + step;
            const float z0 = origin_z + z * step, z1 = z0 + step;
            const float h00 = mesh.sample(x, z), h10 = mesh.sample(x + 1, z);
            const float h01 = mesh.sample(x, z + 1), h11 = mesh.sample(x + 1, z + 1);
            const float cx = (x0 + x1) * 0.5f;
            const float cz = (z0 + z1) * 0.5f;
            const float center_h = (h00 + h01 + h10 + h11) * 0.25f;
            const float center_slope = (slope_at(mesh, x, z, step) + slope_at(mesh, x + 1, z, step) +
                                           slope_at(mesh, x, z + 1, step) + slope_at(mesh, x + 1, z + 1, step)) *
                                      0.25f;
            const auto surface = surface_at(cx, cz, center_h, center_slope);
            mesh.triangles.insert(mesh.triangles.end(), {
                vertex(x0, h00, z0, surface, minimum, maximum,
                    painted_sample_color(paint, coordinate, x, z, lookup_material)),
                vertex(x0, h01, z1, surface, minimum, maximum,
                    painted_sample_color(paint, coordinate, x, z + 1, lookup_material)),
                vertex(x1, h11, z1, surface, minimum, maximum,
                    painted_sample_color(paint, coordinate, x + 1, z + 1, lookup_material))});
            mesh.triangles.insert(mesh.triangles.end(), {
                vertex(x0, h00, z0, surface, minimum, maximum,
                    painted_sample_color(paint, coordinate, x, z, lookup_material)),
                vertex(x1, h11, z1, surface, minimum, maximum,
                    painted_sample_color(paint, coordinate, x + 1, z + 1, lookup_material)),
                vertex(x1, h10, z0, surface, minimum, maximum,
                    painted_sample_color(paint, coordinate, x + 1, z, lookup_material))});
        }
    }
    return Result<TerrainMesh>::success(std::move(mesh));
}

CellCoord terrain_cell_for_position(float x, float z, float cell_size) {
    return {static_cast<std::int32_t>(std::floor((x + cell_size * 0.5f) / cell_size)),
            static_cast<std::int32_t>(std::floor((z + cell_size * 0.5f) / cell_size))};
}

std::vector<CellCoord> terrain_cells_in_radius(CellCoord center, std::uint32_t radius) {
    std::vector<CellCoord> cells;
    if (radius > 32) return cells;
    const int extent = static_cast<int>(radius);
    cells.reserve(static_cast<std::size_t>((extent * 2 + 1) * (extent * 2 + 1)));
    for (int z = -extent; z <= extent; ++z) {
        for (int x = -extent; x <= extent; ++x) {
            cells.push_back({center.x + x, center.z + z});
        }
    }
    return cells;
}

} // namespace engine
