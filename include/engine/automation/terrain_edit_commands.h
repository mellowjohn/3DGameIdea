#pragma once

#include "engine/world/terrain_edits.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace engine {

class TerrainEditCommand {
public:
    virtual ~TerrainEditCommand() = default;
    [[nodiscard]] virtual Result<void> apply(TerrainEditStore& store) = 0;
    [[nodiscard]] virtual Result<void> revert(TerrainEditStore& store) = 0;
    [[nodiscard]] virtual std::string label() const = 0;
};

class TerrainBrushStrokeCommand final : public TerrainEditCommand {
public:
    TerrainBrushStrokeCommand(std::map<CellCoord, std::vector<float>> before, std::map<CellCoord, std::vector<float>> after);
    Result<void> apply(TerrainEditStore& store) override;
    Result<void> revert(TerrainEditStore& store) override;
    std::string label() const override { return "terrain-brush-stroke"; }

private:
    std::map<CellCoord, std::vector<float>> before_;
    std::map<CellCoord, std::vector<float>> after_;
};

class TerrainEditHistory final {
public:
    [[nodiscard]] Result<void> execute(TerrainEditStore& store, std::unique_ptr<TerrainEditCommand> command);
    [[nodiscard]] Result<void> undo(TerrainEditStore& store);
    [[nodiscard]] Result<void> redo(TerrainEditStore& store);
    [[nodiscard]] std::size_t undo_size() const noexcept { return undo_.size(); }
    [[nodiscard]] std::size_t redo_size() const noexcept { return redo_.size(); }
    [[nodiscard]] const std::string& last_summary() const noexcept { return last_summary_; }

private:
    std::vector<std::unique_ptr<TerrainEditCommand>> undo_;
    std::vector<std::unique_ptr<TerrainEditCommand>> redo_;
    std::string last_summary_;
};

} // namespace engine
