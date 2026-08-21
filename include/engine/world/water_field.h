#pragma once

#include "engine/world/water_store.h"

#include <array>
#include <cstdint>
#include <limits>
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
    /// Vertical column depth (seaLevel - terrain) in meters; drives absorption in the water pass.
    float depth = 0.0f;
};

class StreamedWaterField final {
public:
    static constexpr std::uint32_t k_resolution = WaterStore::k_resolution;
    static constexpr float k_cell_size = WaterStore::k_cell_size;
    static constexpr std::uint32_t k_default_radius = 4;
    /** Match `StreamedTerrainField::k_editor_view_radius` for Scene/menu previews. */
    static constexpr std::uint32_t k_editor_view_radius = 6;
    /** Runtime hitch budget: build at most one new water cell mesh per update while streaming. */
    static constexpr std::size_t k_default_max_new_cells_per_update = 1;
    /** Runtime hitch budget: upload at most one dirty water cell mesh per frame. */
    static constexpr std::size_t k_default_max_render_uploads_per_update = 1;

    /**
     * Align resident water meshes with the camera neighborhood. Unloads apply immediately; new-cell
     * mesh builds are capped by `max_new_cells` so crossing a cell boundary does not rebuild a full
     * ring of shoreline meshes in one frame.
     */
    [[nodiscard]] Result<void> update(const std::array<float, 3>& camera_position, std::uint32_t radius = k_default_radius,
        const WaterStore* store = nullptr,
        std::size_t max_new_cells = k_default_max_new_cells_per_update);
    [[nodiscard]] Result<void> reload_cells(const std::set<CellCoord>& cells, const WaterStore* store = nullptr);
    [[nodiscard]] std::vector<WaterRenderVertex> build_render_vertices() const;
    [[nodiscard]] std::vector<WaterRenderVertex> build_cell_render_vertices(CellCoord cell) const;
    [[nodiscard]] std::size_t loaded_cell_count() const noexcept { return meshes_.size(); }
    [[nodiscard]] std::set<CellCoord> loaded_cell_coordinates() const;
    /** True when the desired stream set still has cells without resident meshes. */
    [[nodiscard]] bool stream_pending() const noexcept;
    [[nodiscard]] bool render_data_dirty() const noexcept {
        return !render_dirty_cells_.empty() || !render_removed_cells_.empty();
    }
    /** Take dirty cells for GPU upload. `max_cells` amortizes hitch (remaining stay dirty). */
    [[nodiscard]] std::set<CellCoord> take_render_dirty_cells(
        std::size_t max_cells = std::numeric_limits<std::size_t>::max());
    [[nodiscard]] std::set<CellCoord> take_render_removed_cells();
    void clear_render_data_dirty() noexcept {
        render_dirty_cells_.clear();
        render_removed_cells_.clear();
    }
    void mark_render_data_dirty() noexcept {
        for (const auto& entry : meshes_) render_dirty_cells_.insert(entry.first);
    }

private:
    struct WaterCellMesh {
        std::vector<WaterRenderVertex> vertices;
    };

    std::map<CellCoord, WaterCellMesh> meshes_;
    std::set<CellCoord> desired_;
    std::set<CellCoord> render_dirty_cells_;
    std::set<CellCoord> render_removed_cells_;
    CellCoord focus_{};

    [[nodiscard]] WaterCellMesh build_cell_mesh(CellCoord cell, const WaterStore* store) const;
};

} // namespace engine
