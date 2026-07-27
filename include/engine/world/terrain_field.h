#pragma once

#include "engine/assets/material_asset.h"
#include "engine/core/error.h"
#include "engine/physics/collision_world.h"
#include "engine/world/terrain.h"
#include "engine/world/terrain_paint.h"

#include <array>
#include <cstdint>
#include <future>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace engine {

struct TerrainRenderVertex {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

/** Streaming knobs for hitch reduction (amortized loads + optional view bias). */
struct TerrainStreamParams {
    std::uint32_t radius = 2;
    /** Full disc kept under each focus for collision. Loaded before unload; may be amortized when overlapping. */
    std::uint32_t support_radius = 1;
    /** Cap newly generated *outer-ring* cells per update.
     *  Use 0 to block outer loads (look-gate). Use SIZE_MAX for uncapped — do not treat 0 as unlimited. */
    std::size_t max_new_cells = std::numeric_limits<std::size_t>::max();
    /**
     * Cap newly generated *support* cells per update when the focus cell is already resident (walk fringe).
     * Bootstrap / teleport (focus cell missing) ignores this and loads the full support disc immediately.
     * Use 0 to pause support fringe loads (keep prior cells; no unload until support catches up).
     */
    std::size_t max_new_support_cells = std::numeric_limits<std::size_t>::max();
    /**
     * Cap completed async terrain jobs committed on the main/render thread per update.
     * Generation is off-thread, but heightfield creation, foliage scatter, and GPU uploads follow a commit;
     * without this cap, several jobs that finish together produce one traversal hitch.
     */
    std::size_t max_ready_commits = std::numeric_limits<std::size_t>::max();
    /** Generate scheduled streaming cells off the render thread; collision commit remains on the main thread. */
    bool async_generation = false;
    float forward_x = 0.0f;
    float forward_z = 0.0f;
    bool view_bias = false;
};

/** Freeze view-bias forward while camera yaw is changing (look-around hitch reduction). */
struct StreamViewBiasGate {
    float last_yaw = 0.0f;
    float stable_forward_x = 0.0f;
    float stable_forward_z = 1.0f;
    int hot_frames_remaining = 0;
    bool initialized = false;
};

/**
 * While |Δyaw| > yaw_hot_radians, keep last stable forward on `params` and freeze stream hitch work:
 * `max_new_cells=0`, `max_new_support_cells=0`, `max_ready_commits=0`. Already-finished async cells
 * must not commit mid-look (heightfield + foliage/GPU follow a commit and hitch). The default
 * threshold is deliberately below one mouse pixel at the default 0.003 rad/pixel sensitivity; slow
 * turns must be gated too. Bootstrap/teleport still fills a missing focus support disc immediately
 * (support budget is ignored when the focus cell is absent). Returns true when look-hot.
 */
bool apply_stream_view_bias_look_gate(StreamViewBiasGate& gate, float yaw_radians, float look_forward_x,
    float look_forward_z, TerrainStreamParams& params, float yaw_hot_radians = 0.0005f,
    int settle_frames = 12);

class StreamedTerrainField final {
public:
    static constexpr std::uint32_t k_resolution = k_default_terrain_resolution;
    static constexpr float k_cell_size = k_default_terrain_cell_size;
    static constexpr std::uint32_t k_default_radius = 2;
    /** Runtime hitch budget: generate at most this many outer-ring cells per frame. */
    static constexpr std::size_t k_default_max_new_cells_per_update = 2;
    /** Runtime hitch budget: generate at most this many support fringe cells per walk frame. */
    static constexpr std::size_t k_default_max_new_support_cells_per_update = 1;
    /** Runtime hitch budget: commit at most one completed async cell per frame. */
    static constexpr std::size_t k_default_max_ready_commits_per_update = 1;

    [[nodiscard]] Result<void> update(CollisionWorld& world, const std::array<float, 3>& camera_position,
        const PhysicalMaterialProperties& physics, std::uint32_t radius = k_default_radius,
        const TerrainEditStore* edits = nullptr, const TerrainPaintStore* paint = nullptr,
        const TerrainPaintMaterialLookup& lookup_material = {});
    /** Union streaming neighborhoods around every focus (e.g. host + guest in local co-op). */
    [[nodiscard]] Result<void> update(CollisionWorld& world, const std::vector<std::array<float, 3>>& focus_positions,
        const PhysicalMaterialProperties& physics, std::uint32_t radius = k_default_radius,
        const TerrainEditStore* edits = nullptr, const TerrainPaintStore* paint = nullptr,
        const TerrainPaintMaterialLookup& lookup_material = {});
    [[nodiscard]] Result<void> update(CollisionWorld& world, const std::vector<std::array<float, 3>>& focus_positions,
        const PhysicalMaterialProperties& physics, const TerrainStreamParams& params,
        const TerrainEditStore* edits = nullptr, const TerrainPaintStore* paint = nullptr,
        const TerrainPaintMaterialLookup& lookup_material = {});
    [[nodiscard]] Result<void> reload_cells(CollisionWorld& world, const std::set<CellCoord>& cells,
        const PhysicalMaterialProperties& physics, const TerrainEditStore* edits = nullptr,
        const TerrainPaintStore* paint = nullptr, const TerrainPaintMaterialLookup& lookup_material = {});
    [[nodiscard]] Result<void> reload_cell_meshes(const std::set<CellCoord>& cells, const TerrainEditStore* edits = nullptr,
        const TerrainPaintStore* paint = nullptr, const TerrainPaintMaterialLookup& lookup_material = {});
    [[nodiscard]] std::vector<TerrainRenderVertex> build_render_vertices(
        const std::array<float, 4>& base_color) const;
    [[nodiscard]] std::vector<TerrainRenderVertex> build_cell_render_vertices(CellCoord cell,
        const std::array<float, 4>& base_color) const;
    [[nodiscard]] std::size_t loaded_cell_count() const noexcept { return cells_.size(); }
    [[nodiscard]] std::set<CellCoord> loaded_cell_coordinates() const;
    [[nodiscard]] bool contains(CellCoord cell) const { return cells_.find(cell) != cells_.end(); }
    [[nodiscard]] CellCoord focus_cell() const noexcept { return focus_; }
    [[nodiscard]] bool stream_pending() const noexcept {
        return !pending_cells_.empty() || !pending_generations_.empty();
    }
    [[nodiscard]] bool render_data_dirty() const noexcept {
        return !render_dirty_cells_.empty() || !render_removed_cells_.empty();
    }
    /** Take dirty cells for GPU upload. `max_cells` amortizes hitch (remaining stay dirty). */
    [[nodiscard]] std::set<CellCoord> take_render_dirty_cells(
        std::size_t max_cells = std::numeric_limits<std::size_t>::max());
    [[nodiscard]] std::set<CellCoord> take_render_removed_cells();
    /** Runtime hitch budget: upload at most one dirty terrain cell mesh per frame. */
    static constexpr std::size_t k_default_max_render_uploads_per_update = 1;
    void clear_render_data_dirty() noexcept {
        render_dirty_cells_.clear();
        render_removed_cells_.clear();
    }
    void mark_render_data_dirty() noexcept {
        for (const auto& entry : cells_) render_dirty_cells_.insert(entry.first);
    }

private:
    struct LoadedCell {
        TerrainMesh mesh;
        CollisionBody body{};
    };

    /** Default-constructible carrier so MSVC std::future does not require Result<> default ctor. */
    struct AsyncTerrainMesh {
        std::optional<TerrainMesh> mesh;
        std::optional<EngineError> error;
    };

    [[nodiscard]] Result<void> load_cell(CollisionWorld& world, CellCoord cell,
        const PhysicalMaterialProperties& physics, const TerrainEditStore* edits, const TerrainPaintStore* paint,
        const TerrainPaintMaterialLookup& lookup_material);
    [[nodiscard]] Result<void> commit_cell(CollisionWorld& world, TerrainMesh mesh,
        const PhysicalMaterialProperties& physics);
    [[nodiscard]] Result<void> queue_cell_generation(CellCoord cell, const TerrainEditStore* edits,
        const TerrainPaintStore* paint, const TerrainPaintMaterialLookup& lookup_material);
    [[nodiscard]] Result<void> commit_ready_generations(CollisionWorld& world,
        const PhysicalMaterialProperties& physics, const std::set<CellCoord>& wanted, std::size_t max_commits);
    [[nodiscard]] Result<void> commit_generation_now(CollisionWorld& world, CellCoord cell,
        const PhysicalMaterialProperties& physics);
    [[nodiscard]] bool generation_pending(CellCoord cell) const;

    std::map<CellCoord, LoadedCell> cells_;
    struct PendingGeneration {
        std::future<AsyncTerrainMesh> future;
    };
    std::map<CellCoord, PendingGeneration> pending_generations_;
    std::set<CellCoord> desired_;
    std::set<CellCoord> pending_cells_;
    std::set<CellCoord> render_dirty_cells_;
    std::set<CellCoord> render_removed_cells_;
    CellCoord focus_{};
};

} // namespace engine
