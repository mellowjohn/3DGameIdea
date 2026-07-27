#pragma once

#include "engine/world/foliage_density.h"
#include "engine/world/foliage_layers.h"
#include "engine/world/foliage_scatter.h"
#include "engine/world/terrain_field.h"

#include <array>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace engine {

class StreamedFoliageField final {
public:
    void set_palette(const FoliageLayerPalette* palette) noexcept { palette_ = palette; }
    void set_density(const FoliageDensityStore* density) noexcept { density_ = density; }
    void set_scatter_config(const FoliageScatterConfig& config) noexcept { scatter_config_ = config; }

    /**
     * Align resident foliage cells with terrain. Unloads always apply; new-cell scatter is capped by
     * `max_new_cells` so a terrain commit frame does not also pay full scatter for every catch-up cell.
     */
    [[nodiscard]] Result<void> sync(const StreamedTerrainField& terrain, const std::array<float, 3>& camera_position,
        std::size_t max_new_cells = std::numeric_limits<std::size_t>::max());
    /** Runtime hitch budget: scatter at most one new foliage cell per frame while streaming. */
    static constexpr std::size_t k_default_max_new_cells_per_sync = 1;
    [[nodiscard]] Result<void> rebuild_cells(const std::set<CellCoord>& cells, const std::array<float, 3>& camera_position);
    void unload_cells(const std::set<CellCoord>& cells);

    /**
     * Merged per-mesh batches, rebuilt lazily on access. The renderer consumes cell_instances()
     * directly, so streaming commits must not pay a full instance copy per changed cell here.
     */
    [[nodiscard]] const std::map<std::string, std::vector<FoliageInstance>>& batches() const {
        if (batches_stale_) rebuild_batch_map();
        return batches_;
    }
    [[nodiscard]] bool batches_stale() const noexcept { return batches_stale_; }
    [[nodiscard]] const std::map<CellCoord, std::vector<FoliageInstance>>& cell_instances() const noexcept {
        return cell_instances_;
    }
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }
    void clear_dirty() noexcept { dirty_ = false; }
    /** Monotonic counter bumped whenever resident foliage content changes. */
    [[nodiscard]] std::uint64_t change_revision() const noexcept { return change_revision_; }

private:
    const FoliageLayerPalette* palette_ = nullptr;
    const FoliageDensityStore* density_ = nullptr;
    FoliageScatterConfig scatter_config_{};
    std::set<CellCoord> loaded_;
    std::map<CellCoord, std::vector<FoliageInstance>> cell_instances_;
    mutable std::map<std::string, std::vector<FoliageInstance>> batches_;
    mutable bool batches_stale_ = false;
    bool dirty_ = false;
    std::uint64_t change_revision_ = 0;

    void mark_changed() noexcept;
    void rebuild_batch_map() const;
};

} // namespace engine
