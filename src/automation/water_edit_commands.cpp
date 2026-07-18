#include "engine/automation/water_edit_commands.h"

namespace engine {

WaterBrushStrokeCommand::WaterBrushStrokeCommand(std::map<CellCoord, WaterCellSnapshot> before,
    std::map<CellCoord, WaterCellSnapshot> after)
    : before_(std::move(before)), after_(std::move(after)) {}

Result<void> WaterBrushStrokeCommand::apply(WaterStore& store) {
    for (const auto& entry : after_) {
        if (entry.second.fill.empty())
            store.remove_cell(entry.first);
        else
            store.set_cell_fill(entry.first, entry.second.fill);
    }
    return Result<void>::success();
}

Result<void> WaterBrushStrokeCommand::revert(WaterStore& store) {
    for (const auto& entry : before_) {
        if (entry.second.fill.empty())
            store.remove_cell(entry.first);
        else
            store.set_cell_fill(entry.first, entry.second.fill);
    }
    return Result<void>::success();
}

Result<void> WaterEditHistory::execute(WaterStore& store, std::unique_ptr<WaterEditCommand> command) {
    if (!command) return Result<void>::failure({{"WATER-HISTORY-EMPTY", Severity::Error, ErrorCategory::Validation,
        "water-history", "Water history command is null", ENGINE_SOURCE_CONTEXT, {}, "Provide a command.", make_correlation_id()}});
    const auto applied = command->apply(store);
    if (!applied) return applied;
    undo_.push_back(std::move(command));
    redo_.clear();
    last_summary_ = undo_.back()->label();
    return Result<void>::success();
}

Result<void> WaterEditHistory::undo(WaterStore& store) {
    if (undo_.empty())
        return Result<void>::failure({{"WATER-UNDO-EMPTY", Severity::Error, ErrorCategory::Validation, "water-history",
            "Nothing to undo", ENGINE_SOURCE_CONTEXT, {}, "Place water before undo.", make_correlation_id()}});
    auto command = std::move(undo_.back());
    undo_.pop_back();
    const auto reverted = command->revert(store);
    if (!reverted) {
        undo_.push_back(std::move(command));
        return reverted;
    }
    last_summary_ = command->label();
    redo_.push_back(std::move(command));
    return Result<void>::success();
}

Result<void> WaterEditHistory::redo(WaterStore& store) {
    if (redo_.empty())
        return Result<void>::failure({{"WATER-REDO-EMPTY", Severity::Error, ErrorCategory::Validation, "water-history",
            "Nothing to redo", ENGINE_SOURCE_CONTEXT, {}, "Undo before redo.", make_correlation_id()}});
    auto command = std::move(redo_.back());
    redo_.pop_back();
    const auto applied = command->apply(store);
    if (!applied) {
        redo_.push_back(std::move(command));
        return applied;
    }
    last_summary_ = command->label();
    undo_.push_back(std::move(command));
    return Result<void>::success();
}

} // namespace engine
