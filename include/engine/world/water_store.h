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

struct WaterSeaRegion {
    std::string id;
    float min_x = 0.0f;
    float max_x = 0.0f;
    float min_z = 0.0f;
    float max_z = 0.0f;
};

struct WaterCellMask {
    CellCoord coordinate{};
    std::vector<std::uint8_t> fill;

    [[nodiscard]] Result<void> validate(std::uint32_t expected_resolution) const;
};

class WaterStore final {
public:
    static constexpr std::uint32_t k_resolution = k_default_terrain_resolution;
    static constexpr float k_cell_size = k_default_terrain_cell_size;
    static constexpr float k_deep_depth = 2.5f;
    static constexpr float k_shallow_depth = 1.2f;
    static constexpr std::uint8_t k_fill_threshold = 64;

    [[nodiscard]] bool empty() const noexcept { return cells_.empty() && regions_.empty(); }
    [[nodiscard]] float sea_level() const noexcept { return sea_level_; }
    void set_sea_level(float value) noexcept { sea_level_ = value; }
    [[nodiscard]] const std::vector<WaterSeaRegion>& sea_regions() const noexcept { return regions_; }
    [[nodiscard]] bool has_cell(CellCoord cell) const;
    [[nodiscard]] const WaterCellMask* find_cell(CellCoord cell) const;
    [[nodiscard]] std::uint8_t fill_at(CellCoord cell, std::uint32_t x, std::uint32_t z) const;
    [[nodiscard]] std::uint8_t sample_fill_world(float world_x, float world_z) const;
    [[nodiscard]] std::set<CellCoord> cell_coordinates() const;
    [[nodiscard]] bool in_sea_region(float world_x, float world_z) const;

    [[nodiscard]] Result<std::set<CellCoord>> apply_place_brush(float world_x, float world_z, float radius,
        float strength);
    [[nodiscard]] Result<std::set<CellCoord>> apply_erase_brush(float world_x, float world_z, float radius,
        float strength);
    void set_cell_fill(CellCoord cell, std::vector<std::uint8_t> fill);
    void remove_cell(CellCoord cell);
    [[nodiscard]] std::vector<std::uint8_t> cell_fill_or_empty(CellCoord cell) const;
    void set_sea_regions(std::vector<WaterSeaRegion> regions);

    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static Result<WaterStore> from_json(const std::string& text);
    [[nodiscard]] static Result<WaterStore> load(const std::filesystem::path& path);
    [[nodiscard]] Result<void> save_atomic(const std::filesystem::path& path) const;
    [[nodiscard]] static Result<void> validate_file(const std::filesystem::path& path);

private:
    std::uint32_t resolution_ = k_resolution;
    float cell_size_ = k_cell_size;
    float sea_level_ = 0.0f;
    std::vector<WaterSeaRegion> regions_;
    std::map<CellCoord, WaterCellMask> cells_;

    [[nodiscard]] WaterCellMask& ensure_cell(CellCoord cell);
    [[nodiscard]] Result<std::set<CellCoord>> apply_fill_brush(float world_x, float world_z, float radius,
        float strength, bool erase);
};

void set_active_water_store(const WaterStore* store) noexcept;
[[nodiscard]] const WaterStore* active_water_store() noexcept;

[[nodiscard]] std::optional<float> sample_water_surface_y(float world_x, float world_z);
[[nodiscard]] bool is_underwater(float world_x, float world_z, float entity_y);
[[nodiscard]] float water_depth(float world_x, float world_z);
[[nodiscard]] bool is_deep_water(float world_x, float world_z);

[[nodiscard]] std::filesystem::path default_water_surfaces_path(const std::filesystem::path& project_root);

} // namespace engine
