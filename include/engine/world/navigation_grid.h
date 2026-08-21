#pragma once

#include "engine/core/result.h"
#include "engine/world/world_partition.h"

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace engine {

struct NavigationGrid {
    CellCoord partition_cell{};
    float cell_size = 128.0f;
    std::uint32_t resolution = 33;
    std::vector<float> heights;
    std::vector<std::uint8_t> walkable;

    [[nodiscard]] float height_at_sample(std::uint32_t x, std::uint32_t z) const;
    [[nodiscard]] bool is_walkable(std::uint32_t x, std::uint32_t z) const;
    [[nodiscard]] std::optional<WorldPosition> nearest_walkable(float world_x, float world_z, float max_search) const;
    [[nodiscard]] bool line_of_walk(float from_x, float from_z, float to_x, float to_z) const;
};

struct NavigationPath {
    bool found = false;
    /// World-space corners including snapped start and goal (Y = terrain height).
    std::vector<WorldPosition> points;
    float length_xz = 0.0f;
};

[[nodiscard]] Result<NavigationGrid> build_navigation_grid(CellCoord partition_cell, float cell_size = 128.0f,
    std::uint32_t resolution = 33, float max_walk_slope = 0.45f);

[[nodiscard]] std::vector<CellCoord> partition_cells_in_radius(CellCoord center, std::uint32_t radius);

class StreamedNavigationField final {
public:
    static constexpr float k_cell_size = 128.0f;
    static constexpr std::uint32_t k_default_resolution = 33;
    static constexpr std::uint32_t k_default_radius = 4;
    static constexpr float k_sample_step = k_cell_size / static_cast<float>(k_default_resolution - 1);

    [[nodiscard]] Result<void> update(const std::array<float, 3>& camera_position, std::uint32_t radius = k_default_radius);
    /// Load partition cells that cover an axis-aligned span around from/to (plus margin samples).
    [[nodiscard]] Result<void> ensure_loaded_for_query(WorldPosition from, WorldPosition to, float margin_meters = 16.0f);
    [[nodiscard]] std::optional<WorldPosition> nearest_walkable_point(WorldPosition query, float max_search) const;
    [[nodiscard]] Result<bool> line_of_walk(WorldPosition from, WorldPosition to) const;
    /// A* over loaded 4 m walkability samples. Fails when required cells are not resident.
    [[nodiscard]] Result<NavigationPath> find_path(WorldPosition from, WorldPosition to, float snap_radius = 12.0f,
        bool simplify = true) const;
    [[nodiscard]] std::size_t loaded_cell_count() const noexcept { return grids_.size(); }
    [[nodiscard]] bool contains(CellCoord cell) const { return grids_.find(cell) != grids_.end(); }
    [[nodiscard]] CellCoord focus_cell() const noexcept { return focus_; }
    [[nodiscard]] bool is_walkable_at(float world_x, float world_z) const;

private:
    [[nodiscard]] std::optional<float> height_at(float world_x, float world_z) const;

    std::map<CellCoord, NavigationGrid> grids_;
    CellCoord focus_{};
};

} // namespace engine
