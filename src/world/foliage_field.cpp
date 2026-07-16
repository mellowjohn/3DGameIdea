#include "engine/world/foliage_field.h"

namespace engine {

Result<void> StreamedFoliageField::sync(const StreamedTerrainField& terrain,
    const std::array<float, 3>& camera_position) {
    if (!palette_ || !density_) {
        loaded_.clear();
        batches_.clear();
        dirty_ = false;
        return Result<void>::success();
    }
    const auto desired = terrain.loaded_cell_coordinates();
    std::set<CellCoord> unload;
    for (const auto& cell : loaded_) {
        if (desired.find(cell) == desired.end()) unload.insert(cell);
    }
    unload_cells(unload);
    bool changed = !unload.empty();
    for (const auto& cell : desired) {
        if (loaded_.insert(cell).second) changed = true;
    }
    if (changed) rebuild_batch_map(camera_position);
    return Result<void>::success();
}

Result<void> StreamedFoliageField::rebuild_cells(const std::set<CellCoord>& cells,
    const std::array<float, 3>& camera_position) {
    if (!palette_ || !density_ || cells.empty()) return Result<void>::success();
    for (const auto& cell : cells) loaded_.insert(cell);
    rebuild_batch_map(camera_position);
    return Result<void>::success();
}

void StreamedFoliageField::unload_cells(const std::set<CellCoord>& cells) {
    if (cells.empty()) return;
    for (const auto& cell : cells) loaded_.erase(cell);
    rebuild_batch_map({0.0f, 0.0f, 0.0f});
    dirty_ = true;
}

void StreamedFoliageField::rebuild_batch_map(const std::array<float, 3>& camera_position) {
    if (!palette_ || !density_) {
        batches_.clear();
        return;
    }
    batches_ = scatter_foliage_cells(loaded_, *density_, *palette_, scatter_config_, camera_position);
    dirty_ = true;
}

} // namespace engine
