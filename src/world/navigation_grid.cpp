#include "engine/world/navigation_grid.h"

#include "engine/world/terrain.h"
#include "engine/world/water_store.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <set>
#include <unordered_map>

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
            const float world_x = origin_x + x * step;
            const float world_z = origin_z + z * step;
            bool walkable = grid_slope(grid, x, z) <= max_walk_slope;
            if (walkable) {
                if (const auto surface = sample_water_surface_y(world_x, world_z)) {
                    if (is_deep_water(world_x, world_z) ||
                        grid.heights[static_cast<std::size_t>(z) * resolution + x] < *surface - 0.35f)
                        walkable = false;
                }
            }
            grid.walkable[static_cast<std::size_t>(z) * resolution + x] = walkable ? 1 : 0;
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

std::optional<float> StreamedNavigationField::height_at(float world_x, float world_z) const {
    WorldPartition partition;
    const auto cell = partition.cell_for({world_x, 0.0, world_z});
    if (!cell) return std::nullopt;
    const auto found = grids_.find(cell.value());
    if (found == grids_.end()) return std::nullopt;
    const auto& grid = found->second;
    float gx = world_x - static_cast<float>(grid.partition_cell.x) * grid.cell_size;
    float gz = world_z - static_cast<float>(grid.partition_cell.z) * grid.cell_size;
    std::uint32_t x = 0;
    std::uint32_t z = 0;
    if (!grid_to_sample(grid, gx, gz, x, z)) {
        const float step = grid.cell_size / static_cast<float>(grid.resolution - 1);
        x = static_cast<std::uint32_t>(std::clamp(gx / step, 0.0f, static_cast<float>(grid.resolution - 1)));
        z = static_cast<std::uint32_t>(std::clamp(gz / step, 0.0f, static_cast<float>(grid.resolution - 1)));
    }
    return grid.height_at_sample(x, z);
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

Result<void> StreamedNavigationField::ensure_loaded_for_query(WorldPosition from, WorldPosition to,
    float margin_meters) {
    const float min_x = static_cast<float>(std::min(from.x, to.x)) - std::max(0.0f, margin_meters);
    const float max_x = static_cast<float>(std::max(from.x, to.x)) + std::max(0.0f, margin_meters);
    const float min_z = static_cast<float>(std::min(from.z, to.z)) - std::max(0.0f, margin_meters);
    const float max_z = static_cast<float>(std::max(from.z, to.z)) + std::max(0.0f, margin_meters);
    const float mid_x = 0.5f * (min_x + max_x);
    const float mid_z = 0.5f * (min_z + max_z);
    const float half = 0.5f * std::max(max_x - min_x, max_z - min_z);
    const std::uint32_t radius =
        static_cast<std::uint32_t>(std::clamp(static_cast<int>(std::ceil(half / k_cell_size)) + 1, 1, 8));
    return update({mid_x, 0.0f, mid_z}, radius);
}

namespace {

struct NavSampleKey {
    int x = 0;
    int z = 0;
    bool operator==(const NavSampleKey& other) const noexcept { return x == other.x && z == other.z; }
};

struct NavSampleKeyHash {
    std::size_t operator()(const NavSampleKey& key) const noexcept {
        return (static_cast<std::size_t>(static_cast<std::uint32_t>(key.x)) << 32) ^
               static_cast<std::uint32_t>(key.z);
    }
};

NavSampleKey world_to_sample_key(float world_x, float world_z) {
    return {static_cast<int>(std::lround(world_x / StreamedNavigationField::k_sample_step)),
            static_cast<int>(std::lround(world_z / StreamedNavigationField::k_sample_step))};
}

WorldPosition sample_key_to_world(const NavSampleKey& key, float height) {
    return {static_cast<double>(key.x) * StreamedNavigationField::k_sample_step, static_cast<double>(height),
            static_cast<double>(key.z) * StreamedNavigationField::k_sample_step};
}

} // namespace

Result<NavigationPath> StreamedNavigationField::find_path(WorldPosition from, WorldPosition to, float snap_radius,
    bool simplify) const {
    WorldPartition partition;
    if (!partition.cell_for(from) || !partition.cell_for(to))
        return Result<NavigationPath>::failure(
            navigation_error("NAV-QUERY-OUTSIDE", "Path query is outside world bounds"));

    const auto start_opt = nearest_walkable_point(from, snap_radius);
    const auto goal_opt = nearest_walkable_point(to, snap_radius);
    if (!start_opt || !goal_opt) {
        NavigationPath empty;
        empty.found = false;
        return Result<NavigationPath>::success(std::move(empty));
    }

    const NavSampleKey start = world_to_sample_key(static_cast<float>(start_opt->x), static_cast<float>(start_opt->z));
    const NavSampleKey goal = world_to_sample_key(static_cast<float>(goal_opt->x), static_cast<float>(goal_opt->z));
    if (!is_walkable_at(static_cast<float>(start_opt->x), static_cast<float>(start_opt->z)) ||
        !is_walkable_at(static_cast<float>(goal_opt->x), static_cast<float>(goal_opt->z))) {
        NavigationPath empty;
        empty.found = false;
        return Result<NavigationPath>::success(std::move(empty));
    }

    if (start == goal) {
        NavigationPath path;
        path.found = true;
        path.points.push_back(*start_opt);
        if (std::hypot(goal_opt->x - start_opt->x, goal_opt->z - start_opt->z) > 1.0e-3)
            path.points.push_back(*goal_opt);
        return Result<NavigationPath>::success(std::move(path));
    }

    struct OpenNode {
        float f = 0.0f;
        float g = 0.0f;
        NavSampleKey key{};
        bool operator>(const OpenNode& other) const noexcept { return f > other.f; }
    };

    auto heuristic = [&](const NavSampleKey& a, const NavSampleKey& b) {
        const float dx = static_cast<float>(a.x - b.x);
        const float dz = static_cast<float>(a.z - b.z);
        return std::hypot(dx, dz) * k_sample_step;
    };

    auto sample_loaded_walkable = [&](const NavSampleKey& key) -> bool {
        const float wx = static_cast<float>(key.x) * k_sample_step;
        const float wz = static_cast<float>(key.z) * k_sample_step;
        const auto cell = partition.cell_for({wx, 0.0, wz});
        if (!cell || !contains(cell.value())) return false;
        return is_walkable_at(wx, wz);
    };

    std::priority_queue<OpenNode, std::vector<OpenNode>, std::greater<OpenNode>> open;
    std::unordered_map<NavSampleKey, float, NavSampleKeyHash> g_score;
    std::unordered_map<NavSampleKey, NavSampleKey, NavSampleKeyHash> came_from;
    g_score[start] = 0.0f;
    open.push({heuristic(start, goal), 0.0f, start});

    constexpr int k_max_expansions = 20000;
    int expansions = 0;
    bool reached = false;
    while (!open.empty() && expansions < k_max_expansions) {
        const OpenNode current = open.top();
        open.pop();
        const auto g_it = g_score.find(current.key);
        if (g_it == g_score.end() || current.g > g_it->second + 1.0e-3f) continue;
        ++expansions;
        if (current.key == goal) {
            reached = true;
            break;
        }

        static constexpr int k_dx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
        static constexpr int k_dz[8] = {0, 0, 1, -1, 1, -1, 1, -1};
        for (int i = 0; i < 8; ++i) {
            const NavSampleKey next{current.key.x + k_dx[i], current.key.z + k_dz[i]};
            if (!sample_loaded_walkable(next)) continue;
            if (i >= 4) {
                const NavSampleKey orth_a{current.key.x + k_dx[i], current.key.z};
                const NavSampleKey orth_b{current.key.x, current.key.z + k_dz[i]};
                if (!sample_loaded_walkable(orth_a) || !sample_loaded_walkable(orth_b)) continue;
            }
            const float step_cost = (i < 4) ? k_sample_step : (k_sample_step * 1.41421356f);
            const float tentative = current.g + step_cost;
            const auto existing = g_score.find(next);
            if (existing != g_score.end() && tentative + 1.0e-3f >= existing->second) continue;
            g_score[next] = tentative;
            came_from[next] = current.key;
            open.push({tentative + heuristic(next, goal), tentative, next});
        }
    }

    NavigationPath path;
    if (!reached) {
        path.found = false;
        return Result<NavigationPath>::success(std::move(path));
    }

    std::vector<NavSampleKey> keys;
    for (NavSampleKey at = goal;;) {
        keys.push_back(at);
        if (at == start) break;
        const auto parent = came_from.find(at);
        if (parent == came_from.end()) {
            path.found = false;
            return Result<NavigationPath>::success(std::move(path));
        }
        at = parent->second;
    }
    std::reverse(keys.begin(), keys.end());

    auto point_for = [&](const NavSampleKey& key) {
        const float wx = static_cast<float>(key.x) * k_sample_step;
        const float wz = static_cast<float>(key.z) * k_sample_step;
        const float height = height_at(wx, wz).value_or(static_cast<float>(sample_terrain_height(wx, wz)));
        return sample_key_to_world(key, height);
    };

    std::vector<WorldPosition> raw;
    raw.reserve(keys.size());
    for (const auto& key : keys) raw.push_back(point_for(key));
    if (!raw.empty()) {
        raw.front() = *start_opt;
        raw.back() = *goal_opt;
    }

    if (simplify && raw.size() > 2) {
        std::vector<WorldPosition> pulled;
        pulled.push_back(raw.front());
        std::size_t anchor = 0;
        while (anchor + 1 < raw.size()) {
            std::size_t farthest = anchor + 1;
            for (std::size_t i = raw.size() - 1; i > anchor; --i) {
                const auto los = line_of_walk(raw[anchor], raw[i]);
                if (los && los.value()) {
                    farthest = i;
                    break;
                }
            }
            pulled.push_back(raw[farthest]);
            if (farthest <= anchor) break;
            anchor = farthest;
        }
        path.points = std::move(pulled);
    } else {
        path.points = std::move(raw);
    }

    path.found = true;
    for (std::size_t i = 1; i < path.points.size(); ++i) {
        path.length_xz += static_cast<float>(
            std::hypot(path.points[i].x - path.points[i - 1].x, path.points[i].z - path.points[i - 1].z));
    }
    return Result<NavigationPath>::success(std::move(path));
}

} // namespace engine
