#pragma once

#include "engine/world/foliage_density.h"
#include "engine/world/foliage_layers.h"
#include "engine/world/foliage_scatter.h"
#include "engine/world/terrain_field.h"

#include <array>
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

    [[nodiscard]] Result<void> sync(const StreamedTerrainField& terrain, const std::array<float, 3>& camera_position);
    [[nodiscard]] Result<void> rebuild_cells(const std::set<CellCoord>& cells, const std::array<float, 3>& camera_position);
    void unload_cells(const std::set<CellCoord>& cells);

    [[nodiscard]] const std::map<std::string, std::vector<FoliageInstance>>& batches() const noexcept { return batches_; }
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }
    void clear_dirty() noexcept { dirty_ = false; }

private:
    const FoliageLayerPalette* palette_ = nullptr;
    const FoliageDensityStore* density_ = nullptr;
    FoliageScatterConfig scatter_config_{};
    std::set<CellCoord> loaded_;
    std::map<std::string, std::vector<FoliageInstance>> batches_;
    bool dirty_ = false;

    void rebuild_batch_map(const std::array<float, 3>& camera_position);
};

} // namespace engine
