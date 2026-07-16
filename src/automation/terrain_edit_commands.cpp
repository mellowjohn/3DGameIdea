#include "engine/automation/terrain_edit_commands.h"

namespace engine {

TerrainBrushStrokeCommand::TerrainBrushStrokeCommand(std::map<CellCoord, std::vector<float>> before,
    std::map<CellCoord, std::vector<float>> after)
    : before_(std::move(before)), after_(std::move(after)) {}

Result<void> TerrainBrushStrokeCommand::apply(TerrainEditStore& store) {
    for (const auto& entry : after_) store.set_cell_deltas(entry.first, entry.second);
    for (const auto& entry : before_) {
        if (after_.find(entry.first) == after_.end()) store.remove_cell(entry.first);
    }
    return Result<void>::success();
}

Result<void> TerrainBrushStrokeCommand::revert(TerrainEditStore& store) {
    for (const auto& entry : before_) store.set_cell_deltas(entry.first, entry.second);
    for (const auto& entry : after_) {
        if (before_.find(entry.first) == before_.end()) store.remove_cell(entry.first);
    }
    return Result<void>::success();
}

Result<void> TerrainEditHistory::execute(TerrainEditStore& store, std::unique_ptr<TerrainEditCommand> command) {
    const auto applied = command->apply(store);
    if (!applied) return applied;
    undo_.push_back(std::move(command));
    redo_.clear();
    last_summary_ = undo_.back()->label();
    return Result<void>::success();
}

Result<void> TerrainEditHistory::undo(TerrainEditStore& store) {
    if (undo_.empty())
        return Result<void>::failure(
            EngineError{"TERRAIN-EDIT-UNDO-EMPTY", Severity::Error, ErrorCategory::Validation, "terrain-edits",
                "No terrain edit to undo", ENGINE_SOURCE_CONTEXT, {}, "Apply a terrain edit first.",
                make_correlation_id()});
    auto command = std::move(undo_.back());
    undo_.pop_back();
    const auto reverted = command->revert(store);
    if (!reverted) {
        undo_.push_back(std::move(command));
        return reverted;
    }
    redo_.push_back(std::move(command));
    last_summary_ = "undo-" + redo_.back()->label();
    return Result<void>::success();
}

Result<void> TerrainEditHistory::redo(TerrainEditStore& store) {
    if (redo_.empty())
        return Result<void>::failure(
            EngineError{"TERRAIN-EDIT-REDO-EMPTY", Severity::Error, ErrorCategory::Validation, "terrain-edits",
                "No terrain edit to redo", ENGINE_SOURCE_CONTEXT, {}, "Undo a terrain edit first.",
                make_correlation_id()});
    auto command = std::move(redo_.back());
    redo_.pop_back();
    const auto applied = command->apply(store);
    if (!applied) {
        redo_.push_back(std::move(command));
        return applied;
    }
    undo_.push_back(std::move(command));
    last_summary_ = undo_.back()->label();
    return Result<void>::success();
}

} // namespace engine
