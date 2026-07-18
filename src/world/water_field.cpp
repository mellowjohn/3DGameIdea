#include "engine/world/water_field.h"

#include "engine/world/terrain.h"

namespace engine {

StreamedWaterField::WaterCellMesh StreamedWaterField::build_cell_mesh(CellCoord cell, const WaterStore* store) const {
    WaterCellMesh mesh;
    if (!store) return mesh;
    const auto* mask = store->find_cell(cell);
    const float origin_x = static_cast<float>(cell.x) * k_cell_size - k_cell_size * 0.5f;
    const float origin_z = static_cast<float>(cell.z) * k_cell_size - k_cell_size * 0.5f;
    const float step = k_cell_size / static_cast<float>(k_resolution - 1);
    const float sea = store->sea_level();

    const auto sample_has_water = [&](std::uint32_t x, std::uint32_t z) -> bool {
        const float wx = origin_x + static_cast<float>(x) * step;
        const float wz = origin_z + static_cast<float>(z) * step;
        if (mask) {
            const std::uint8_t fill = mask->fill[static_cast<std::size_t>(z) * k_resolution + x];
            if (fill >= WaterStore::k_fill_threshold) return true;
        }
        return store->in_sea_region(wx, wz) && sample_terrain_height(wx, wz) < sea;
    };

    const auto push_vertex = [&](float x, float z) {
        mesh.vertices.push_back({x, sea, z, 0.08f, 0.22f, 0.35f});
    };

    for (std::uint32_t z = 0; z + 1 < k_resolution; ++z) {
        for (std::uint32_t x = 0; x + 1 < k_resolution; ++x) {
            const bool q00 = sample_has_water(x, z);
            const bool q10 = sample_has_water(x + 1, z);
            const bool q01 = sample_has_water(x, z + 1);
            const bool q11 = sample_has_water(x + 1, z + 1);
            if (!q00 && !q10 && !q01 && !q11) continue;
            const float x0 = origin_x + static_cast<float>(x) * step;
            const float x1 = origin_x + static_cast<float>(x + 1) * step;
            const float z0 = origin_z + static_cast<float>(z) * step;
            const float z1 = origin_z + static_cast<float>(z + 1) * step;
            if (q00 && q10 && q11) {
                push_vertex(x0, z0);
                push_vertex(x1, z0);
                push_vertex(x1, z1);
            }
            if (q00 && q11 && q01) {
                push_vertex(x0, z0);
                push_vertex(x1, z1);
                push_vertex(x0, z1);
            }
            if (q00 && q10 && q01 && !q11) {
                push_vertex(x0, z0);
                push_vertex(x1, z0);
                push_vertex(x0, z1);
            }
            if (q10 && q11 && q01 && !q00) {
                push_vertex(x1, z0);
                push_vertex(x1, z1);
                push_vertex(x0, z1);
            }
        }
    }
    return mesh;
}

Result<void> StreamedWaterField::update(const std::array<float, 3>& camera_position, std::uint32_t radius,
    const WaterStore* store) {
    const auto center = terrain_cell_for_position(camera_position[0], camera_position[2], k_cell_size);
    const auto wanted = terrain_cells_in_radius(center, radius);
    const std::set<CellCoord> wanted_set(wanted.begin(), wanted.end());
    if (center.x == focus_.x && center.z == focus_.z && wanted_set == desired_) return Result<void>::success();

    for (auto it = meshes_.begin(); it != meshes_.end();) {
        if (wanted_set.find(it->first) == wanted_set.end())
            it = meshes_.erase(it);
        else
            ++it;
    }

    for (const auto& cell : wanted_set) {
        if (meshes_.find(cell) != meshes_.end()) continue;
        meshes_.emplace(cell, build_cell_mesh(cell, store));
    }

    focus_ = center;
    desired_ = wanted_set;
    render_data_dirty_ = true;
    return Result<void>::success();
}

Result<void> StreamedWaterField::reload_cells(const std::set<CellCoord>& cells, const WaterStore* store) {
    for (const auto& cell : cells) {
        const auto found = meshes_.find(cell);
        if (found == meshes_.end()) continue;
        found->second = build_cell_mesh(cell, store);
    }
    if (!cells.empty()) render_data_dirty_ = true;
    return Result<void>::success();
}

std::set<CellCoord> StreamedWaterField::loaded_cell_coordinates() const {
    std::set<CellCoord> cells;
    for (const auto& entry : meshes_) cells.insert(entry.first);
    return cells;
}

std::vector<WaterRenderVertex> StreamedWaterField::build_render_vertices() const {
    std::vector<WaterRenderVertex> vertices;
    std::size_t total = 0;
    for (const auto& entry : meshes_) total += entry.second.vertices.size();
    vertices.reserve(total);
    for (const auto& entry : meshes_) {
        vertices.insert(vertices.end(), entry.second.vertices.begin(), entry.second.vertices.end());
    }
    return vertices;
}

} // namespace engine
