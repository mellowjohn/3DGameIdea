#include "engine/world/terrain_edits.h"

#include "engine/world/terrain.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <sstream>
#include <windows.h>

namespace engine {
namespace {
const TerrainEditStore* g_active_edits = nullptr;

EngineError terrain_edit_error(std::string code, std::string message, std::string remediation) {
    return {std::move(code), Severity::Error, ErrorCategory::Validation, "terrain-edits", std::move(message),
            ENGINE_SOURCE_CONTEXT, {}, std::move(remediation), make_correlation_id()};
}

[[nodiscard]] float brush_falloff(float distance, float radius) {
    if (radius <= 0.0f) return 0.0f;
    const float t = std::clamp(1.0f - distance / radius, 0.0f, 1.0f);
    return t * t;
}
} // namespace

void set_active_terrain_edits(const TerrainEditStore* store) noexcept { g_active_edits = store; }
const TerrainEditStore* active_terrain_edits() noexcept { return g_active_edits; }

std::filesystem::path default_terrain_edits_path(const std::filesystem::path& project_root) {
    return project_root / "assets/terrain/terrain-edits.json";
}

Result<void> TerrainCellEdits::validate(std::uint32_t expected_resolution) const {
    const auto expected = static_cast<std::size_t>(expected_resolution) * expected_resolution;
    if (deltas.size() != expected)
        return Result<void>::failure(terrain_edit_error("TERRAIN-EDIT-COUNT",
            "Terrain edit sample count does not match resolution", "Provide deltas for every height sample."));
    for (const float delta : deltas) {
        if (!std::isfinite(delta) || std::abs(delta) > TerrainEditStore::k_max_delta)
            return Result<void>::failure(terrain_edit_error("TERRAIN-EDIT-DELTA-INVALID",
                "Terrain edit delta is out of range", "Keep deltas finite and within +/- 32 meters."));
    }
    return Result<void>::success();
}

bool TerrainEditStore::has_cell(CellCoord cell) const { return cells_.find(cell) != cells_.end(); }

const TerrainCellEdits* TerrainEditStore::find_cell(CellCoord cell) const {
    const auto found = cells_.find(cell);
    return found == cells_.end() ? nullptr : &found->second;
}

float TerrainEditStore::delta_at(CellCoord cell, std::uint32_t x, std::uint32_t z) const {
    const auto* entry = find_cell(cell);
    if (!entry || x >= resolution_ || z >= resolution_) return 0.0f;
    return entry->deltas[static_cast<std::size_t>(z) * resolution_ + x];
}

float TerrainEditStore::sample_delta_world(float world_x, float world_z) const {
    if (cells_.empty()) return 0.0f;
    const float origin_x = std::floor((world_x + cell_size_ * 0.5f) / cell_size_) * cell_size_ - cell_size_ * 0.5f;
    const float origin_z = std::floor((world_z + cell_size_ * 0.5f) / cell_size_) * cell_size_ - cell_size_ * 0.5f;
    const float step = cell_size_ / static_cast<float>(resolution_ - 1);
    const float local_x = (world_x - origin_x) / step;
    const float local_z = (world_z - origin_z) / step;
    if (local_x < 0.0f || local_z < 0.0f || local_x > static_cast<float>(resolution_ - 1) ||
        local_z > static_cast<float>(resolution_ - 1))
        return 0.0f;

    const auto x0 = static_cast<std::uint32_t>(std::floor(local_x));
    const auto z0 = static_cast<std::uint32_t>(std::floor(local_z));
    const auto x1 = std::min(x0 + 1, resolution_ - 1);
    const auto z1 = std::min(z0 + 1, resolution_ - 1);
    const float tx = local_x - static_cast<float>(x0);
    const float tz = local_z - static_cast<float>(z0);
    const auto cell = terrain_cell_for_position(world_x, world_z, cell_size_);
    const float d00 = delta_at(cell, x0, z0);
    const float d10 = delta_at(cell, x1, z0);
    const float d01 = delta_at(cell, x0, z1);
    const float d11 = delta_at(cell, x1, z1);
    const float dx0 = d00 + (d10 - d00) * tx;
    const float dx1 = d01 + (d11 - d01) * tx;
    return dx0 + (dx1 - dx0) * tz;
}

std::set<CellCoord> TerrainEditStore::cell_coordinates() const {
    std::set<CellCoord> cells;
    for (const auto& entry : cells_) cells.insert(entry.first);
    return cells;
}

TerrainCellEdits& TerrainEditStore::ensure_cell(CellCoord cell) {
    auto& entry = cells_[cell];
    if (entry.deltas.empty()) {
        entry.coordinate = cell;
        entry.deltas.assign(static_cast<std::size_t>(resolution_) * resolution_, 0.0f);
    }
    return entry;
}

Result<std::set<CellCoord>> TerrainEditStore::apply_brush(float world_x, float world_z, float radius, float strength,
    bool lower) {
    if (!std::isfinite(world_x) || !std::isfinite(world_z) || !std::isfinite(radius) || radius <= 0.0f ||
        !std::isfinite(strength) || strength <= 0.0f)
        return Result<std::set<CellCoord>>::failure(terrain_edit_error("TERRAIN-BRUSH-INVALID",
            "Terrain brush parameters must be finite and positive", "Use a positive radius and strength."));
    const float signed_strength = lower ? -strength : strength;
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
                    const float delta = signed_strength * brush_falloff(distance, radius);
                    const float next = std::clamp(entry.deltas[index] + delta, -k_max_delta, k_max_delta);
                    if (std::abs(next - entry.deltas[index]) > 0.00001f) {
                        entry.deltas[index] = next;
                        changed = true;
                    }
                }
            }
            if (changed) {
                touched.insert(cell);
                bool all_zero = true;
                for (float value : cells_[cell].deltas) {
                    if (std::abs(value) > 0.00001f) {
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

Result<std::set<CellCoord>> TerrainEditStore::apply_flatten_brush(float world_x, float world_z, float radius,
    float strength, float target_height) {
    if (!std::isfinite(world_x) || !std::isfinite(world_z) || !std::isfinite(radius) || radius <= 0.0f ||
        !std::isfinite(strength) || strength <= 0.0f || !std::isfinite(target_height))
        return Result<std::set<CellCoord>>::failure(terrain_edit_error("TERRAIN-FLATTEN-INVALID",
            "Terrain flatten parameters must be finite and positive",
            "Use a positive radius/strength and a finite targetHeight."));
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
                    const float procedural = procedural_terrain_height(sample_x, sample_z);
                    const float desired_delta = std::clamp(target_height - procedural, -k_max_delta, k_max_delta);
                    const float diff = desired_delta - entry.deltas[index];
                    const float max_step = strength * brush_falloff(distance, radius);
                    const float step_delta = std::clamp(diff, -max_step, max_step);
                    const float next = std::clamp(entry.deltas[index] + step_delta, -k_max_delta, k_max_delta);
                    if (std::abs(next - entry.deltas[index]) > 0.00001f) {
                        entry.deltas[index] = next;
                        changed = true;
                    }
                }
            }
            if (changed) {
                touched.insert(cell);
                bool all_zero = true;
                for (float value : cells_[cell].deltas) {
                    if (std::abs(value) > 0.00001f) {
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

Result<std::set<CellCoord>> TerrainEditStore::apply_set_height_brush(float world_x, float world_z, float radius,
    float strength, float target_height) {
    if (!std::isfinite(world_x) || !std::isfinite(world_z) || !std::isfinite(radius) || radius <= 0.0f ||
        !std::isfinite(strength) || strength <= 0.0f || !std::isfinite(target_height))
        return Result<std::set<CellCoord>>::failure(terrain_edit_error("TERRAIN-SET-HEIGHT-INVALID",
            "Terrain set_height parameters must be finite and positive",
            "Use a positive radius/strength and a finite targetHeight."));
    const float blend = std::clamp(strength, 0.0f, 1.0f);
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
                    const float procedural = procedural_terrain_height(sample_x, sample_z);
                    const float desired_delta = std::clamp(target_height - procedural, -k_max_delta, k_max_delta);
                    const float t = blend * brush_falloff(distance, radius);
                    const float next =
                        std::clamp(entry.deltas[index] + (desired_delta - entry.deltas[index]) * t, -k_max_delta,
                            k_max_delta);
                    if (std::abs(next - entry.deltas[index]) > 0.00001f) {
                        entry.deltas[index] = next;
                        changed = true;
                    }
                }
            }
            if (changed) {
                touched.insert(cell);
                bool all_zero = true;
                for (float value : cells_[cell].deltas) {
                    if (std::abs(value) > 0.00001f) {
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

Result<std::set<CellCoord>> TerrainEditStore::apply_grade_strip(
    const std::vector<std::array<float, 3>>& path_samples, float half_width, float skirt_width, float strength) {
    if (path_samples.empty())
        return Result<std::set<CellCoord>>::failure(terrain_edit_error("TERRAIN-GRADE-STRIP-EMPTY",
            "Grade strip requires at least one path sample", "Provide densified path points with heights."));
    if (!std::isfinite(half_width) || half_width <= 0.0f || !std::isfinite(skirt_width) || skirt_width < 0.0f ||
        !std::isfinite(strength) || strength <= 0.0f)
        return Result<std::set<CellCoord>>::failure(terrain_edit_error("TERRAIN-GRADE-STRIP-INVALID",
            "Grade strip halfWidth/skirtWidth/strength must be finite and valid",
            "Use positive halfWidth, non-negative skirtWidth, and positive strength."));
    for (const auto& sample : path_samples) {
        if (!std::isfinite(sample[0]) || !std::isfinite(sample[1]) || !std::isfinite(sample[2]))
            return Result<std::set<CellCoord>>::failure(terrain_edit_error("TERRAIN-GRADE-STRIP-FINITE",
                "Grade strip path samples must be finite", "Provide finite x/z/height on every sample."));
    }

    const float blend = std::clamp(strength, 0.0f, 1.0f);
    const float influence = half_width + skirt_width;
    float min_x = path_samples.front()[0];
    float max_x = min_x;
    float min_z = path_samples.front()[1];
    float max_z = min_z;
    for (const auto& sample : path_samples) {
        min_x = std::min(min_x, sample[0]);
        max_x = std::max(max_x, sample[0]);
        min_z = std::min(min_z, sample[1]);
        max_z = std::max(max_z, sample[1]);
    }
    min_x -= influence;
    max_x += influence;
    min_z -= influence;
    max_z += influence;

    const auto min_cell = terrain_cell_for_position(min_x, min_z, cell_size_);
    const auto max_cell = terrain_cell_for_position(max_x, max_z, cell_size_);
    const float step = cell_size_ / static_cast<float>(resolution_ - 1);
    std::set<CellCoord> touched;

    auto closest_on_path = [&](float sx, float sz, float& out_height, float& out_lateral) -> bool {
        float best_dist_sq = 1.0e30f;
        float best_height = path_samples.front()[2];
        bool found = false;
        if (path_samples.size() == 1) {
            const float dx = sx - path_samples[0][0];
            const float dz = sz - path_samples[0][1];
            out_lateral = std::hypot(dx, dz);
            out_height = path_samples[0][2];
            return true;
        }
        for (std::size_t i = 0; i + 1 < path_samples.size(); ++i) {
            const float x0 = path_samples[i][0];
            const float z0 = path_samples[i][1];
            const float h0 = path_samples[i][2];
            const float x1 = path_samples[i + 1][0];
            const float z1 = path_samples[i + 1][1];
            const float h1 = path_samples[i + 1][2];
            const float vx = x1 - x0;
            const float vz = z1 - z0;
            const float len_sq = vx * vx + vz * vz;
            float t = 0.0f;
            if (len_sq > 1.0e-8f) {
                t = std::clamp(((sx - x0) * vx + (sz - z0) * vz) / len_sq, 0.0f, 1.0f);
            }
            const float px = x0 + vx * t;
            const float pz = z0 + vz * t;
            const float dx = sx - px;
            const float dz = sz - pz;
            const float dist_sq = dx * dx + dz * dz;
            if (dist_sq < best_dist_sq) {
                best_dist_sq = dist_sq;
                best_height = h0 + (h1 - h0) * t;
                found = true;
            }
        }
        if (!found) return false;
        out_lateral = std::sqrt(best_dist_sq);
        out_height = best_height;
        return true;
    };

    for (std::int32_t cz = min_cell.z; cz <= max_cell.z; ++cz) {
        for (std::int32_t cx = min_cell.x; cx <= max_cell.x; ++cx) {
            const CellCoord cell{cx, cz};
            const float origin_x = static_cast<float>(cell.x) * cell_size_ - cell_size_ * 0.5f;
            const float origin_z = static_cast<float>(cell.z) * cell_size_ - cell_size_ * 0.5f;
            bool changed = false;
            for (std::uint32_t z = 0; z < resolution_; ++z) {
                for (std::uint32_t x = 0; x < resolution_; ++x) {
                    const float sample_x = origin_x + static_cast<float>(x) * step;
                    const float sample_z = origin_z + static_cast<float>(z) * step;
                    float target_height = 0.0f;
                    float lateral = 0.0f;
                    if (!closest_on_path(sample_x, sample_z, target_height, lateral)) continue;
                    if (lateral > influence) continue;
                    float weight = 1.0f;
                    if (lateral > half_width) {
                        if (skirt_width <= 1.0e-5f) continue;
                        weight = brush_falloff(lateral - half_width, skirt_width);
                    }
                    const float t = blend * weight;
                    if (t <= 1.0e-6f) continue;
                    auto& entry = ensure_cell(cell);
                    const std::size_t index = static_cast<std::size_t>(z) * resolution_ + x;
                    const float procedural = procedural_terrain_height(sample_x, sample_z);
                    const float desired_delta = std::clamp(target_height - procedural, -k_max_delta, k_max_delta);
                    const float next = std::clamp(
                        entry.deltas[index] + (desired_delta - entry.deltas[index]) * t, -k_max_delta, k_max_delta);
                    if (std::abs(next - entry.deltas[index]) > 0.00001f) {
                        entry.deltas[index] = next;
                        changed = true;
                    }
                }
            }
            if (changed) {
                touched.insert(cell);
                bool all_zero = true;
                for (float value : cells_[cell].deltas) {
                    if (std::abs(value) > 0.00001f) {
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

Result<std::set<CellCoord>> TerrainEditStore::apply_plateau_brush(float world_x, float world_z, float radius,
    float skirt_width, float strength, float target_height) {
    if (!std::isfinite(world_x) || !std::isfinite(world_z) || !std::isfinite(radius) || radius <= 0.0f ||
        !std::isfinite(skirt_width) || skirt_width < 0.0f || !std::isfinite(strength) || strength <= 0.0f ||
        !std::isfinite(target_height))
        return Result<std::set<CellCoord>>::failure(terrain_edit_error("TERRAIN-PLATEAU-INVALID",
            "Terrain plateau parameters must be finite and valid",
            "Use positive radius/strength, non-negative skirtWidth, and a finite targetHeight."));
    const float blend = std::clamp(strength, 0.0f, 1.0f);
    const float influence = radius + skirt_width;
    std::set<CellCoord> touched;
    const int cell_extent = static_cast<int>(std::ceil(influence / cell_size_)) + 1;
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
                    if (distance > influence) continue;
                    float weight = 1.0f;
                    if (distance > radius) {
                        if (skirt_width <= 1.0e-5f) continue;
                        weight = brush_falloff(distance - radius, skirt_width);
                    }
                    const float t = blend * weight;
                    if (t <= 1.0e-6f) continue;
                    auto& entry = ensure_cell(cell);
                    const std::size_t index = static_cast<std::size_t>(z) * resolution_ + x;
                    const float procedural = procedural_terrain_height(sample_x, sample_z);
                    const float desired_delta = std::clamp(target_height - procedural, -k_max_delta, k_max_delta);
                    const float next = std::clamp(
                        entry.deltas[index] + (desired_delta - entry.deltas[index]) * t, -k_max_delta, k_max_delta);
                    if (std::abs(next - entry.deltas[index]) > 0.00001f) {
                        entry.deltas[index] = next;
                        changed = true;
                    }
                }
            }
            if (changed) {
                touched.insert(cell);
                bool all_zero = true;
                for (float value : cells_[cell].deltas) {
                    if (std::abs(value) > 0.00001f) {
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

Result<std::set<CellCoord>> TerrainEditStore::apply_smooth_brush(float world_x, float world_z, float radius,
    float strength, float kernel_radius) {
    if (!std::isfinite(world_x) || !std::isfinite(world_z) || !std::isfinite(radius) || radius <= 0.0f ||
        !std::isfinite(strength) || strength <= 0.0f || !std::isfinite(kernel_radius) || kernel_radius <= 0.0f)
        return Result<std::set<CellCoord>>::failure(terrain_edit_error("TERRAIN-SMOOTH-INVALID",
            "Terrain smooth parameters must be finite and positive",
            "Use a positive radius/strength/kernelRadius."));
    const float blend = std::clamp(strength, 0.0f, 1.0f);
    const float sample_step = cell_size_ / static_cast<float>(resolution_ - 1);
    const int kernel_steps = std::max(1, static_cast<int>(std::ceil(kernel_radius / sample_step)));

    struct SampleKey {
        CellCoord cell{};
        std::uint32_t x = 0;
        std::uint32_t z = 0;
    };
    std::vector<SampleKey> targets;
    const int cell_extent = static_cast<int>(std::ceil(radius / cell_size_)) + 1;
    const auto center_cell = terrain_cell_for_position(world_x, world_z, cell_size_);
    for (int dz = -cell_extent; dz <= cell_extent; ++dz) {
        for (int dx = -cell_extent; dx <= cell_extent; ++dx) {
            const CellCoord cell{center_cell.x + dx, center_cell.z + dz};
            const float origin_x = static_cast<float>(cell.x) * cell_size_ - cell_size_ * 0.5f;
            const float origin_z = static_cast<float>(cell.z) * cell_size_ - cell_size_ * 0.5f;
            for (std::uint32_t z = 0; z < resolution_; ++z) {
                for (std::uint32_t x = 0; x < resolution_; ++x) {
                    const float sample_x = origin_x + static_cast<float>(x) * sample_step;
                    const float sample_z = origin_z + static_cast<float>(z) * sample_step;
                    if (std::hypot(sample_x - world_x, sample_z - world_z) > radius) continue;
                    targets.push_back(SampleKey{cell, x, z});
                }
            }
        }
    }

    std::map<CellCoord, std::vector<float>> next_deltas;
    std::set<CellCoord> touched;
    for (const auto& target : targets) {
        const float origin_x = static_cast<float>(target.cell.x) * cell_size_ - cell_size_ * 0.5f;
        const float origin_z = static_cast<float>(target.cell.z) * cell_size_ - cell_size_ * 0.5f;
        const float sample_x = origin_x + static_cast<float>(target.x) * sample_step;
        const float sample_z = origin_z + static_cast<float>(target.z) * sample_step;
        const float distance = std::hypot(sample_x - world_x, sample_z - world_z);
        float sum = 0.0f;
        float weight_sum = 0.0f;
        for (int kz = -kernel_steps; kz <= kernel_steps; ++kz) {
            for (int kx = -kernel_steps; kx <= kernel_steps; ++kx) {
                const float nx = sample_x + static_cast<float>(kx) * sample_step;
                const float nz = sample_z + static_cast<float>(kz) * sample_step;
                const float kd = std::hypot(static_cast<float>(kx) * sample_step, static_cast<float>(kz) * sample_step);
                if (kd > kernel_radius) continue;
                const float w = brush_falloff(kd, kernel_radius);
                sum += sample_delta_world(nx, nz) * w;
                weight_sum += w;
            }
        }
        if (weight_sum <= 1.0e-6f) continue;
        const float avg = sum / weight_sum;
        const std::size_t index = static_cast<std::size_t>(target.z) * resolution_ + target.x;
        if (!next_deltas.count(target.cell))
            next_deltas[target.cell] = cell_deltas_or_empty(target.cell);
        auto& deltas = next_deltas[target.cell];
        if (deltas.empty()) deltas.assign(static_cast<std::size_t>(resolution_) * resolution_, 0.0f);
        const float t = blend * brush_falloff(distance, radius);
        const float next = std::clamp(deltas[index] + (avg - deltas[index]) * t, -k_max_delta, k_max_delta);
        if (std::abs(next - deltas[index]) > 0.00001f) {
            deltas[index] = next;
            touched.insert(target.cell);
        }
    }
    for (auto& entry : next_deltas) {
        if (!touched.count(entry.first)) continue;
        bool all_zero = true;
        for (float value : entry.second) {
            if (std::abs(value) > 0.00001f) {
                all_zero = false;
                break;
            }
        }
        if (all_zero) remove_cell(entry.first);
        else set_cell_deltas(entry.first, std::move(entry.second));
    }
    return Result<std::set<CellCoord>>::success(std::move(touched));
}

Result<std::set<CellCoord>> TerrainEditStore::apply_terrace_brush(float world_x, float world_z, float radius,
    float strength, float step_size) {
    if (!std::isfinite(world_x) || !std::isfinite(world_z) || !std::isfinite(radius) || radius <= 0.0f ||
        !std::isfinite(strength) || strength <= 0.0f || !std::isfinite(step_size) || step_size <= 0.05f)
        return Result<std::set<CellCoord>>::failure(terrain_edit_error("TERRAIN-TERRACE-INVALID",
            "Terrain terrace parameters must be finite and positive",
            "Use a positive radius/strength and stepSize > 0.05."));
    const float blend = std::clamp(strength, 0.0f, 1.0f);
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
                    const float procedural = procedural_terrain_height(sample_x, sample_z);
                    const float current = procedural + entry.deltas[index];
                    const float terraced = std::round(current / step_size) * step_size;
                    const float desired_delta = std::clamp(terraced - procedural, -k_max_delta, k_max_delta);
                    const float t = blend * brush_falloff(distance, radius);
                    const float next =
                        std::clamp(entry.deltas[index] + (desired_delta - entry.deltas[index]) * t, -k_max_delta,
                            k_max_delta);
                    if (std::abs(next - entry.deltas[index]) > 0.00001f) {
                        entry.deltas[index] = next;
                        changed = true;
                    }
                }
            }
            if (changed) {
                touched.insert(cell);
                bool all_zero = true;
                for (float value : cells_[cell].deltas) {
                    if (std::abs(value) > 0.00001f) {
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

void TerrainEditStore::set_cell_deltas(CellCoord cell, std::vector<float> deltas) {
    if (deltas.empty()) {
        cells_.erase(cell);
        return;
    }
    TerrainCellEdits entry{cell, std::move(deltas)};
    cells_[cell] = std::move(entry);
}

void TerrainEditStore::remove_cell(CellCoord cell) { cells_.erase(cell); }

std::vector<float> TerrainEditStore::cell_deltas_or_empty(CellCoord cell) const {
    const auto* entry = find_cell(cell);
    if (!entry) return {};
    return entry->deltas;
}

std::string TerrainEditStore::to_json() const {
    nlohmann::ordered_json root{{"schemaVersion", 1}, {"resolution", resolution_}, {"cellSize", cell_size_},
        {"cells", nlohmann::ordered_json::array()}};
    for (const auto& entry : cells_) {
        root["cells"].push_back(
            {{"x", entry.second.coordinate.x}, {"z", entry.second.coordinate.z}, {"deltas", entry.second.deltas}});
    }
    return root.dump(2) + "\n";
}

Result<TerrainEditStore> TerrainEditStore::from_json(const std::string& text) {
    try {
        const auto root = nlohmann::json::parse(text);
        if (root.value("schemaVersion", 0) != 1 || !root["cells"].is_array())
            return Result<TerrainEditStore>::failure(terrain_edit_error("TERRAIN-EDIT-SCHEMA",
                "Terrain edit schema is invalid", "Use schemaVersion 1 with a cells array."));
        TerrainEditStore store;
        store.resolution_ = root.value("resolution", k_resolution);
        store.cell_size_ = root.value("cellSize", k_cell_size);
        if (store.resolution_ != k_resolution || !std::isfinite(store.cell_size_) || store.cell_size_ <= 0.0f)
            return Result<TerrainEditStore>::failure(terrain_edit_error("TERRAIN-EDIT-SCHEMA",
                "Terrain edit resolution or cell size is unsupported", "Use resolution 33 and cellSize 40."));
        for (const auto& cell_json : root["cells"]) {
            TerrainCellEdits cell{{cell_json.at("x").get<std::int32_t>(), cell_json.at("z").get<std::int32_t>()},
                cell_json.at("deltas").get<std::vector<float>>()};
            const auto valid = cell.validate(store.resolution_);
            if (!valid) return Result<TerrainEditStore>::failure(valid.error());
            store.cells_[cell.coordinate] = std::move(cell);
        }
        return Result<TerrainEditStore>::success(std::move(store));
    } catch (const std::exception& exception) {
        auto error = terrain_edit_error("TERRAIN-EDIT-PARSE", "Terrain edit JSON is malformed",
            "Correct the terrain edit file structure.");
        error.causes.push_back(exception.what());
        return Result<TerrainEditStore>::failure(std::move(error));
    }
}

Result<TerrainEditStore> TerrainEditStore::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return Result<TerrainEditStore>::failure(terrain_edit_error("TERRAIN-EDIT-READ",
            "Could not read terrain edits: " + path.generic_string(), "Check the terrain edit path."));
    std::ostringstream text;
    text << input.rdbuf();
    return from_json(text.str());
}

Result<void> TerrainEditStore::save_atomic(const std::filesystem::path& path) const {
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
        auto error = terrain_edit_error("TERRAIN-EDIT-WRITE", "Terrain edit atomic save failed",
            "Check file permissions and disk space.");
        error.causes.push_back(exception.what());
        return Result<void>::failure(std::move(error));
    }
}

Result<void> TerrainEditStore::validate_file(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return Result<void>::success();
    const auto loaded = load(path);
    if (!loaded) return Result<void>::failure(loaded.error());
    return Result<void>::success();
}

} // namespace engine
