#include "engine/world/navigation_grid.h"

#include "engine/world/terrain.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace engine {
namespace {

EngineError navigation_error(std::string code, std::string message) {
    return {std::move(code), Severity::Error, ErrorCategory::Validation, "navigation-grid", std::move(message),
            ENGINE_SOURCE_CONTEXT, {}, "Use partition-aligned cells and valid world coordinates.",
            make_correlation_id()};
}

float grid_slope(const NavigationGrid& grid, std::uint32_t x, std::uint32_t z) {
    const float step = grid.cell_size / static_cast<float>(grid.resolution - 1);
    const auto sample = [&](std::uint32_t sx, std::uint32_t sz) {
        sx = std::min(sx, grid.resolution - 1);
        sz = std::min(sz, grid.resolution - 1);
        return grid.height_at_sample(sx, sz);
    };
    const float dhdx = (sample(x + 1, z) - sample(x - 1, z)) / std::max(step * 2.0f, 0.001f);
    const float dhdz = (sample(x, z + 1) - sample(x, z - 1)) / std::max(step * 2.0f, 0.001f);
    return std::sqrt(dhdx * dhdx + dhdz * dhdz);
}

void world_to_grid(const NavigationGrid& grid, float world_x, float world_z, float& gx, float& gz) {
    const float origin_x = static_cast<float>(grid.partition_cell.x) * grid.cell_size;
    const float origin_z = static_cast<float>(grid.partition_cell.z) * grid.cell_size;
    gx = world_x - origin_x;
    gz = world_z - origin_z;
}

bool grid_to_sample(const NavigationGrid& grid, float gx, float gz, std::uint32_t& x, std::uint32_t& z) {
    if (gx < 0.0f || gz < 0.0f || gx > grid.cell_size || gz > grid.cell_size) return false;
    const float step = grid.cell_size / static_cast<float>(grid.resolution - 1);
    x = static_cast<std::uint32_t>(std::round(gx / step));
    z = static_cast<std::uint32_t>(std::round(gz / step));
    return x < grid.resolution && z < grid.resolution;
}

} // namespace

float NavigationGrid::height_at_sample(std::uint32_t x, std::uint32_t z) const {
    if (x >= resolution || z >= resolution || heights.size() != static_cast<std::size_t>(resolution) * resolution)
        return 0.0f;
    return heights[static_cast<std::size_t>(z) * resolution + x];
}

bool NavigationGrid::is_walkable(std::uint32_t x, std::uint32_t z) const {
    if (x >= resolution || z >= resolution || walkable.size() != static_cast<std::size_t>(resolution) * resolution)
        return false;
    return walkable[static_cast<std::size_t>(z) * resolution + x] != 0;
}

std::optional<WorldPosition> NavigationGrid::nearest_walkable(float world_x, float world_z, float max_search) const {
    if (!(max_search > 0.0f)) return std::nullopt;
    float gx = 0.0f;
    float gz = 0.0f;
    world_to_grid(*this, world_x, world_z, gx, gz);
    const float step = cell_size / static_cast<float>(resolution - 1);
    const int max_steps = static_cast<int>(std::ceil(max_search / step));
    std::uint32_t center_x = 0;
    std::uint32_t center_z = 0;
    if (!grid_to_sample(*this, gx, gz, center_x, center_z)) {
        center_x = static_cast<std::uint32_t>(std::clamp(gx / step, 0.0f, static_cast<float>(resolution - 1)));
        center_z = static_cast<std::uint32_t>(std::clamp(gz / step, 0.0f, static_cast<float>(resolution - 1)));
    }
    if (is_walkable(center_x, center_z)) {
        const float origin_x = static_cast<float>(partition_cell.x) * cell_size;
        const float origin_z = static_cast<float>(partition_cell.z) * cell_size;
        return WorldPosition{origin_x + center_x * step, height_at_sample(center_x, center_z),
                             origin_z + center_z * step};
    }
    float best_dist = max_search + 1.0f;
    std::optional<WorldPosition> best;
    for (int dz = -max_steps; dz <= max_steps; ++dz) {
        for (int dx = -max_steps; dx <= max_steps; ++dx) {
            const int sx = static_cast<int>(center_x) + dx;
            const int sz = static_cast<int>(center_z) + dz;
            if (sx < 0 || sz < 0 || sx >= static_cast<int>(resolution) || sz >= static_cast<int>(resolution)) continue;
            if (!is_walkable(static_cast<std::uint32_t>(sx), static_cast<std::uint32_t>(sz))) continue;
            const float dist = std::hypot(static_cast<float>(dx) * step, static_cast<float>(dz) * step);
            if (dist > max_search || dist >= best_dist) continue;
            const float origin_x = static_cast<float>(partition_cell.x) * cell_size;
            const float origin_z = static_cast<float>(partition_cell.z) * cell_size;
            best_dist = dist;
            best = WorldPosition{origin_x + sx * step, height_at_sample(static_cast<std::uint32_t>(sx),
                                                                        static_cast<std::uint32_t>(sz)),
                                 origin_z + sz * step};
        }
    }
    return best;
}

bool NavigationGrid::line_of_walk(float from_x, float from_z, float to_x, float to_z) const {
    const float dx = to_x - from_x;
    const float dz = to_z - from_z;
    const float length = std::hypot(dx, dz);
    if (!(length > 0.0f)) {
        float gx = 0.0f;
        float gz = 0.0f;
        world_to_grid(*this, from_x, from_z, gx, gz);
        std::uint32_t x = 0;
        std::uint32_t z = 0;
        return grid_to_sample(*this, gx, gz, x, z) && is_walkable(x, z);
    }
    const float step = cell_size / static_cast<float>(resolution - 1);
    const int samples = std::max(2, static_cast<int>(std::ceil(length / (step * 0.5f))) + 1);
    for (int i = 0; i <= samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(samples);
        const float wx = from_x + dx * t;
        const float wz = from_z + dz * t;
        float gx = 0.0f;
        float gz = 0.0f;
        world_to_grid(*this, wx, wz, gx, gz);
        std::uint32_t x = 0;
        std::uint32_t z = 0;
        if (!grid_to_sample(*this, gx, gz, x, z) || !is_walkable(x, z)) return false;
    }
    return true;
}

Result<NavigationGrid> build_navigation_grid(CellCoord partition_cell, float cell_size, std::uint32_t resolution,
    float max_walk_slope) {
    if (resolution < 3 || resolution > 257 || ((resolution - 1) & (resolution - 2)) != 0)
        return Result<NavigationGrid>::failure(
            navigation_error("NAV-GRID-RESOLUTION", "Navigation resolution must be 2^n+1"));
    if (!(cell_size > 0.0f) || !(max_walk_slope > 0.0f))
        return Result<NavigationGrid>::failure(
            navigation_error("NAV-GRID-INVALID", "Navigation cell size and walk slope must be positive"));

    NavigationGrid grid;
    grid.partition_cell = partition_cell;
    grid.cell_size = cell_size;
    grid.resolution = resolution;
    grid.heights.resize(static_cast<std::size_t>(resolution) * resolution);
    grid.walkable.resize(static_cast<std::size_t>(resolution) * resolution);
    const float step = cell_size / static_cast<float>(resolution - 1);
    const float origin_x = static_cast<float>(partition_cell.x) * cell_size;
    const float origin_z = static_cast<float>(partition_cell.z) * cell_size;
    for (std::uint32_t z = 0; z < resolution; ++z) {
        for (std::uint32_t x = 0; x < resolution; ++x) {
            const float world_x = origin_x + x * step;
            const float world_z = origin_z + z * step;
            grid.heights[static_cast<std::size_t>(z) * resolution + x] = sample_terrain_height(world_x, world_z);
        }
    }
    for (std::uint32_t z = 0; z < resolution; ++z) {
        for (std::uint32_t x = 0; x < resolution; ++x) {
            grid.walkable[static_cast<std::size_t>(z) * resolution + x] =
                grid_slope(grid, x, z) <= max_walk_slope ? 1 : 0;
        }
    }
    return Result<NavigationGrid>::success(std::move(grid));
}

std::vector<CellCoord> partition_cells_in_radius(CellCoord center, std::uint32_t radius) {
    std::vector<CellCoord> cells;
    if (radius > 32) return cells;
    const int extent = static_cast<int>(radius);
    cells.reserve(static_cast<std::size_t>((extent * 2 + 1) * (extent * 2 + 1)));
    for (int z = -extent; z <= extent; ++z) {
        for (int x = -extent; x <= extent; ++x) cells.push_back({center.x + x, center.z + z});
    }
    return cells;
}

Result<void> StreamedNavigationField::update(const std::array<float, 3>& camera_position, std::uint32_t radius) {
    WorldPartition partition;
    const auto center = partition.cell_for({camera_position[0], camera_position[1], camera_position[2]});
    if (!center) return Result<void>::failure(center.error());
    const auto wanted = partition_cells_in_radius(center.value(), radius);
    const std::set<CellCoord> wanted_set(wanted.begin(), wanted.end());
    if (center.value().x == focus_.x && center.value().z == focus_.z && wanted_set.size() == grids_.size()) {
        bool same = true;
        for (const auto& cell : wanted_set) {
            if (!contains(cell)) {
                same = false;
                break;
            }
        }
        if (same) return Result<void>::success();
    }
    for (auto it = grids_.begin(); it != grids_.end();) {
        if (wanted_set.find(it->first) == wanted_set.end())
            it = grids_.erase(it);
        else
            ++it;
    }
    for (const auto& cell : wanted_set) {
        if (contains(cell)) continue;
        const auto built = build_navigation_grid(cell);
        if (!built) return Result<void>::failure(built.error());
        grids_.emplace(cell, built.value());
    }
    focus_ = center.value();
    return Result<void>::success();
}

bool StreamedNavigationField::is_walkable_at(float world_x, float world_z) const {
    WorldPartition partition;
    const auto cell = partition.cell_for({world_x, 0.0, world_z});
    if (!cell) return false;
    const auto found = grids_.find(cell.value());
    if (found == grids_.end()) return false;
    const auto& grid = found->second;
    float gx = world_x - static_cast<float>(grid.partition_cell.x) * grid.cell_size;
    float gz = world_z - static_cast<float>(grid.partition_cell.z) * grid.cell_size;
    if (gx < 0.0f || gz < 0.0f || gx > grid.cell_size || gz > grid.cell_size) return false;
    std::uint32_t x = 0;
    std::uint32_t z = 0;
    if (!grid_to_sample(grid, gx, gz, x, z)) {
        const float step = grid.cell_size / static_cast<float>(grid.resolution - 1);
        x = static_cast<std::uint32_t>(std::clamp(gx / step, 0.0f, static_cast<float>(grid.resolution - 1)));
        z = static_cast<std::uint32_t>(std::clamp(gz / step, 0.0f, static_cast<float>(grid.resolution - 1)));
    }
    return grid.is_walkable(x, z);
}

std::optional<WorldPosition> StreamedNavigationField::nearest_walkable_point(WorldPosition query,
    float max_search) const {
    if (!(max_search > 0.0f)) return std::nullopt;
    WorldPartition partition;
    const auto cell = partition.cell_for(query);
    if (!cell) return std::nullopt;
    const auto found = grids_.find(cell.value());
    if (found == grids_.end()) return std::nullopt;
    return found->second.nearest_walkable(static_cast<float>(query.x), static_cast<float>(query.z), max_search);
}

Result<bool> StreamedNavigationField::line_of_walk(WorldPosition from, WorldPosition to) const {
    WorldPartition partition;
    const auto from_cell = partition.cell_for(from);
    const auto to_cell = partition.cell_for(to);
    if (!from_cell || !to_cell)
        return Result<bool>::failure(navigation_error("NAV-QUERY-OUTSIDE", "Walk query is outside world bounds"));
    const float dx = static_cast<float>(to.x - from.x);
    const float dz = static_cast<float>(to.z - from.z);
    const float length = std::hypot(dx, dz);
    if (!(length > 0.0f)) {
        if (!grids_.count(from_cell.value()))
            return Result<bool>::failure(navigation_error("NAV-CELL-NOT-LOADED", "Navigation cell is not loaded"));
        return Result<bool>::success(is_walkable_at(static_cast<float>(from.x), static_cast<float>(from.z)));
    }
    const float step = k_cell_size / static_cast<float>(k_default_resolution - 1);
    const int samples = std::max(2, static_cast<int>(std::ceil(length / (step * 0.5f))) + 1);
    for (int i = 0; i <= samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(samples);
        const float wx = static_cast<float>(from.x) + dx * t;
        const float wz = static_cast<float>(from.z) + dz * t;
        const auto sample_cell = partition.cell_for({wx, 0.0, wz});
        if (!sample_cell)
            return Result<bool>::failure(navigation_error("NAV-QUERY-OUTSIDE", "Walk query is outside world bounds"));
        if (!contains(sample_cell.value()))
            return Result<bool>::failure(navigation_error("NAV-CELL-NOT-LOADED", "Navigation cell is not loaded"));
        if (!is_walkable_at(wx, wz)) return Result<bool>::success(false);
    }
    (void)from_cell;
    (void)to_cell;
    return Result<bool>::success(true);
}

} // namespace engine
