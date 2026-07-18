#pragma once

#include "engine/world/water_store.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace engine {

struct WaterCellSnapshot {
    std::vector<std::uint8_t> fill;
};

class WaterEditCommand {
public:
    virtual ~WaterEditCommand() = default;
    [[nodiscard]] virtual Result<void> apply(WaterStore& store) = 0;
    [[nodiscard]] virtual Result<void> revert(WaterStore& store) = 0;
    [[nodiscard]] virtual std::string label() const = 0;
};

class WaterBrushStrokeCommand final : public WaterEditCommand {
public:
    WaterBrushStrokeCommand(std::map<CellCoord, WaterCellSnapshot> before, std::map<CellCoord, WaterCellSnapshot> after);
    Result<void> apply(WaterStore& store) override;
    Result<void> revert(WaterStore& store) override;
    std::string label() const override { return "water-brush-stroke"; }

private:
    std::map<CellCoord, WaterCellSnapshot> before_;
    std::map<CellCoord, WaterCellSnapshot> after_;
};

class WaterEditHistory final {
public:
    [[nodiscard]] Result<void> execute(WaterStore& store, std::unique_ptr<WaterEditCommand> command);
    [[nodiscard]] Result<void> undo(WaterStore& store);
    [[nodiscard]] Result<void> redo(WaterStore& store);
    [[nodiscard]] std::size_t undo_size() const noexcept { return undo_.size(); }
    [[nodiscard]] std::size_t redo_size() const noexcept { return redo_.size(); }
    [[nodiscard]] const std::string& last_summary() const noexcept { return last_summary_; }

private:
    std::vector<std::unique_ptr<WaterEditCommand>> undo_;
    std::vector<std::unique_ptr<WaterEditCommand>> redo_;
    std::string last_summary_;
};

} // namespace engine
