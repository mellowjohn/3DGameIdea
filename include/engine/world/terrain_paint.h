#pragma once

#include "engine/assets/material_asset.h"
#include "engine/core/result.h"
#include "engine/world/terrain.h"
#include "engine/world/world_partition.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace engine {

struct TerrainPaintCell {
    CellCoord coordinate{};
    std::vector<std::uint16_t> indices;
};

class TerrainPaintStore final {
public:
    static constexpr std::uint32_t k_resolution = k_default_terrain_resolution;
    static constexpr float k_cell_size = k_default_terrain_cell_size;

    [[nodiscard]] bool empty() const noexcept { return cells_.empty(); }
    [[nodiscard]] const std::vector<std::string>& materials() const noexcept { return materials_; }
    [[nodiscard]] bool has_cell(CellCoord cell) const;
    [[nodiscard]] const TerrainPaintCell* find_cell(CellCoord cell) const;
    [[nodiscard]] std::uint16_t index_at(CellCoord cell, std::uint32_t x, std::uint32_t z) const;
    [[nodiscard]] std::uint16_t sample_index_world(float world_x, float world_z) const;
    [[nodiscard]] std::set<CellCoord> cell_coordinates() const;
    [[nodiscard]] const std::string* material_path_for_index(std::uint16_t index) const;

    [[nodiscard]] std::uint16_t ensure_material_index(const std::string& material_path);
    [[nodiscard]] Result<std::set<CellCoord>> apply_material_brush(float world_x, float world_z, float radius,
        std::uint16_t material_index);
    void set_cell_indices(CellCoord cell, std::vector<std::uint16_t> indices);
    void remove_cell(CellCoord cell);
    [[nodiscard]] std::vector<std::uint16_t> cell_indices_or_empty(CellCoord cell) const;

    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static Result<TerrainPaintStore> from_json(const std::string& text);
    [[nodiscard]] static Result<TerrainPaintStore> load(const std::filesystem::path& path);
    [[nodiscard]] Result<void> save_atomic(const std::filesystem::path& path) const;
    [[nodiscard]] static Result<void> validate_file(const std::filesystem::path& path);

private:
    std::uint32_t resolution_ = k_resolution;
    float cell_size_ = k_cell_size;
    std::vector<std::string> materials_;
    std::map<CellCoord, TerrainPaintCell> cells_;

    [[nodiscard]] TerrainPaintCell& ensure_cell(CellCoord cell);
};

using TerrainPaintMaterialLookup = std::function<const MaterialAsset*(const std::string& normalized_path)>;

class TerrainPaintCommand {
public:
    virtual ~TerrainPaintCommand() = default;
    [[nodiscard]] virtual Result<void> apply(TerrainPaintStore& store) = 0;
    [[nodiscard]] virtual Result<void> revert(TerrainPaintStore& store) = 0;
    [[nodiscard]] virtual std::string label() const = 0;
};

class TerrainPaintBrushStrokeCommand final : public TerrainPaintCommand {
public:
    TerrainPaintBrushStrokeCommand(std::map<CellCoord, std::vector<std::uint16_t>> before,
        std::map<CellCoord, std::vector<std::uint16_t>> after);
    Result<void> apply(TerrainPaintStore& store) override;
    Result<void> revert(TerrainPaintStore& store) override;
    std::string label() const override { return "terrain-paint-stroke"; }

private:
    std::map<CellCoord, std::vector<std::uint16_t>> before_;
    std::map<CellCoord, std::vector<std::uint16_t>> after_;
};

class TerrainPaintHistory final {
public:
    [[nodiscard]] Result<void> execute(TerrainPaintStore& store, std::unique_ptr<TerrainPaintCommand> command);
    [[nodiscard]] Result<void> undo(TerrainPaintStore& store);
    [[nodiscard]] Result<void> redo(TerrainPaintStore& store);
    [[nodiscard]] std::size_t undo_size() const noexcept { return undo_.size(); }
    [[nodiscard]] std::size_t redo_size() const noexcept { return redo_.size(); }
    [[nodiscard]] const std::string& last_summary() const noexcept { return last_summary_; }

private:
    std::vector<std::unique_ptr<TerrainPaintCommand>> undo_;
    std::vector<std::unique_ptr<TerrainPaintCommand>> redo_;
    std::string last_summary_;
};

[[nodiscard]] std::filesystem::path default_terrain_paint_path(const std::filesystem::path& project_root);

} // namespace engine
