#include "engine/world/foliage_density.h"

#include "engine/world/foliage_layers.h"
#include "engine/world/terrain.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <array>
#include <windows.h>

namespace engine {
namespace {

EngineError foliage_density_error(std::string code, std::string message, std::string remediation) {
    return {std::move(code), Severity::Error, ErrorCategory::Validation, "foliage-density", std::move(message),
            ENGINE_SOURCE_CONTEXT, {}, std::move(remediation), make_correlation_id()};
}

[[nodiscard]] float brush_falloff(float distance, float radius) {
    if (radius <= 0.0f) return 0.0f;
    const float t = std::clamp(1.0f - distance / radius, 0.0f, 1.0f);
    return t * t;
}

std::uint32_t mixed_layer_hash(float sample_x, float sample_z) noexcept {
    const auto x_bits = static_cast<std::uint32_t>(sample_x * 4096.0f);
    const auto z_bits = static_cast<std::uint32_t>(sample_z * 4096.0f);
    std::uint32_t seed = x_bits * 73856093u ^ z_bits * 19349663u;
    seed ^= seed >> 16;
    seed *= 0x7feb352du;
    seed ^= seed >> 15;
    seed *= 0x846ca68bu;
    seed ^= seed >> 16;
    return seed;
}

float mixed_layer_unit(float sample_x, float sample_z) noexcept {
    return static_cast<float>(mixed_layer_hash(sample_x, sample_z)) / static_cast<float>(0xffffffffu);
}

std::uint8_t pick_mixed_layer(float sample_x, float sample_z, const std::vector<FoliageMixedLayerWeight>& layers) {
    if (layers.empty()) return 0;
    if (layers.size() == 1) return layers.front().layer_index;
    float total = 0.0f;
    for (const auto& layer : layers) total += layer.weight;
    if (total <= 0.0f) return layers.front().layer_index;
    const float unit = mixed_layer_unit(sample_x, sample_z) * total;
    float cumulative = 0.0f;
    for (const auto& layer : layers) {
        cumulative += layer.weight;
        if (unit <= cumulative) return layer.layer_index;
    }
    return layers.back().layer_index;
}

} // namespace

std::filesystem::path default_foliage_density_path(const std::filesystem::path& project_root) {
    return project_root / "assets/terrain/foliage-density.json";
}

bool FoliageDensityStore::has_cell(CellCoord cell) const { return cells_.find(cell) != cells_.end(); }

const FoliageDensityCell* FoliageDensityStore::find_cell(CellCoord cell) const {
    const auto found = cells_.find(cell);
    return found == cells_.end() ? nullptr : &found->second;
}

std::uint8_t FoliageDensityStore::density_at(CellCoord cell, std::uint32_t x, std::uint32_t z) const {
    const auto* entry = find_cell(cell);
    if (!entry || x >= resolution_ || z >= resolution_) return 0;
    return entry->density[static_cast<std::size_t>(z) * resolution_ + x];
}

std::uint8_t FoliageDensityStore::layer_at(CellCoord cell, std::uint32_t x, std::uint32_t z) const {
    const auto* entry = find_cell(cell);
    if (!entry || x >= resolution_ || z >= resolution_) return 0;
    return entry->layer[static_cast<std::size_t>(z) * resolution_ + x];
}

std::set<CellCoord> FoliageDensityStore::cell_coordinates() const {
    std::set<CellCoord> cells;
    for (const auto& entry : cells_) cells.insert(entry.first);
    return cells;
}

FoliageDensityCell& FoliageDensityStore::ensure_cell(CellCoord cell) {
    auto& entry = cells_[cell];
    if (entry.density.empty()) {
        entry.coordinate = cell;
        const auto count = static_cast<std::size_t>(resolution_) * resolution_;
        entry.density.assign(count, 0);
        entry.layer.assign(count, 0);
    }
    return entry;
}

Result<std::set<CellCoord>> FoliageDensityStore::apply_foliage_brush(float world_x, float world_z, float radius,
    float strength, std::uint8_t layer_index, bool erase) {
    if (!std::isfinite(world_x) || !std::isfinite(world_z) || !std::isfinite(radius) || radius <= 0.0f ||
        !std::isfinite(strength) || strength <= 0.0f)
        return Result<std::set<CellCoord>>::failure(foliage_density_error("FOLIAGE-BRUSH-INVALID",
            "Foliage brush parameters must be finite and positive", "Use positive radius and strength."));
    const int delta = erase ? -1 : 1;
    const float step_amount = std::clamp(strength * 255.0f, 1.0f, 64.0f);
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
                    const float falloff = brush_falloff(distance, radius);
                    if (falloff < 0.05f) continue;
                    auto& entry = ensure_cell(cell);
                    const std::size_t index = static_cast<std::size_t>(z) * resolution_ + x;
                    const int updated = static_cast<int>(entry.density[index]) +
                        static_cast<int>(std::lround(step_amount * falloff)) * delta;
                    const auto clamped = static_cast<std::uint8_t>(std::clamp(updated, 0, 255));
                    if (clamped != entry.density[index] || (clamped > 0 && entry.layer[index] != layer_index)) {
                        entry.density[index] = clamped;
                        if (clamped > 0) entry.layer[index] = layer_index;
                        changed = true;
                    }
                }
            }
            if (changed) {
                touched.insert(cell);
                bool all_zero = true;
                for (std::uint8_t value : cells_[cell].density) {
                    if (value != 0) {
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

Result<std::set<CellCoord>> FoliageDensityStore::apply_foliage_mixed_brush(float world_x, float world_z, float radius,
    float strength, const std::vector<FoliageMixedLayerWeight>& layers, bool erase) {
    if (layers.empty())
        return Result<std::set<CellCoord>>::failure(foliage_density_error("FOLIAGE-MIXED-EMPTY",
            "Mixed foliage brush requires at least one layer weight", "Load foliage layers before painting."));
    if (!std::isfinite(world_x) || !std::isfinite(world_z) || !std::isfinite(radius) || radius <= 0.0f ||
        !std::isfinite(strength) || strength <= 0.0f)
        return Result<std::set<CellCoord>>::failure(foliage_density_error("FOLIAGE-BRUSH-INVALID",
            "Foliage brush parameters must be finite and positive", "Use positive radius and strength."));
    const int delta = erase ? -1 : 1;
    const float step_amount = std::clamp(strength * 255.0f, 1.0f, 64.0f);
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
                    const float falloff = brush_falloff(distance, radius);
                    if (falloff < 0.05f) continue;
                    auto& entry = ensure_cell(cell);
                    const std::size_t index = static_cast<std::size_t>(z) * resolution_ + x;
                    const int updated = static_cast<int>(entry.density[index]) +
                        static_cast<int>(std::lround(step_amount * falloff)) * delta;
                    const auto clamped = static_cast<std::uint8_t>(std::clamp(updated, 0, 255));
                    const std::uint8_t layer_index =
                        erase ? entry.layer[index] : pick_mixed_layer(sample_x, sample_z, layers);
                    if (clamped != entry.density[index] || (clamped > 0 && entry.layer[index] != layer_index)) {
                        entry.density[index] = clamped;
                        if (clamped > 0) entry.layer[index] = layer_index;
                        changed = true;
                    }
                }
            }
            if (changed) {
                touched.insert(cell);
                bool all_zero = true;
                for (std::uint8_t value : cells_[cell].density) {
                    if (value != 0) {
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

void FoliageDensityStore::set_cell(CellCoord cell, FoliageCellSnapshot snapshot) {
    if (snapshot.density.empty()) {
        cells_.erase(cell);
        return;
    }
    FoliageDensityCell entry{cell, std::move(snapshot.density), std::move(snapshot.layer)};
    cells_[cell] = std::move(entry);
}

void FoliageDensityStore::remove_cell(CellCoord cell) { cells_.erase(cell); }

FoliageCellSnapshot FoliageDensityStore::cell_snapshot_or_empty(CellCoord cell) const {
    const auto* entry = find_cell(cell);
    if (!entry) return {};
    return {entry->density, entry->layer};
}

std::string FoliageDensityStore::to_json() const {
    nlohmann::ordered_json root{{"schemaVersion", 1}, {"resolution", resolution_}, {"cellSize", cell_size_},
        {"cells", nlohmann::ordered_json::array()}};
    for (const auto& entry : cells_) {
        root["cells"].push_back({{"x", entry.second.coordinate.x}, {"z", entry.second.coordinate.z},
            {"density", entry.second.density}, {"layer", entry.second.layer}});
    }
    return root.dump(2) + "\n";
}

Result<FoliageDensityStore> FoliageDensityStore::from_json(const std::string& text) {
    try {
        const auto root = nlohmann::json::parse(text);
        if (root.value("schemaVersion", 0) != 1 || !root["cells"].is_array())
            return Result<FoliageDensityStore>::failure(foliage_density_error("FOLIAGE-DENSITY-SCHEMA",
                "Foliage density schema is invalid", "Use schemaVersion 1 with a cells array."));
        FoliageDensityStore store;
        store.resolution_ = root.value("resolution", k_resolution);
        store.cell_size_ = root.value("cellSize", k_cell_size);
        if (store.resolution_ != k_resolution || !std::isfinite(store.cell_size_) || store.cell_size_ <= 0.0f)
            return Result<FoliageDensityStore>::failure(foliage_density_error("FOLIAGE-DENSITY-SCHEMA",
                "Foliage density resolution or cell size is unsupported", "Use resolution 33 and cellSize 40."));
        const auto expected = static_cast<std::size_t>(store.resolution_) * store.resolution_;
        for (const auto& cell_json : root["cells"]) {
            FoliageDensityCell cell{{cell_json.at("x").get<std::int32_t>(), cell_json.at("z").get<std::int32_t>()},
                cell_json.at("density").get<std::vector<std::uint8_t>>(),
                cell_json.at("layer").get<std::vector<std::uint8_t>>()};
            if (cell.density.size() != expected || cell.layer.size() != expected)
                return Result<FoliageDensityStore>::failure(foliage_density_error("FOLIAGE-DENSITY-COUNT",
                    "Foliage density sample count does not match resolution",
                    "Provide density and layer for every height sample."));
            store.cells_[cell.coordinate] = std::move(cell);
        }
        return Result<FoliageDensityStore>::success(std::move(store));
    } catch (const std::exception& exception) {
        auto error = foliage_density_error("FOLIAGE-DENSITY-PARSE", "Foliage density JSON is malformed",
            "Correct the foliage density file structure.");
        error.causes.push_back(exception.what());
        return Result<FoliageDensityStore>::failure(std::move(error));
    }
}

Result<FoliageDensityStore> FoliageDensityStore::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return Result<FoliageDensityStore>::failure(foliage_density_error("FOLIAGE-DENSITY-READ",
            "Could not read foliage density: " + path.generic_string(), "Check the foliage density path."));
    std::ostringstream text;
    text << input.rdbuf();
    return from_json(text.str());
}

Result<void> FoliageDensityStore::save_atomic(const std::filesystem::path& path) const {
    try {
        if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
        const auto temp = std::filesystem::path(path.string() + ".tmp");
        {
            std::ofstream output(temp, std::ios::binary | std::ios::trunc);
            output << to_json();
            if (!output) throw std::runtime_error("write");
        }
        if (std::filesystem::exists(path)) {
            const auto backup = std::filesystem::path(path.string() + ".bak");
            if (!ReplaceFileW(path.c_str(), temp.c_str(), backup.c_str(), REPLACEFILE_WRITE_THROUGH, nullptr, nullptr))
                throw std::runtime_error("replace");
        } else if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH)) {
            throw std::runtime_error("move");
        }
        return Result<void>::success();
    } catch (const std::exception& exception) {
        auto error = foliage_density_error("FOLIAGE-DENSITY-WRITE", "Foliage density atomic save failed",
            "Check file permissions and disk space.");
        error.causes.push_back(exception.what());
        return Result<void>::failure(std::move(error));
    }
}

Result<void> FoliageDensityStore::validate_file(const std::filesystem::path& path, std::uint8_t max_layer_index) {
    if (!std::filesystem::exists(path)) return Result<void>::success();
    const auto loaded = load(path);
    if (!loaded) return Result<void>::failure(loaded.error());
    for (const auto& cell_coord : loaded.value().cell_coordinates()) {
        const auto* cell = loaded.value().find_cell(cell_coord);
        if (!cell) continue;
        for (std::uint8_t layer : cell->layer) {
            if (layer > max_layer_index)
                return Result<void>::failure(foliage_density_error("FOLIAGE-DENSITY-LAYER-INVALID",
                    "Foliage density layer index exceeds palette size", "Repaint or expand the foliage layer palette."));
        }
    }
    return Result<void>::success();
}

FoliageDensityBrushStrokeCommand::FoliageDensityBrushStrokeCommand(std::map<CellCoord, FoliageCellSnapshot> before,
    std::map<CellCoord, FoliageCellSnapshot> after)
    : before_(std::move(before)), after_(std::move(after)) {}

Result<void> FoliageDensityBrushStrokeCommand::apply(FoliageDensityStore& store) {
    for (const auto& entry : after_) store.set_cell(entry.first, entry.second);
    return Result<void>::success();
}

Result<void> FoliageDensityBrushStrokeCommand::revert(FoliageDensityStore& store) {
    for (const auto& entry : before_) {
        if (entry.second.density.empty()) store.remove_cell(entry.first);
        else store.set_cell(entry.first, entry.second);
    }
    return Result<void>::success();
}

Result<void> FoliageDensityHistory::execute(FoliageDensityStore& store, std::unique_ptr<FoliageDensityCommand> command) {
    if (!command)
        return Result<void>::failure(foliage_density_error("FOLIAGE-COMMAND-EMPTY", "Foliage command is missing",
            "Provide a foliage command."));
    const auto applied = command->apply(store);
    if (!applied) return applied;
    undo_.push_back(std::move(command));
    redo_.clear();
    last_summary_ = undo_.back()->label();
    return Result<void>::success();
}

Result<void> FoliageDensityHistory::undo(FoliageDensityStore& store) {
    if (undo_.empty())
        return Result<void>::failure(foliage_density_error("FOLIAGE-UNDO-EMPTY", "Nothing to undo",
            "Paint foliage before undoing."));
    const auto reverted = undo_.back()->revert(store);
    if (!reverted) return reverted;
    redo_.push_back(std::move(undo_.back()));
    undo_.pop_back();
    last_summary_ = "Undid foliage density stroke";
    return Result<void>::success();
}

Result<void> FoliageDensityHistory::redo(FoliageDensityStore& store) {
    if (redo_.empty())
        return Result<void>::failure(foliage_density_error("FOLIAGE-REDO-EMPTY", "Nothing to redo",
            "Undo a foliage stroke before redoing."));
    const auto applied = redo_.back()->apply(store);
    if (!applied) return applied;
    undo_.push_back(std::move(redo_.back()));
    redo_.pop_back();
    last_summary_ = "Redid foliage density stroke";
    return Result<void>::success();
}

std::vector<FoliageMixedLayerWeight> default_meadow_mix_weights(const FoliageLayerPalette& palette) {
    static constexpr std::array<std::pair<const char*, float>, 3> k_entries{
        std::pair{"grass", 0.72f}, std::pair{"flower", 0.18f}, std::pair{"bush", 0.10f}};
    std::vector<FoliageMixedLayerWeight> weights;
    weights.reserve(k_entries.size());
    for (std::uint8_t index = 0; index < palette.layers.size(); ++index) {
        for (const auto& entry : k_entries) {
            if (palette.layers[index].id == entry.first) {
                weights.push_back({index, entry.second});
                break;
            }
        }
    }
    if (weights.empty()) {
        for (std::uint8_t index = 0; index < palette.layers.size(); ++index)
            weights.push_back({index, 1.0f});
    }
    return weights;
}

} // namespace engine
