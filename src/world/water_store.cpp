#include "engine/world/water_store.h"

#include "engine/world/terrain.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace engine {
namespace {
const WaterStore* g_active_water = nullptr;

EngineError water_error(std::string code, std::string message, std::string remediation) {
    return {std::move(code), Severity::Error, ErrorCategory::Validation, "water-surfaces", std::move(message),
            ENGINE_SOURCE_CONTEXT, {}, std::move(remediation), make_correlation_id()};
}

[[nodiscard]] float brush_falloff(float distance, float radius) {
    if (radius <= 0.0f) return 0.0f;
    const float t = std::clamp(1.0f - distance / radius, 0.0f, 1.0f);
    return t * t;
}

[[nodiscard]] bool has_authored_fill(const WaterStore* store, float world_x, float world_z) {
    if (!store) return false;
    return store->sample_fill_world(world_x, world_z) >= WaterStore::k_fill_threshold;
}
} // namespace

void set_active_water_store(const WaterStore* store) noexcept { g_active_water = store; }
const WaterStore* active_water_store() noexcept { return g_active_water; }

std::filesystem::path default_water_surfaces_path(const std::filesystem::path& project_root) {
    return project_root / "assets/terrain/water-surfaces.json";
}

Result<void> WaterCellMask::validate(std::uint32_t expected_resolution) const {
    const auto expected = static_cast<std::size_t>(expected_resolution) * expected_resolution;
    if (fill.size() != expected)
        return Result<void>::failure(water_error("WATER-FILL-COUNT", "Water fill sample count does not match resolution",
            "Provide fill values for every sample."));
    return Result<void>::success();
}

bool WaterStore::has_cell(CellCoord cell) const { return cells_.find(cell) != cells_.end(); }

const WaterCellMask* WaterStore::find_cell(CellCoord cell) const {
    const auto found = cells_.find(cell);
    return found == cells_.end() ? nullptr : &found->second;
}

std::uint8_t WaterStore::fill_at(CellCoord cell, std::uint32_t x, std::uint32_t z) const {
    const auto* entry = find_cell(cell);
    if (!entry || x >= resolution_ || z >= resolution_) return 0;
    return entry->fill[static_cast<std::size_t>(z) * resolution_ + x];
}

std::uint8_t WaterStore::sample_fill_world(float world_x, float world_z) const {
    if (cells_.empty()) return 0;
    const float origin_x = std::floor((world_x + cell_size_ * 0.5f) / cell_size_) * cell_size_ - cell_size_ * 0.5f;
    const float origin_z = std::floor((world_z + cell_size_ * 0.5f) / cell_size_) * cell_size_ - cell_size_ * 0.5f;
    const float step = cell_size_ / static_cast<float>(resolution_ - 1);
    const float local_x = (world_x - origin_x) / step;
    const float local_z = (world_z - origin_z) / step;
    if (local_x < 0.0f || local_z < 0.0f || local_x > static_cast<float>(resolution_ - 1) ||
        local_z > static_cast<float>(resolution_ - 1))
        return 0;

    const auto x0 = static_cast<std::uint32_t>(std::floor(local_x));
    const auto z0 = static_cast<std::uint32_t>(std::floor(local_z));
    const auto x1 = std::min(x0 + 1, resolution_ - 1);
    const auto z1 = std::min(z0 + 1, resolution_ - 1);
    const float tx = local_x - static_cast<float>(x0);
    const float tz = local_z - static_cast<float>(z0);
    const auto cell = terrain_cell_for_position(world_x, world_z, cell_size_);
    const float f00 = static_cast<float>(fill_at(cell, x0, z0));
    const float f10 = static_cast<float>(fill_at(cell, x1, z0));
    const float f01 = static_cast<float>(fill_at(cell, x0, z1));
    const float f11 = static_cast<float>(fill_at(cell, x1, z1));
    const float fx0 = f00 + (f10 - f00) * tx;
    const float fx1 = f01 + (f11 - f01) * tx;
    const float value = fx0 + (fx1 - fx0) * tz;
    return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 255.0f));
}

std::set<CellCoord> WaterStore::cell_coordinates() const {
    std::set<CellCoord> cells;
    for (const auto& entry : cells_) cells.insert(entry.first);
    return cells;
}

bool WaterStore::in_sea_region(float world_x, float world_z) const {
    for (const auto& region : regions_) {
        if (world_x >= region.min_x && world_x <= region.max_x && world_z >= region.min_z &&
            world_z <= region.max_z)
            return true;
    }
    return false;
}

WaterCellMask& WaterStore::ensure_cell(CellCoord cell) {
    auto& entry = cells_[cell];
    if (entry.fill.empty()) {
        entry.coordinate = cell;
        entry.fill.assign(static_cast<std::size_t>(resolution_) * resolution_, 0);
    }
    return entry;
}

Result<std::set<CellCoord>> WaterStore::apply_fill_brush(float world_x, float world_z, float radius, float strength,
    bool erase) {
    if (!std::isfinite(world_x) || !std::isfinite(world_z) || !std::isfinite(radius) || radius <= 0.0f ||
        !std::isfinite(strength) || strength <= 0.0f)
        return Result<std::set<CellCoord>>::failure(water_error("WATER-BRUSH-INVALID",
            "Water brush parameters must be finite and positive", "Use a positive radius and strength."));
    const float delta = erase ? -strength * 255.0f : strength * 255.0f;
    std::set<CellCoord> touched;
    const int cell_extent = static_cast<int>(std::ceil(radius / cell_size_)) + 1;
    const auto center_cell = terrain_cell_for_position(world_x, world_z, cell_size_);
    for (int dz = -cell_extent; dz <= cell_extent; ++dz) {
        for (int dx = -cell_extent; dx <= cell_extent; ++dx) {
            const CellCoord cell{center_cell.x + dx, center_cell.z + dz};
            const float origin_x = static_cast<float>(cell.x) * cell_size_ - cell_size_ * 0.5f;
            const float origin_z = static_cast<float>(cell.z) * cell_size_ - cell_size_ * 0.5f;
            const float step = cell_size_ / static_cast<float>(resolution_ - 1);
            bool changed = false;
            for (std::uint32_t z = 0; z < resolution_; ++z) {
                for (std::uint32_t x = 0; x < resolution_; ++x) {
                    const float sample_x = origin_x + static_cast<float>(x) * step;
                    const float sample_z = origin_z + static_cast<float>(z) * step;
                    const float distance = std::hypot(sample_x - world_x, sample_z - world_z);
                    if (distance > radius) continue;
                    auto& entry = ensure_cell(cell);
                    const std::size_t index = static_cast<std::size_t>(z) * resolution_ + x;
                    const float amount = delta * brush_falloff(distance, radius);
                    const float next = std::clamp(static_cast<float>(entry.fill[index]) + amount, 0.0f, 255.0f);
                    const auto next_u8 = static_cast<std::uint8_t>(next);
                    if (next_u8 != entry.fill[index]) {
                        entry.fill[index] = next_u8;
                        changed = true;
                    }
                }
            }
            if (changed) {
                touched.insert(cell);
                bool all_zero = true;
                for (const std::uint8_t value : cells_[cell].fill) {
                    if (value > 0) {
                        all_zero = false;
                        break;
                    }
                }
                if (all_zero) cells_.erase(cell);
            }
        }
    }
    return Result<std::set<CellCoord>>::success(std::move(touched));
}

Result<std::set<CellCoord>> WaterStore::apply_place_brush(float world_x, float world_z, float radius, float strength) {
    return apply_fill_brush(world_x, world_z, radius, strength, false);
}

Result<std::set<CellCoord>> WaterStore::apply_erase_brush(float world_x, float world_z, float radius, float strength) {
    return apply_fill_brush(world_x, world_z, radius, strength, true);
}

void WaterStore::set_cell_fill(CellCoord cell, std::vector<std::uint8_t> fill) {
    if (fill.empty()) {
        cells_.erase(cell);
        return;
    }
    auto& entry = ensure_cell(cell);
    entry.fill = std::move(fill);
}

void WaterStore::remove_cell(CellCoord cell) { cells_.erase(cell); }

std::vector<std::uint8_t> WaterStore::cell_fill_or_empty(CellCoord cell) const {
    const auto* entry = find_cell(cell);
    if (!entry) return {};
    return entry->fill;
}

void WaterStore::set_sea_regions(std::vector<WaterSeaRegion> regions) { regions_ = std::move(regions); }

std::optional<float> sample_water_surface_y(float world_x, float world_z) {
    const WaterStore* store = active_water_store();
    if (!store) return std::nullopt;
    if (has_authored_fill(store, world_x, world_z)) return store->sea_level();
    if (store->in_sea_region(world_x, world_z) &&
        sample_terrain_height(world_x, world_z) < store->sea_level())
        return store->sea_level();
    return std::nullopt;
}

bool is_underwater(float world_x, float world_z, float entity_y) {
    const auto surface = sample_water_surface_y(world_x, world_z);
    if (!surface) return false;
    return entity_y < *surface;
}

float water_depth(float world_x, float world_z) {
    const auto surface = sample_water_surface_y(world_x, world_z);
    if (!surface) return 0.0f;
    return std::max(0.0f, *surface - sample_terrain_height(world_x, world_z));
}

bool is_deep_water(float world_x, float world_z) {
    return water_depth(world_x, world_z) >= WaterStore::k_deep_depth;
}

std::string WaterStore::to_json() const {
    nlohmann::json root;
    root["schemaVersion"] = 1;
    root["resolution"] = resolution_;
    root["cellSize"] = cell_size_;
    root["seaLevel"] = sea_level_;
    nlohmann::json regions = nlohmann::json::array();
    for (const auto& region : regions_) {
        regions.push_back({{"id", region.id},
            {"minX", region.min_x},
            {"maxX", region.max_x},
            {"minZ", region.min_z},
            {"maxZ", region.max_z}});
    }
    root["seaRegions"] = std::move(regions);
    nlohmann::json cells = nlohmann::json::array();
    for (const auto& entry : cells_) {
        cells.push_back({{"x", entry.second.coordinate.x},
            {"z", entry.second.coordinate.z},
            {"fill", entry.second.fill}});
    }
    root["cells"] = std::move(cells);
    return root.dump(2);
}

Result<WaterStore> WaterStore::from_json(const std::string& text) {
    WaterStore store;
    nlohmann::json root;
    try {
        root = nlohmann::json::parse(text);
    } catch (const std::exception& ex) {
        return Result<WaterStore>::failure(
            water_error("WATER-JSON-PARSE", ex.what(), "Fix JSON syntax in water-surfaces.json."));
    }
    if (!root.contains("schemaVersion") || root["schemaVersion"].get<int>() != 1)
        return Result<WaterStore>::failure(
            water_error("WATER-SCHEMA", "Unsupported water schema version", "Use schemaVersion 1."));
    store.resolution_ = root.value("resolution", k_resolution);
    store.cell_size_ = root.value("cellSize", k_cell_size);
    store.sea_level_ = root.value("seaLevel", 0.0f);
    if (root.contains("seaRegions")) {
        for (const auto& region : root["seaRegions"]) {
            WaterSeaRegion entry;
            entry.id = region.value("id", std::string{});
            entry.min_x = region.value("minX", 0.0f);
            entry.max_x = region.value("maxX", 0.0f);
            entry.min_z = region.value("minZ", 0.0f);
            entry.max_z = region.value("maxZ", 0.0f);
            store.regions_.push_back(std::move(entry));
        }
    }
    if (root.contains("cells")) {
        for (const auto& cell : root["cells"]) {
            WaterCellMask entry;
            entry.coordinate.x = cell.value("x", 0);
            entry.coordinate.z = cell.value("z", 0);
            entry.fill = cell.value("fill", std::vector<std::uint8_t>{});
            const auto validated = entry.validate(store.resolution_);
            if (!validated) return Result<WaterStore>::failure(validated.error());
            store.cells_[entry.coordinate] = std::move(entry);
        }
    }
    return Result<WaterStore>::success(std::move(store));
}

Result<WaterStore> WaterStore::load(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        if (!std::filesystem::exists(path)) return Result<WaterStore>::success(WaterStore{});
        return Result<WaterStore>::failure(
            water_error("WATER-LOAD", "Could not open water surfaces file", "Check file permissions."));
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return from_json(buffer.str());
}

Result<void> WaterStore::save_atomic(const std::filesystem::path& path) const {
    const auto parent = path.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    const auto temp = path.string() + ".tmp";
    std::ofstream output(temp, std::ios::binary | std::ios::trunc);
    if (!output)
        return Result<void>::failure(
            water_error("WATER-SAVE", "Could not write temporary water file", "Check disk space and permissions."));
    output << to_json();
    output.flush();
    if (!output)
        return Result<void>::failure(water_error("WATER-SAVE", "Failed while writing water file", "Retry save."));
    output.close();
    std::error_code ec;
    std::filesystem::rename(temp, path, ec);
    if (ec)
        return Result<void>::failure(
            water_error("WATER-SAVE", "Could not finalize water file rename", ec.message()));
    return Result<void>::success();
}

Result<void> WaterStore::validate_file(const std::filesystem::path& path) {
    const auto loaded = load(path);
    if (!loaded) return Result<void>::failure(loaded.error());
    return Result<void>::success();
}

} // namespace engine
