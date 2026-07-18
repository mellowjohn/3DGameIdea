#pragma once

#include "engine/world/water_store.h"

#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace engine {

struct WaterRenderVertex {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float r = 0.08f;
    float g = 0.22f;
    float b = 0.35f;
};

class StreamedWaterField final {
public:
    static constexpr std::uint32_t k_resolution = WaterStore::k_resolution;
    static constexpr float k_cell_size = WaterStore::k_cell_size;
    static constexpr std::uint32_t k_default_radius = 2;

    [[nodiscard]] Result<void> update(const std::array<float, 3>& camera_position, std::uint32_t radius = k_default_radius,
        const WaterStore* store = nullptr);
    [[nodiscard]] Result<void> reload_cells(const std::set<CellCoord>& cells, const WaterStore* store = nullptr);
    [[nodiscard]] std::vector<WaterRenderVertex> build_render_vertices() const;
    [[nodiscard]] std::size_t loaded_cell_count() const noexcept { return meshes_.size(); }
    [[nodiscard]] std::set<CellCoord> loaded_cell_coordinates() const;
    [[nodiscard]] bool render_data_dirty() const noexcept { return render_data_dirty_; }
    void clear_render_data_dirty() noexcept { render_data_dirty_ = false; }
    void mark_render_data_dirty() noexcept { render_data_dirty_ = true; }

private:
    struct WaterCellMesh {
        std::vector<WaterRenderVertex> vertices;
    };

    std::map<CellCoord, WaterCellMesh> meshes_;
    std::set<CellCoord> desired_;
    CellCoord focus_{};
    bool render_data_dirty_ = true;

    [[nodiscard]] WaterCellMesh build_cell_mesh(CellCoord cell, const WaterStore* store) const;
};

} // namespace engine
