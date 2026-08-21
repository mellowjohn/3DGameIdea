#include "engine/world/terrain_field.h"

#include "engine/world/terrain_edits.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <memory>

namespace engine {
namespace {

int chebyshev_distance(CellCoord a, CellCoord b) {
    return std::max(std::abs(a.x - b.x), std::abs(a.z - b.z));
}

float forward_dot_for_cell(CellCoord cell, CellCoord center, float forward_x, float forward_z) {
    const int dx = cell.x - center.x;
    const int dz = cell.z - center.z;
    if (dx == 0 && dz == 0) return 1.0f;
    const float len = std::sqrt(static_cast<float>(dx * dx + dz * dz));
    if (!(len > 0.0f)) return 1.0f;
    const float flen = std::sqrt(forward_x * forward_x + forward_z * forward_z);
    if (!(flen > 1.0e-8f)) return 0.0f;
    return (static_cast<float>(dx) / len) * (forward_x / flen) + (static_cast<float>(dz) / len) * (forward_z / flen);
}

} // namespace

Result<void> StreamedTerrainField::update(CollisionWorld& world, const std::array<float, 3>& camera_position,
    const PhysicalMaterialProperties& physics, std::uint32_t radius, const TerrainEditStore* edits,
    const TerrainPaintStore* paint, const TerrainPaintMaterialLookup& lookup_material) {
    TerrainStreamParams params;
    params.radius = radius;
    return update(world, std::vector<std::array<float, 3>>{camera_position}, physics, params, edits, paint,
        lookup_material);
}

Result<void> StreamedTerrainField::update(CollisionWorld& world,
    const std::vector<std::array<float, 3>>& focus_positions, const PhysicalMaterialProperties& physics,
    std::uint32_t radius, const TerrainEditStore* edits, const TerrainPaintStore* paint,
    const TerrainPaintMaterialLookup& lookup_material) {
    TerrainStreamParams params;
    params.radius = radius;
    return update(world, focus_positions, physics, params, edits, paint, lookup_material);
}

Result<void> StreamedTerrainField::update(CollisionWorld& world,
    const std::vector<std::array<float, 3>>& focus_positions, const PhysicalMaterialProperties& physics,
    const TerrainStreamParams& params, const TerrainEditStore* edits, const TerrainPaintStore* paint,
    const TerrainPaintMaterialLookup& lookup_material) {
    if (focus_positions.empty()) return Result<void>::success();

    std::vector<CellCoord> focus_cells;
    focus_cells.reserve(focus_positions.size());
    for (const auto& focus : focus_positions) {
        focus_cells.push_back(terrain_cell_for_position(focus[0], focus[2], k_cell_size));
    }
    const CellCoord primary = focus_cells.front();

    std::set<CellCoord> wanted_set;
    std::set<CellCoord> support_set;
    const std::uint32_t support_radius = std::min(params.support_radius, params.radius);
    for (const auto& center : focus_cells) {
        const auto support = terrain_cells_in_radius(center, support_radius);
        support_set.insert(support.begin(), support.end());
        if (params.view_bias) {
            const auto wanted =
                terrain_cells_in_view_bias(center, params.radius, support_radius, params.forward_x, params.forward_z);
            wanted_set.insert(wanted.begin(), wanted.end());
        } else {
            const auto wanted = terrain_cells_in_radius(center, params.radius);
            wanted_set.insert(wanted.begin(), wanted.end());
        }
    }

    const auto committed = commit_ready_generations(world, physics, wanted_set, params.max_ready_commits);
    if (!committed) return committed;

    const bool neighborhood_unchanged = primary.x == focus_.x && primary.z == focus_.z && wanted_set == desired_;
    if (neighborhood_unchanged && pending_cells_.empty() && pending_generations_.empty()) return Result<void>::success();

    // Bootstrap / teleport: the focus support disc cannot remain asynchronous because the player has no
    // guaranteed floor until it commits. Waiting here preserves the existing safety contract; ordinary
    // walk-fringe and outer-ring generation stay off-thread.
    bool focus_unsupported = cells_.empty();
    if (!focus_unsupported) {
        for (const auto& center : focus_cells) {
            if (cells_.find(center) == cells_.end()) {
                focus_unsupported = true;
                break;
            }
        }
    }
    if (focus_unsupported) {
        for (const auto& cell : support_set) {
            if (!generation_pending(cell)) continue;
            const auto committed_now = commit_generation_now(world, cell, physics);
            if (!committed_now) return committed_now;
        }
    }

    std::vector<CellCoord> missing_support;
    std::vector<CellCoord> missing_outer;
    for (const auto& cell : wanted_set) {
        if (cells_.find(cell) != cells_.end() || generation_pending(cell)) continue;
        if (support_set.find(cell) != support_set.end()) missing_support.push_back(cell);
        else missing_outer.push_back(cell);
    }

    auto priority_less = [&](CellCoord a, CellCoord b) {
        int a_cheb = std::numeric_limits<int>::max();
        int b_cheb = std::numeric_limits<int>::max();
        float a_dot = -2.0f;
        float b_dot = -2.0f;
        for (const auto& center : focus_cells) {
            a_cheb = std::min(a_cheb, chebyshev_distance(a, center));
            b_cheb = std::min(b_cheb, chebyshev_distance(b, center));
            a_dot = std::max(a_dot, forward_dot_for_cell(a, center, params.forward_x, params.forward_z));
            b_dot = std::max(b_dot, forward_dot_for_cell(b, center, params.forward_x, params.forward_z));
        }
        if (a_cheb != b_cheb) return a_cheb < b_cheb;
        if (a_dot != b_dot) return a_dot > b_dot;
        if (a.x != b.x) return a.x < b.x;
        return a.z < b.z;
    };
    std::sort(missing_support.begin(), missing_support.end(), priority_less);
    std::sort(missing_outer.begin(), missing_outer.end(), priority_less);

    const std::size_t support_budget =
        focus_unsupported ? std::numeric_limits<std::size_t>::max() : params.max_new_support_cells;
    std::size_t loaded_support = 0;
    for (const auto& cell : missing_support) {
        if (loaded_support >= support_budget) break;
        const auto result = params.async_generation && !focus_unsupported
            ? queue_cell_generation(cell, edits, paint, lookup_material)
            : load_cell(world, cell, physics, edits, paint, lookup_material);
        if (!result) return result;
        ++loaded_support;
    }

    bool support_complete = true;
    for (const auto& cell : support_set) {
        if (cells_.find(cell) == cells_.end()) {
            support_complete = false;
            break;
        }
    }

    // Hold prior cells until the new support disc is fully resident — never open a floor hole under walk.
    if (support_complete) {
        // Remove only this stream's heightfield bodies. Never call unload_cell here — that cell key is also
        // used by placement partition bodies (player Rigidbody), and nuking the bucket drops idle avatars.
        for (auto it = cells_.begin(); it != cells_.end();) {
            if (wanted_set.find(it->first) == wanted_set.end()) {
                (void)world.remove(it->second.body);
                render_removed_cells_.insert(it->first);
                render_dirty_cells_.erase(it->first);
                it = cells_.erase(it);
            } else {
                ++it;
            }
        }
    }

    // 0 blocks outer loads (look-gate). Prefer SIZE_MAX for uncapped, not 0.
    // Defer outer catch-up while support fringe is still streaming so walk frames stay to one gen.
    const std::size_t budget = support_complete ? params.max_new_cells : 0;
    std::size_t loaded_outer = 0;
    for (const auto& cell : missing_outer) {
        if (loaded_outer >= budget) break;
        if (cells_.find(cell) != cells_.end() || generation_pending(cell)) continue;
        const auto result = params.async_generation
            ? queue_cell_generation(cell, edits, paint, lookup_material)
            : load_cell(world, cell, physics, edits, paint, lookup_material);
        if (!result) return result;
        ++loaded_outer;
    }

    pending_cells_.clear();
    for (const auto& cell : wanted_set) {
        if (cells_.find(cell) == cells_.end()) pending_cells_.insert(cell);
    }
    // Also treat incomplete support as pending so callers keep pumping updates.
    if (!support_complete) {
        for (const auto& cell : support_set) {
            if (cells_.find(cell) == cells_.end()) pending_cells_.insert(cell);
        }
    }

    focus_ = primary;
    desired_ = wanted_set;
    return Result<void>::success();
}

Result<void> StreamedTerrainField::load_cell(CollisionWorld& world, CellCoord cell,
    const PhysicalMaterialProperties& physics, const TerrainEditStore* edits, const TerrainPaintStore* paint,
    const TerrainPaintMaterialLookup& lookup_material) {
    auto terrain = generate_stylized_terrain(cell, k_resolution, k_cell_size, edits, paint, lookup_material);
    if (!terrain) return Result<void>::failure(terrain.error());
    return commit_cell(world, std::move(terrain.value()), physics);
}

Result<void> StreamedTerrainField::commit_cell(CollisionWorld& world, TerrainMesh terrain,
    const PhysicalMaterialProperties& physics) {
    const CellCoord cell = terrain.coordinate;
    const auto body = world.add_heightfield(terrain, physics, cell);
    if (!body) return Result<void>::failure(body.error());
    LoadedCell loaded;
    loaded.mesh = std::move(terrain);
    loaded.body = body.value();
    cells_.emplace(cell, std::move(loaded));
    render_dirty_cells_.insert(cell);
    render_removed_cells_.erase(cell);
    return Result<void>::success();
}

bool StreamedTerrainField::generation_pending(CellCoord cell) const {
    return pending_generations_.find(cell) != pending_generations_.end();
}

Result<void> StreamedTerrainField::queue_cell_generation(CellCoord cell, const TerrainEditStore* edits,
    const TerrainPaintStore* paint, const TerrainPaintMaterialLookup& lookup_material) {
    if (generation_pending(cell)) return Result<void>::success();

    // Editor terrain and paint stores are mutable on the main thread. Snapshot the data and only hand those
    // copies to the worker so brush/MCP updates cannot race a cell generation task.
    const auto edits_snapshot = edits ? std::make_shared<TerrainEditStore>(*edits) : nullptr;
    const auto paint_snapshot = paint ? std::make_shared<TerrainPaintStore>(*paint) : nullptr;
    auto materials_snapshot = std::make_shared<std::map<std::string, MaterialAsset>>();
    if (paint && lookup_material) {
        for (const auto& path : paint->materials()) {
            if (const MaterialAsset* material = lookup_material(path)) materials_snapshot->emplace(path, *material);
        }
    }
    pending_generations_.emplace(cell, PendingGeneration{std::async(std::launch::async,
        [cell, edits_snapshot, paint_snapshot, materials_snapshot]() {
            const TerrainPaintMaterialLookup lookup = [materials_snapshot](const std::string& path) {
                const auto found = materials_snapshot->find(path);
                return found == materials_snapshot->end() ? nullptr : &found->second;
            };
            AsyncTerrainMesh output;
            auto terrain = generate_stylized_terrain(cell, k_resolution, k_cell_size, edits_snapshot.get(),
                paint_snapshot.get(), lookup);
            if (terrain) output.mesh = std::move(terrain.value());
            else output.error = terrain.error();
            return output;
        })});
    return Result<void>::success();
}

Result<void> StreamedTerrainField::commit_ready_generations(CollisionWorld& world,
    const PhysicalMaterialProperties& physics, const std::set<CellCoord>& wanted, std::size_t max_commits) {
    std::size_t committed_count = 0;
    for (auto it = pending_generations_.begin(); it != pending_generations_.end();) {
        if (committed_count >= max_commits) break;
        if (it->second.future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            ++it;
            continue;
        }
        auto generated = it->second.future.get();
        const CellCoord requested_cell = it->first;
        const bool still_wanted = wanted.find(requested_cell) != wanted.end();
        it = pending_generations_.erase(it);
        if (!still_wanted) continue;
        if (generated.error) return Result<void>::failure(*generated.error);
        if (!generated.mesh) {
            return Result<void>::failure(EngineError{"TERRAIN-STREAM-RESULT-MISSING", Severity::Error,
                ErrorCategory::Validation, "terrain", "Terrain generation returned no result"});
        }
        if (generated.mesh->coordinate != requested_cell) {
            return Result<void>::failure(EngineError{"TERRAIN-STREAM-CELL-MISMATCH", Severity::Error,
                ErrorCategory::Validation, "terrain", "Generated terrain cell does not match its stream request"});
        }
        const auto committed = commit_cell(world, std::move(*generated.mesh), physics);
        if (!committed) return committed;
        ++committed_count;
    }
    return Result<void>::success();
}

Result<void> StreamedTerrainField::commit_generation_now(CollisionWorld& world, CellCoord cell,
    const PhysicalMaterialProperties& physics) {
    const auto found = pending_generations_.find(cell);
    if (found == pending_generations_.end()) return Result<void>::success();
    auto generated = found->second.future.get();
    pending_generations_.erase(found);
    if (generated.error) return Result<void>::failure(*generated.error);
    if (!generated.mesh) {
        return Result<void>::failure(EngineError{"TERRAIN-STREAM-RESULT-MISSING", Severity::Error,
            ErrorCategory::Validation, "terrain", "Terrain generation returned no result"});
    }
    if (generated.mesh->coordinate != cell) {
        return Result<void>::failure(EngineError{"TERRAIN-STREAM-CELL-MISMATCH", Severity::Error,
            ErrorCategory::Validation, "terrain", "Generated terrain cell does not match its stream request"});
    }
    return commit_cell(world, std::move(*generated.mesh), physics);
}

std::set<CellCoord> StreamedTerrainField::loaded_cell_coordinates() const {
    std::set<CellCoord> cells;
    for (const auto& entry : cells_) cells.insert(entry.first);
    return cells;
}

std::set<CellCoord> StreamedTerrainField::take_render_dirty_cells(std::size_t max_cells) {
    std::set<CellCoord> out;
    if (max_cells == 0) return out;
    if (max_cells >= render_dirty_cells_.size()) {
        out = std::move(render_dirty_cells_);
        render_dirty_cells_.clear();
        return out;
    }
    for (auto it = render_dirty_cells_.begin(); it != render_dirty_cells_.end() && out.size() < max_cells;) {
        out.insert(*it);
        it = render_dirty_cells_.erase(it);
    }
    return out;
}

std::set<CellCoord> StreamedTerrainField::take_render_removed_cells() {
    std::set<CellCoord> out = std::move(render_removed_cells_);
    render_removed_cells_.clear();
    return out;
}

Result<void> StreamedTerrainField::reload_cells(CollisionWorld& world, const std::set<CellCoord>& cells,
    const PhysicalMaterialProperties& physics, const TerrainEditStore* edits, const TerrainPaintStore* paint,
    const TerrainPaintMaterialLookup& lookup_material) {
    for (const auto& cell : cells) {
        const auto found = cells_.find(cell);
        if (found == cells_.end()) continue;
        (void)world.remove(found->second.body);
        auto terrain = generate_stylized_terrain(cell, k_resolution, k_cell_size, edits, paint, lookup_material);
        if (!terrain) return Result<void>::failure(terrain.error());
        const auto body = world.add_heightfield(terrain.value(), physics, cell);
        if (!body) return Result<void>::failure(body.error());
        found->second.mesh = std::move(terrain.value());
        found->second.body = body.value();
        render_dirty_cells_.insert(cell);
    }
    return Result<void>::success();
}

Result<void> StreamedTerrainField::reload_cell_meshes(const std::set<CellCoord>& cells, const TerrainEditStore* edits,
    const TerrainPaintStore* paint, const TerrainPaintMaterialLookup& lookup_material) {
    for (const auto& cell : cells) {
        const auto found = cells_.find(cell);
        if (found == cells_.end()) continue;
        auto terrain = generate_stylized_terrain(cell, k_resolution, k_cell_size, edits, paint, lookup_material);
        if (!terrain) return Result<void>::failure(terrain.error());
        found->second.mesh = std::move(terrain.value());
        render_dirty_cells_.insert(cell);
    }
    return Result<void>::success();
}

Result<void> StreamedTerrainField::reload_collision_cells(
    CollisionWorld& world, const std::set<CellCoord>& cells,
    const PhysicalMaterialProperties& physics) {
    for (const auto& cell : cells) {
        const auto found = cells_.find(cell);
        if (found == cells_.end()) continue;
        (void)world.remove(found->second.body);
        const auto body = world.add_heightfield(found->second.mesh, physics, cell);
        if (!body) return Result<void>::failure(body.error());
        found->second.body = body.value();
    }
    return Result<void>::success();
}

Result<void> StreamedTerrainField::update_collision_materials(
    CollisionWorld& world, const std::set<CellCoord>& cells,
    const PhysicalMaterialProperties& physics) {
    for (const auto& cell : cells) {
        const auto found = cells_.find(cell);
        if (found == cells_.end()) continue;
        const auto updated = world.set_material(found->second.body, physics);
        if (!updated) return updated;
    }
    return Result<void>::success();
}

Result<void> StreamedTerrainField::queue_reload_cells(
    const std::set<CellCoord>& cells, const TerrainEditStore* edits,
    const TerrainPaintStore* paint, const TerrainPaintMaterialLookup& lookup_material) {
    for (const auto& cell : cells) {
        if (cells_.find(cell) == cells_.end() || pending_reloads_.find(cell) != pending_reloads_.end()) continue;
        const auto edits_snapshot = edits ? std::make_shared<TerrainEditStore>(*edits) : nullptr;
        const auto paint_snapshot = paint ? std::make_shared<TerrainPaintStore>(*paint) : nullptr;
        auto materials_snapshot = std::make_shared<std::map<std::string, MaterialAsset>>();
        if (paint && lookup_material) {
            for (const auto& path : paint->materials()) {
                if (const MaterialAsset* material = lookup_material(path)) materials_snapshot->emplace(path, *material);
            }
        }
        pending_reloads_.emplace(cell, PendingGeneration{std::async(std::launch::async,
            [cell, edits_snapshot, paint_snapshot, materials_snapshot]() {
                const auto started = std::chrono::steady_clock::now();
                const TerrainPaintMaterialLookup lookup = [materials_snapshot](const std::string& path) {
                    const auto found = materials_snapshot->find(path);
                    return found == materials_snapshot->end() ? nullptr : &found->second;
                };
                AsyncTerrainMesh output;
                auto terrain = generate_stylized_terrain(cell, k_resolution, k_cell_size, edits_snapshot.get(),
                    paint_snapshot.get(), lookup);
                if (terrain) output.mesh = std::move(terrain.value());
                else output.error = terrain.error();
                output.build_ms = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - started).count();
                return output;
            }), std::chrono::steady_clock::now()});
    }
    return Result<void>::success();
}

Result<std::size_t> StreamedTerrainField::commit_ready_reloads(
    CollisionWorld& world, const PhysicalMaterialProperties& physics, std::size_t max_cells) {
    std::size_t committed = 0;
    for (auto it = pending_reloads_.begin(); it != pending_reloads_.end() && committed < max_cells;) {
        if (it->second.future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) { ++it; continue; }
        auto generated = it->second.future.get();
        last_reload_queue_wait_ms_ = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - it->second.queued_at).count() - generated.build_ms;
        const CellCoord cell = it->first;
        it = pending_reloads_.erase(it);
        if (generated.error) return Result<std::size_t>::failure(*generated.error);
        if (!generated.mesh) return Result<std::size_t>::failure(EngineError{"TERRAIN-RELOAD-RESULT-MISSING", Severity::Error,
            ErrorCategory::Validation, "terrain", "Sculpt terrain regeneration returned no mesh"});
        const auto found = cells_.find(cell);
        if (found == cells_.end()) continue;
        const auto collision_started = std::chrono::steady_clock::now();
        (void)world.remove(found->second.body);
        const auto body = world.add_heightfield(*generated.mesh, physics, cell);
        if (!body) return Result<std::size_t>::failure(body.error());
        found->second.mesh = std::move(*generated.mesh);
        found->second.body = body.value();
        render_dirty_cells_.insert(cell);
        last_reload_generation_ms_ = generated.build_ms;
        last_reload_collision_ms_ = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - collision_started).count();
        ++committed;
    }
    return Result<std::size_t>::success(committed);
}

std::vector<TerrainRenderVertex> StreamedTerrainField::build_cell_render_vertices(CellCoord cell,
    const std::array<float, 4>& base_color) const {
    std::vector<TerrainRenderVertex> vertices;
    const auto found = cells_.find(cell);
    if (found == cells_.end()) return vertices;
    vertices.reserve(found->second.mesh.triangles.size());
    for (const auto& triangle : found->second.mesh.triangles) {
        if (triangle.painted) {
            vertices.push_back({triangle.x, triangle.y, triangle.z, triangle.r, triangle.g, triangle.b});
        } else {
            vertices.push_back({triangle.x, triangle.y, triangle.z, triangle.r * base_color[0],
                triangle.g * base_color[1], triangle.b * base_color[2]});
        }
    }
    return vertices;
}

std::vector<TerrainRenderVertex> StreamedTerrainField::build_render_vertices(
    const std::array<float, 4>& base_color) const {
    std::vector<TerrainRenderVertex> vertices;
    std::size_t total = 0;
    for (const auto& entry : cells_) total += entry.second.mesh.triangles.size();
    vertices.reserve(total);
    for (const auto& entry : cells_) {
        auto cell_verts = build_cell_render_vertices(entry.first, base_color);
        vertices.insert(vertices.end(), cell_verts.begin(), cell_verts.end());
    }
    return vertices;
}

bool apply_stream_view_bias_look_gate(StreamViewBiasGate& gate, float yaw_radians, float look_forward_x,
    float look_forward_z, TerrainStreamParams& params, float yaw_hot_radians, int settle_frames) {
    constexpr float k_pi = 3.14159265358979323846f;
    auto yaw_delta = [](float from, float to) {
        float d = to - from;
        while (d > k_pi) d -= 2.0f * k_pi;
        while (d < -k_pi) d += 2.0f * k_pi;
        return d;
    };

    if (!gate.initialized) {
        gate.last_yaw = yaw_radians;
        gate.stable_forward_x = look_forward_x;
        gate.stable_forward_z = look_forward_z;
        gate.hot_frames_remaining = 0;
        gate.initialized = true;
    }

    const float dyaw = std::abs(yaw_delta(gate.last_yaw, yaw_radians));
    gate.last_yaw = yaw_radians;
    if (dyaw > yaw_hot_radians) {
        gate.hot_frames_remaining = settle_frames;
    } else if (gate.hot_frames_remaining > 0) {
        --gate.hot_frames_remaining;
    }

    const bool look_hot = gate.hot_frames_remaining > 0;
    if (look_hot) {
        params.forward_x = gate.stable_forward_x;
        params.forward_z = gate.stable_forward_z;
        params.max_new_cells = 0;
        // Walk+look: do not start support fringe work mid-turn (prior support stays resident).
        params.max_new_support_cells = 0;
        // Queued workers may finish during a turn; committing them mid-look still spikes
        // (heightfield create + render dirty + foliage). Hold commits until yaw settles.
        params.max_ready_commits = 0;
    } else {
        gate.stable_forward_x = look_forward_x;
        gate.stable_forward_z = look_forward_z;
        params.forward_x = look_forward_x;
        params.forward_z = look_forward_z;
    }
    return look_hot;
}

} // namespace engine
