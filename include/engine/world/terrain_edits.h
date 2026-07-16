#pragma once

#include "engine/core/result.h"
#include "engine/world/terrain.h"
#include "engine/world/world_partition.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace engine {

struct TerrainCellEdits {
    CellCoord coordinate{};
    std::vector<float> deltas;

    [[nodiscard]] Result<void> validate(std::uint32_t expected_resolution) const;
};

class TerrainEditStore final {
public:
    static constexpr std::uint32_t k_resolution = k_default_terrain_resolution;
    static constexpr float k_cell_size = k_default_terrain_cell_size;
    static constexpr float k_max_delta = 32.0f;

    [[nodiscard]] bool empty() const noexcept { return cells_.empty(); }
    [[nodiscard]] bool has_cell(CellCoord cell) const;
    [[nodiscard]] const TerrainCellEdits* find_cell(CellCoord cell) const;
    [[nodiscard]] float delta_at(CellCoord cell, std::uint32_t x, std::uint32_t z) const;
    [[nodiscard]] float sample_delta_world(float world_x, float world_z) const;
    [[nodiscard]] std::set<CellCoord> cell_coordinates() const;

    [[nodiscard]] Result<std::set<CellCoord>> apply_brush(float world_x, float world_z, float radius, float strength,
        bool lower);
    /// Blend sample heights toward `target_height` (world Y). `strength` is max meters moved per sample (falloff applied).
    [[nodiscard]] Result<std::set<CellCoord>> apply_flatten_brush(float world_x, float world_z, float radius,
        float strength, float target_height);
    void set_cell_deltas(CellCoord cell, std::vector<float> deltas);
    void remove_cell(CellCoord cell);
    [[nodiscard]] std::vector<float> cell_deltas_or_empty(CellCoord cell) const;

    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static Result<TerrainEditStore> from_json(const std::string& text);
    [[nodiscard]] static Result<TerrainEditStore> load(const std::filesystem::path& path);
    [[nodiscard]] Result<void> save_atomic(const std::filesystem::path& path) const;
    [[nodiscard]] static Result<void> validate_file(const std::filesystem::path& path);

private:
    std::uint32_t resolution_ = k_resolution;
    float cell_size_ = k_cell_size;
    std::map<CellCoord, TerrainCellEdits> cells_;

    [[nodiscard]] TerrainCellEdits& ensure_cell(CellCoord cell);
};

void set_active_terrain_edits(const TerrainEditStore* store) noexcept;
[[nodiscard]] const TerrainEditStore* active_terrain_edits() noexcept;

[[nodiscard]] std::filesystem::path default_terrain_edits_path(const std::filesystem::path& project_root);

} // namespace engine
