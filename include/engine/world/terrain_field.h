#pragma once

#include "engine/assets/material_asset.h"
#include "engine/physics/collision_world.h"
#include "engine/world/terrain.h"
#include "engine/world/terrain_paint.h"

#include <array>
#include <cstdint>
#include <map>
#include <set>

namespace engine {

struct TerrainRenderVertex {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

class StreamedTerrainField final {
public:
    static constexpr std::uint32_t k_resolution = k_default_terrain_resolution;
    static constexpr float k_cell_size = k_default_terrain_cell_size;
    static constexpr std::uint32_t k_default_radius = 2;

    [[nodiscard]] Result<void> update(CollisionWorld& world, const std::array<float, 3>& camera_position,
        const PhysicalMaterialProperties& physics, std::uint32_t radius = k_default_radius,
        const TerrainEditStore* edits = nullptr, const TerrainPaintStore* paint = nullptr,
        const TerrainPaintMaterialLookup& lookup_material = {});
    [[nodiscard]] Result<void> reload_cells(CollisionWorld& world, const std::set<CellCoord>& cells,
        const PhysicalMaterialProperties& physics, const TerrainEditStore* edits = nullptr,
        const TerrainPaintStore* paint = nullptr, const TerrainPaintMaterialLookup& lookup_material = {});
    [[nodiscard]] Result<void> reload_cell_meshes(const std::set<CellCoord>& cells, const TerrainEditStore* edits = nullptr,
        const TerrainPaintStore* paint = nullptr, const TerrainPaintMaterialLookup& lookup_material = {});
    [[nodiscard]] std::vector<TerrainRenderVertex> build_render_vertices(
        const std::array<float, 4>& base_color) const;
    [[nodiscard]] std::size_t loaded_cell_count() const noexcept { return meshes_.size(); }
    [[nodiscard]] std::set<CellCoord> loaded_cell_coordinates() const;
    [[nodiscard]] bool contains(CellCoord cell) const { return meshes_.find(cell) != meshes_.end(); }
    [[nodiscard]] CellCoord focus_cell() const noexcept { return focus_; }
    [[nodiscard]] bool render_data_dirty() const noexcept { return render_data_dirty_; }
    void clear_render_data_dirty() noexcept { render_data_dirty_ = false; }
    void mark_render_data_dirty() noexcept { render_data_dirty_ = true; }

private:
    std::map<CellCoord, TerrainMesh> meshes_;
    std::set<CellCoord> desired_;
    CellCoord focus_{};
    bool render_data_dirty_ = true;
};

} // namespace engine
