#include "engine/world/foliage_field.h"

namespace engine {

Result<void> StreamedFoliageField::sync(const StreamedTerrainField& terrain,
    const std::array<float, 3>& camera_position, std::size_t max_new_cells) {
    if (!palette_ || !density_) {
        loaded_.clear();
        cell_instances_.clear();
        batches_.clear();
        batches_stale_ = false;
        dirty_ = false;
        return Result<void>::success();
    }
    const auto desired = terrain.loaded_cell_coordinates();
    std::set<CellCoord> unload;
    for (const auto& cell : loaded_) {
        if (desired.find(cell) == desired.end()) unload.insert(cell);
    }
    bool changed = !unload.empty();
    for (const auto& cell : unload) {
        loaded_.erase(cell);
        cell_instances_.erase(cell);
    }

    std::vector<CellCoord> missing;
    for (const auto& cell : desired) {
        if (loaded_.find(cell) == loaded_.end()) missing.push_back(cell);
    }
    std::size_t scattered = 0;
    for (const auto& cell : missing) {
        if (scattered >= max_new_cells) break;
        loaded_.insert(cell);
        cell_instances_[cell] =
            scatter_foliage_cell(cell, *density_, *palette_, scatter_config_, camera_position);
        ++scattered;
        changed = true;
    }
    if (changed) mark_changed();
    return Result<void>::success();
}

Result<void> StreamedFoliageField::rebuild_cells(const std::set<CellCoord>& cells,
    const std::array<float, 3>& camera_position) {
    if (!palette_ || !density_ || cells.empty()) return Result<void>::success();
    for (const auto& cell : cells) {
        loaded_.insert(cell);
        cell_instances_[cell] = scatter_foliage_cell(cell, *density_, *palette_, scatter_config_, camera_position);
    }
    mark_changed();
    return Result<void>::success();
}

void StreamedFoliageField::unload_cells(const std::set<CellCoord>& cells) {
    if (cells.empty()) return;
    for (const auto& cell : cells) {
        loaded_.erase(cell);
        cell_instances_.erase(cell);
    }
    mark_changed();
}

void StreamedFoliageField::mark_changed() noexcept {
    batches_stale_ = true;
    dirty_ = true;
    ++change_revision_;
}

void StreamedFoliageField::rebuild_batch_map() const {
    batches_.clear();
    batches_stale_ = false;
    if (!palette_) return;
    for (const auto& entry : cell_instances_) {
        for (const auto& instance : entry.second) {
            const auto mesh_key = palette_->mesh_key_for_layer(instance.layer_index);
            if (mesh_key.empty()) continue;
            batches_[mesh_key].push_back(instance);
        }
    }
}

} // namespace engine
