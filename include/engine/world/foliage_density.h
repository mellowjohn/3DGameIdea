#pragma once

#include "engine/core/result.h"
#include "engine/world/foliage_layers.h"
#include "engine/world/terrain.h"
#include "engine/world/world_partition.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace engine {

struct FoliageMixedLayerWeight {
    std::uint8_t layer_index = 0;
    float weight = 1.0f;
};

struct FoliageDensityCell {
    CellCoord coordinate{};
    std::vector<std::uint8_t> density;
    std::vector<std::uint8_t> layer;
};

struct FoliageCellSnapshot {
    std::vector<std::uint8_t> density;
    std::vector<std::uint8_t> layer;
};

class FoliageDensityStore final {
public:
    static constexpr std::uint32_t k_resolution = k_default_terrain_resolution;
    static constexpr float k_cell_size = k_default_terrain_cell_size;

    [[nodiscard]] bool empty() const noexcept { return cells_.empty(); }
    [[nodiscard]] bool has_cell(CellCoord cell) const;
    [[nodiscard]] const FoliageDensityCell* find_cell(CellCoord cell) const;
    [[nodiscard]] std::uint8_t density_at(CellCoord cell, std::uint32_t x, std::uint32_t z) const;
    [[nodiscard]] std::uint8_t layer_at(CellCoord cell, std::uint32_t x, std::uint32_t z) const;
    [[nodiscard]] std::set<CellCoord> cell_coordinates() const;

    [[nodiscard]] Result<std::set<CellCoord>> apply_foliage_brush(float world_x, float world_z, float radius,
        float strength, std::uint8_t layer_index, bool erase);
    [[nodiscard]] Result<std::set<CellCoord>> apply_foliage_mixed_brush(float world_x, float world_z, float radius,
        float strength, const std::vector<FoliageMixedLayerWeight>& layers, bool erase);
    void set_cell(CellCoord cell, FoliageCellSnapshot snapshot);
    void remove_cell(CellCoord cell);
    [[nodiscard]] FoliageCellSnapshot cell_snapshot_or_empty(CellCoord cell) const;

    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static Result<FoliageDensityStore> from_json(const std::string& text);
    [[nodiscard]] static Result<FoliageDensityStore> load(const std::filesystem::path& path);
    [[nodiscard]] Result<void> save_atomic(const std::filesystem::path& path) const;
    [[nodiscard]] static Result<void> validate_file(const std::filesystem::path& path, std::uint8_t max_layer_index);

private:
    std::uint32_t resolution_ = k_resolution;
    float cell_size_ = k_cell_size;
    std::map<CellCoord, FoliageDensityCell> cells_;

    [[nodiscard]] FoliageDensityCell& ensure_cell(CellCoord cell);
};

class FoliageDensityCommand {
public:
    virtual ~FoliageDensityCommand() = default;
    [[nodiscard]] virtual Result<void> apply(FoliageDensityStore& store) = 0;
    [[nodiscard]] virtual Result<void> revert(FoliageDensityStore& store) = 0;
    [[nodiscard]] virtual std::string label() const = 0;
};

class FoliageDensityBrushStrokeCommand final : public FoliageDensityCommand {
public:
    FoliageDensityBrushStrokeCommand(std::map<CellCoord, FoliageCellSnapshot> before,
        std::map<CellCoord, FoliageCellSnapshot> after);
    Result<void> apply(FoliageDensityStore& store) override;
    Result<void> revert(FoliageDensityStore& store) override;
    std::string label() const override { return "foliage-density-stroke"; }

private:
    std::map<CellCoord, FoliageCellSnapshot> before_;
    std::map<CellCoord, FoliageCellSnapshot> after_;
};

class FoliageDensityHistory final {
public:
    [[nodiscard]] Result<void> execute(FoliageDensityStore& store, std::unique_ptr<FoliageDensityCommand> command);
    [[nodiscard]] Result<void> undo(FoliageDensityStore& store);
    [[nodiscard]] Result<void> redo(FoliageDensityStore& store);
    [[nodiscard]] std::size_t undo_size() const noexcept { return undo_.size(); }
    [[nodiscard]] std::size_t redo_size() const noexcept { return redo_.size(); }
    [[nodiscard]] const std::string& last_summary() const noexcept { return last_summary_; }

private:
    std::vector<std::unique_ptr<FoliageDensityCommand>> undo_;
    std::vector<std::unique_ptr<FoliageDensityCommand>> redo_;
    std::string last_summary_;
};

[[nodiscard]] std::filesystem::path default_foliage_density_path(const std::filesystem::path& project_root);
[[nodiscard]] std::vector<FoliageMixedLayerWeight> default_meadow_mix_weights(const FoliageLayerPalette& palette);

} // namespace engine
