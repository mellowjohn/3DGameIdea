#include "engine/world/terrain_field.h"

#include "engine/world/terrain_edits.h"

namespace engine {

Result<void> StreamedTerrainField::update(CollisionWorld& world, const std::array<float, 3>& camera_position,
    const PhysicalMaterialProperties& physics, std::uint32_t radius, const TerrainEditStore* edits,
    const TerrainPaintStore* paint, const TerrainPaintMaterialLookup& lookup_material) {
    const auto center = terrain_cell_for_position(camera_position[0], camera_position[2], k_cell_size);
    const auto wanted = terrain_cells_in_radius(center, radius);
    const std::set<CellCoord> wanted_set(wanted.begin(), wanted.end());
    if (center.x == focus_.x && center.z == focus_.z && wanted_set == desired_) return Result<void>::success();

    for (auto it = meshes_.begin(); it != meshes_.end();) {
        if (wanted_set.find(it->first) == wanted_set.end()) {
            world.unload_cell(it->first);
            it = meshes_.erase(it);
        } else {
            ++it;
        }
    }

    for (const auto& cell : wanted_set) {
        if (meshes_.find(cell) != meshes_.end()) continue;
        auto terrain = generate_stylized_terrain(cell, k_resolution, k_cell_size, edits, paint, lookup_material);
        if (!terrain) return Result<void>::failure(terrain.error());
        const auto body = world.add_heightfield(terrain.value(), physics, cell);
        if (!body) return Result<void>::failure(body.error());
        meshes_.emplace(cell, std::move(terrain.value()));
    }

    focus_ = center;
    desired_ = wanted_set;
    render_data_dirty_ = true;
    return Result<void>::success();
}

std::set<CellCoord> StreamedTerrainField::loaded_cell_coordinates() const {
    std::set<CellCoord> cells;
    for (const auto& entry : meshes_) cells.insert(entry.first);
    return cells;
}

Result<void> StreamedTerrainField::reload_cells(CollisionWorld& world, const std::set<CellCoord>& cells,
    const PhysicalMaterialProperties& physics, const TerrainEditStore* edits, const TerrainPaintStore* paint,
    const TerrainPaintMaterialLookup& lookup_material) {
    for (const auto& cell : cells) {
        const auto found = meshes_.find(cell);
        if (found == meshes_.end()) continue;
        world.unload_cell(cell);
        auto terrain = generate_stylized_terrain(cell, k_resolution, k_cell_size, edits, paint, lookup_material);
        if (!terrain) return Result<void>::failure(terrain.error());
        const auto body = world.add_heightfield(terrain.value(), physics, cell);
        if (!body) return Result<void>::failure(body.error());
        found->second = std::move(terrain.value());
    }
    if (!cells.empty()) render_data_dirty_ = true;
    return Result<void>::success();
}

Result<void> StreamedTerrainField::reload_cell_meshes(const std::set<CellCoord>& cells, const TerrainEditStore* edits,
    const TerrainPaintStore* paint, const TerrainPaintMaterialLookup& lookup_material) {
    for (const auto& cell : cells) {
        const auto found = meshes_.find(cell);
        if (found == meshes_.end()) continue;
        auto terrain = generate_stylized_terrain(cell, k_resolution, k_cell_size, edits, paint, lookup_material);
        if (!terrain) return Result<void>::failure(terrain.error());
        found->second = std::move(terrain.value());
    }
    if (!cells.empty()) render_data_dirty_ = true;
    return Result<void>::success();
}

std::vector<TerrainRenderVertex> StreamedTerrainField::build_render_vertices(
    const std::array<float, 4>& base_color) const {
    std::vector<TerrainRenderVertex> vertices;
    std::size_t total = 0;
    for (const auto& entry : meshes_) total += entry.second.triangles.size();
    vertices.reserve(total);
    for (const auto& entry : meshes_) {
        for (const auto& triangle : entry.second.triangles) {
            if (triangle.painted) {
                vertices.push_back(
                    {triangle.x, triangle.y, triangle.z, triangle.r, triangle.g, triangle.b});
            } else {
                vertices.push_back({triangle.x, triangle.y, triangle.z, triangle.r * base_color[0],
                    triangle.g * base_color[1], triangle.b * base_color[2]});
            }
        }
    }
    return vertices;
}

} // namespace engine
