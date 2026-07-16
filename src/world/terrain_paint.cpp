#include "engine/world/terrain_paint.h"

#include "engine/world/terrain.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <windows.h>

namespace engine {
namespace {

EngineError terrain_paint_error(std::string code, std::string message, std::string remediation) {
    return {std::move(code), Severity::Error, ErrorCategory::Validation, "terrain-paint", std::move(message),
            ENGINE_SOURCE_CONTEXT, {}, std::move(remediation), make_correlation_id()};
}

[[nodiscard]] float brush_falloff(float distance, float radius) {
    if (radius <= 0.0f) return 0.0f;
    const float t = std::clamp(1.0f - distance / radius, 0.0f, 1.0f);
    return t * t;
}

std::string normalize_material_path(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    std::transform(path.begin(), path.end(), path.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return path;
}

} // namespace

std::filesystem::path default_terrain_paint_path(const std::filesystem::path& project_root) {
    return project_root / "assets/terrain/terrain-paint.json";
}

bool TerrainPaintStore::has_cell(CellCoord cell) const { return cells_.find(cell) != cells_.end(); }

const TerrainPaintCell* TerrainPaintStore::find_cell(CellCoord cell) const {
    const auto found = cells_.find(cell);
    return found == cells_.end() ? nullptr : &found->second;
}

std::uint16_t TerrainPaintStore::index_at(CellCoord cell, std::uint32_t x, std::uint32_t z) const {
    const auto* entry = find_cell(cell);
    if (!entry || x >= resolution_ || z >= resolution_) return 0;
    return entry->indices[static_cast<std::size_t>(z) * resolution_ + x];
}

std::uint16_t TerrainPaintStore::sample_index_world(float world_x, float world_z) const {
    if (cells_.empty()) return 0;
    const float origin_x = std::floor((world_x + cell_size_ * 0.5f) / cell_size_) * cell_size_ - cell_size_ * 0.5f;
    const float origin_z = std::floor((world_z + cell_size_ * 0.5f) / cell_size_) * cell_size_ - cell_size_ * 0.5f;
    const float step = cell_size_ / static_cast<float>(resolution_ - 1);
    const float local_x = (world_x - origin_x) / step;
    const float local_z = (world_z - origin_z) / step;
    if (local_x < 0.0f || local_z < 0.0f || local_x > static_cast<float>(resolution_ - 1) ||
        local_z > static_cast<float>(resolution_ - 1))
        return 0;
    const auto x = static_cast<std::uint32_t>(std::lround(local_x));
    const auto z = static_cast<std::uint32_t>(std::lround(local_z));
    return index_at(terrain_cell_for_position(world_x, world_z, cell_size_), x, z);
}

std::set<CellCoord> TerrainPaintStore::cell_coordinates() const {
    std::set<CellCoord> cells;
    for (const auto& entry : cells_) cells.insert(entry.first);
    return cells;
}

const std::string* TerrainPaintStore::material_path_for_index(std::uint16_t index) const {
    if (index == 0 || index > materials_.size()) return nullptr;
    return &materials_[static_cast<std::size_t>(index - 1)];
}

std::uint16_t TerrainPaintStore::ensure_material_index(const std::string& material_path) {
    const auto normalized = normalize_material_path(material_path);
    for (std::size_t i = 0; i < materials_.size(); ++i) {
        if (materials_[i] == normalized) return static_cast<std::uint16_t>(i + 1);
    }
    materials_.push_back(normalized);
    return static_cast<std::uint16_t>(materials_.size());
}

TerrainPaintCell& TerrainPaintStore::ensure_cell(CellCoord cell) {
    auto& entry = cells_[cell];
    if (entry.indices.empty()) {
        entry.coordinate = cell;
        entry.indices.assign(static_cast<std::size_t>(resolution_) * resolution_, 0);
    }
    return entry;
}

Result<std::set<CellCoord>> TerrainPaintStore::apply_material_brush(float world_x, float world_z, float radius,
    std::uint16_t material_index) {
    if (!std::isfinite(world_x) || !std::isfinite(world_z) || !std::isfinite(radius) || radius <= 0.0f)
        return Result<std::set<CellCoord>>::failure(terrain_paint_error("TERRAIN-PAINT-BRUSH-INVALID",
            "Terrain paint brush parameters must be finite and positive", "Use a positive radius."));
    if (material_index == 0)
        return Result<std::set<CellCoord>>::failure(terrain_paint_error("TERRAIN-PAINT-MATERIAL-INVALID",
            "Terrain paint requires a material selection", "Choose a material before painting."));
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
                    if (brush_falloff(distance, radius) < 0.05f) continue;
                    auto& entry = ensure_cell(cell);
                    const std::size_t index = static_cast<std::size_t>(z) * resolution_ + x;
                    if (entry.indices[index] != material_index) {
                        entry.indices[index] = material_index;
                        changed = true;
                    }
                }
            }
            if (changed) {
                touched.insert(cell);
                bool all_zero = true;
                for (std::uint16_t value : cells_[cell].indices) {
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

void TerrainPaintStore::set_cell_indices(CellCoord cell, std::vector<std::uint16_t> indices) {
    if (indices.empty()) {
        cells_.erase(cell);
        return;
    }
    TerrainPaintCell entry{cell, std::move(indices)};
    cells_[cell] = std::move(entry);
}

void TerrainPaintStore::remove_cell(CellCoord cell) { cells_.erase(cell); }

std::vector<std::uint16_t> TerrainPaintStore::cell_indices_or_empty(CellCoord cell) const {
    const auto* entry = find_cell(cell);
    if (!entry) return {};
    return entry->indices;
}

std::string TerrainPaintStore::to_json() const {
    nlohmann::ordered_json root{{"schemaVersion", 1}, {"resolution", resolution_}, {"cellSize", cell_size_},
        {"materials", materials_}, {"cells", nlohmann::ordered_json::array()}};
    for (const auto& entry : cells_) {
        root["cells"].push_back(
            {{"x", entry.second.coordinate.x}, {"z", entry.second.coordinate.z}, {"indices", entry.second.indices}});
    }
    return root.dump(2) + "\n";
}

Result<TerrainPaintStore> TerrainPaintStore::from_json(const std::string& text) {
    try {
        const auto root = nlohmann::json::parse(text);
        if (root.value("schemaVersion", 0) != 1 || !root["cells"].is_array())
            return Result<TerrainPaintStore>::failure(terrain_paint_error("TERRAIN-PAINT-SCHEMA",
                "Terrain paint schema is invalid", "Use schemaVersion 1 with a cells array."));
        TerrainPaintStore store;
        store.resolution_ = root.value("resolution", k_resolution);
        store.cell_size_ = root.value("cellSize", k_cell_size);
        if (store.resolution_ != k_resolution || !std::isfinite(store.cell_size_) || store.cell_size_ <= 0.0f)
            return Result<TerrainPaintStore>::failure(terrain_paint_error("TERRAIN-PAINT-SCHEMA",
                "Terrain paint resolution or cell size is unsupported", "Use resolution 33 and cellSize 40."));
        if (root.contains("materials") && root["materials"].is_array()) {
            for (const auto& material : root["materials"])
                store.materials_.push_back(normalize_material_path(material.get<std::string>()));
        }
        const auto expected = static_cast<std::size_t>(store.resolution_) * store.resolution_;
        for (const auto& cell_json : root["cells"]) {
            TerrainPaintCell cell{{cell_json.at("x").get<std::int32_t>(), cell_json.at("z").get<std::int32_t>()},
                cell_json.at("indices").get<std::vector<std::uint16_t>>()};
            if (cell.indices.size() != expected)
                return Result<TerrainPaintStore>::failure(terrain_paint_error("TERRAIN-PAINT-COUNT",
                    "Terrain paint sample count does not match resolution",
                    "Provide indices for every height sample."));
            for (const std::uint16_t index : cell.indices) {
                if (index > store.materials_.size())
                    return Result<TerrainPaintStore>::failure(terrain_paint_error("TERRAIN-PAINT-INDEX-INVALID",
                        "Terrain paint material index is out of range", "Correct material palette indices."));
            }
            store.cells_[cell.coordinate] = std::move(cell);
        }
        return Result<TerrainPaintStore>::success(std::move(store));
    } catch (const std::exception& exception) {
        auto error = terrain_paint_error("TERRAIN-PAINT-PARSE", "Terrain paint JSON is malformed",
            "Correct the terrain paint file structure.");
        error.causes.push_back(exception.what());
        return Result<TerrainPaintStore>::failure(std::move(error));
    }
}

Result<TerrainPaintStore> TerrainPaintStore::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return Result<TerrainPaintStore>::failure(terrain_paint_error("TERRAIN-PAINT-READ",
            "Could not read terrain paint: " + path.generic_string(), "Check the terrain paint path."));
    std::ostringstream text;
    text << input.rdbuf();
    return from_json(text.str());
}

Result<void> TerrainPaintStore::save_atomic(const std::filesystem::path& path) const {
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
        auto error = terrain_paint_error("TERRAIN-PAINT-WRITE", "Terrain paint atomic save failed",
            "Check file permissions and disk space.");
        error.causes.push_back(exception.what());
        return Result<void>::failure(std::move(error));
    }
}

Result<void> TerrainPaintStore::validate_file(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return Result<void>::success();
    const auto loaded = load(path);
    if (!loaded) return Result<void>::failure(loaded.error());
    return Result<void>::success();
}

TerrainPaintBrushStrokeCommand::TerrainPaintBrushStrokeCommand(
    std::map<CellCoord, std::vector<std::uint16_t>> before, std::map<CellCoord, std::vector<std::uint16_t>> after)
    : before_(std::move(before)), after_(std::move(after)) {}

Result<void> TerrainPaintBrushStrokeCommand::apply(TerrainPaintStore& store) {
    for (const auto& entry : after_) store.set_cell_indices(entry.first, entry.second);
    return Result<void>::success();
}

Result<void> TerrainPaintBrushStrokeCommand::revert(TerrainPaintStore& store) {
    for (const auto& entry : before_) {
        if (entry.second.empty()) store.remove_cell(entry.first);
        else store.set_cell_indices(entry.first, entry.second);
    }
    return Result<void>::success();
}

Result<void> TerrainPaintHistory::execute(TerrainPaintStore& store, std::unique_ptr<TerrainPaintCommand> command) {
    if (!command) return Result<void>::failure(terrain_paint_error("TERRAIN-PAINT-COMMAND-EMPTY",
        "Terrain paint command is missing", "Provide a paint command."));
    const auto applied = command->apply(store);
    if (!applied) return applied;
    undo_.push_back(std::move(command));
    redo_.clear();
    last_summary_ = undo_.back()->label();
    return Result<void>::success();
}

Result<void> TerrainPaintHistory::undo(TerrainPaintStore& store) {
    if (undo_.empty())
        return Result<void>::failure(terrain_paint_error("TERRAIN-PAINT-UNDO-EMPTY", "Nothing to undo",
            "Paint on terrain before undoing."));
    const auto reverted = undo_.back()->revert(store);
    if (!reverted) return reverted;
    redo_.push_back(std::move(undo_.back()));
    undo_.pop_back();
    last_summary_ = "Undid terrain paint stroke";
    return Result<void>::success();
}

Result<void> TerrainPaintHistory::redo(TerrainPaintStore& store) {
    if (redo_.empty())
        return Result<void>::failure(terrain_paint_error("TERRAIN-PAINT-REDO-EMPTY", "Nothing to redo",
            "Undo a paint stroke before redoing."));
    const auto applied = redo_.back()->apply(store);
    if (!applied) return applied;
    undo_.push_back(std::move(redo_.back()));
    redo_.pop_back();
    last_summary_ = "Redid terrain paint stroke";
    return Result<void>::success();
}

} // namespace engine
