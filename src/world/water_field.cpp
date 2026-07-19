#include "engine/world/water_field.h"

#include "engine/world/terrain.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

namespace engine {
namespace {
constexpr float k_min_depth = 0.12f;
constexpr float k_surface_r = 0.08f;
constexpr float k_surface_g = 0.22f;
constexpr float k_surface_b = 0.35f;
constexpr float k_skirt_r = 0.05f;
constexpr float k_skirt_g = 0.14f;
constexpr float k_skirt_b = 0.26f;

struct XZ {
    float x = 0.0f;
    float z = 0.0f;
};

[[nodiscard]] XZ lerp_xz(const XZ& a, const XZ& b, float t) {
    return {a.x + (b.x - a.x) * t, a.z + (b.z - a.z) * t};
}

} // namespace

StreamedWaterField::WaterCellMesh StreamedWaterField::build_cell_mesh(CellCoord cell, const WaterStore* store) const {
    WaterCellMesh mesh;
    if (!store) return mesh;
    const auto* mask = store->find_cell(cell);
    const float origin_x = static_cast<float>(cell.x) * k_cell_size - k_cell_size * 0.5f;
    const float origin_z = static_cast<float>(cell.z) * k_cell_size - k_cell_size * 0.5f;
    const float step = k_cell_size / static_cast<float>(k_resolution - 1);
    const float sea = store->sea_level();
    const float waterline = sea - k_min_depth;

    const auto sample_wet = [&](std::uint32_t x, std::uint32_t z) -> bool {
        const float wx = origin_x + static_cast<float>(x) * step;
        const float wz = origin_z + static_cast<float>(z) * step;
        if (sample_terrain_height(wx, wz) >= waterline) return false;
        if (mask) {
            const std::uint8_t fill = mask->fill[static_cast<std::size_t>(z) * k_resolution + x];
            if (fill >= WaterStore::k_fill_threshold) return true;
        }
        return store->in_sea_region(wx, wz);
    };

    const auto depth_at = [&](float wx, float wz) {
        return std::max(0.0f, sea - sample_terrain_height(wx, wz));
    };
    const auto push_colored = [&](float x, float y, float z, float r, float g, float b, float depth) {
        mesh.vertices.push_back({x, y, z, r, g, b, depth});
    };
    const auto push_surface = [&](const XZ& p) {
        push_colored(p.x, sea, p.z, k_surface_r, k_surface_g, k_surface_b, depth_at(p.x, p.z));
    };
    const auto push_skirt_edge = [&](const XZ& a, const XZ& b) {
        const float y0 = sample_terrain_height(a.x, a.z);
        const float y1 = sample_terrain_height(b.x, b.z);
        if (sea - y0 < k_min_depth && sea - y1 < k_min_depth) return;
        const float d0 = std::max(0.0f, sea - y0);
        const float d1 = std::max(0.0f, sea - y1);
        push_colored(a.x, sea, a.z, k_skirt_r, k_skirt_g, k_skirt_b, d0);
        push_colored(b.x, sea, b.z, k_skirt_r, k_skirt_g, k_skirt_b, d1);
        push_colored(b.x, y1, b.z, k_skirt_r, k_skirt_g, k_skirt_b, d1);
        push_colored(a.x, sea, a.z, k_skirt_r, k_skirt_g, k_skirt_b, d0);
        push_colored(b.x, y1, b.z, k_skirt_r, k_skirt_g, k_skirt_b, d1);
        push_colored(a.x, y0, a.z, k_skirt_r, k_skirt_g, k_skirt_b, d0);
    };

    const auto edge_crossing = [&](const XZ& a, float ya, bool wet_a, const XZ& b, float yb, bool wet_b) -> XZ {
        if (wet_a == wet_b) return a;
        const float da = waterline - ya;
        const float db = waterline - yb;
        float t = 0.5f;
        if ((da > 0.0f) != (db > 0.0f) && std::abs(da - db) > 1.0e-5f) t = da / (da - db);
        t = std::clamp(t, 0.0f, 1.0f);
        return lerp_xz(a, b, t);
    };

    const auto emit_polygon = [&](const std::vector<XZ>& poly) {
        if (poly.size() < 3) return;
        for (std::size_t i = 1; i + 1 < poly.size(); ++i) {
            push_surface(poly[0]);
            push_surface(poly[i]);
            push_surface(poly[i + 1]);
        }
        // Shore skirts on every polygon edge that is not fully interior (approx: all boundary edges).
        // For clipped cells, every edge is a shoreline or grid edge; skirt all of them for volume.
        for (std::size_t i = 0; i < poly.size(); ++i) {
            const XZ& a = poly[i];
            const XZ& b = poly[(i + 1) % poly.size()];
            const float mid_y = sample_terrain_height(0.5f * (a.x + b.x), 0.5f * (a.z + b.z));
            // Only skirt edges that sit near the waterline (shore), not deep interior spans.
            if (sea - mid_y < 2.5f) push_skirt_edge(a, b);
        }
    };

    for (std::uint32_t z = 0; z + 1 < k_resolution; ++z) {
        for (std::uint32_t x = 0; x + 1 < k_resolution; ++x) {
            const std::array<XZ, 4> corners = {{
                {origin_x + static_cast<float>(x) * step, origin_z + static_cast<float>(z) * step},
                {origin_x + static_cast<float>(x + 1) * step, origin_z + static_cast<float>(z) * step},
                {origin_x + static_cast<float>(x + 1) * step, origin_z + static_cast<float>(z + 1) * step},
                {origin_x + static_cast<float>(x) * step, origin_z + static_cast<float>(z + 1) * step},
            }};
            const std::array<bool, 4> wet = {
                sample_wet(x, z),
                sample_wet(x + 1, z),
                sample_wet(x + 1, z + 1),
                sample_wet(x, z + 1),
            };
            const std::array<float, 4> height = {
                sample_terrain_height(corners[0].x, corners[0].z),
                sample_terrain_height(corners[1].x, corners[1].z),
                sample_terrain_height(corners[2].x, corners[2].z),
                sample_terrain_height(corners[3].x, corners[3].z),
            };

            int mask_bits = 0;
            for (int i = 0; i < 4; ++i)
                if (wet[static_cast<std::size_t>(i)]) mask_bits |= (1 << i);
            if (mask_bits == 0) continue;

            if (mask_bits == 0xF) {
                // Fully wet: two triangles (keep skirts only on dry-neighbor borders).
                push_surface(corners[0]);
                push_surface(corners[1]);
                push_surface(corners[2]);
                push_surface(corners[0]);
                push_surface(corners[2]);
                push_surface(corners[3]);
                const auto neighbor_dry = [&](int edge) {
                    switch (edge) {
                    case 0: // south
                        return z == 0 || !sample_wet(x, z - 1) || !sample_wet(x + 1, z - 1);
                    case 1: // east
                        return x + 2 >= k_resolution || !sample_wet(x + 2, z) || !sample_wet(x + 2, z + 1);
                    case 2: // north
                        return z + 2 >= k_resolution || !sample_wet(x, z + 2) || !sample_wet(x + 1, z + 2);
                    case 3: // west
                        return x == 0 || !sample_wet(x - 1, z) || !sample_wet(x - 1, z + 1);
                    default:
                        return false;
                    }
                };
                if (neighbor_dry(0)) push_skirt_edge(corners[0], corners[1]);
                if (neighbor_dry(1)) push_skirt_edge(corners[1], corners[2]);
                if (neighbor_dry(2)) push_skirt_edge(corners[2], corners[3]);
                if (neighbor_dry(3)) push_skirt_edge(corners[3], corners[0]);
                continue;
            }

            // Build wet polygon: walk corners CCW, insert shoreline crossings on wet/dry edges.
            std::vector<XZ> poly;
            poly.reserve(6);
            for (int i = 0; i < 4; ++i) {
                const int j = (i + 1) & 3;
                const bool wi = wet[static_cast<std::size_t>(i)];
                const bool wj = wet[static_cast<std::size_t>(j)];
                if (wi) poly.push_back(corners[static_cast<std::size_t>(i)]);
                if (wi != wj) {
                    poly.push_back(edge_crossing(corners[static_cast<std::size_t>(i)], height[static_cast<std::size_t>(i)],
                        wi, corners[static_cast<std::size_t>(j)], height[static_cast<std::size_t>(j)], wj));
                }
            }
            emit_polygon(poly);
        }
    }
    return mesh;
}

Result<void> StreamedWaterField::update(const std::array<float, 3>& camera_position, std::uint32_t radius,
    const WaterStore* store) {
    const auto center = terrain_cell_for_position(camera_position[0], camera_position[2], k_cell_size);
    const auto wanted = terrain_cells_in_radius(center, radius);
    const std::set<CellCoord> wanted_set(wanted.begin(), wanted.end());
    if (center.x == focus_.x && center.z == focus_.z && wanted_set == desired_) return Result<void>::success();

    for (auto it = meshes_.begin(); it != meshes_.end();) {
        if (wanted_set.find(it->first) == wanted_set.end())
            it = meshes_.erase(it);
        else
            ++it;
    }

    for (const auto& cell : wanted_set) {
        if (meshes_.find(cell) != meshes_.end()) continue;
        meshes_.emplace(cell, build_cell_mesh(cell, store));
    }

    focus_ = center;
    desired_ = wanted_set;
    render_data_dirty_ = true;
    return Result<void>::success();
}

Result<void> StreamedWaterField::reload_cells(const std::set<CellCoord>& cells, const WaterStore* store) {
    for (const auto& cell : cells) {
        const auto found = meshes_.find(cell);
        if (found == meshes_.end()) continue;
        found->second = build_cell_mesh(cell, store);
    }
    if (!cells.empty()) render_data_dirty_ = true;
    return Result<void>::success();
}

std::set<CellCoord> StreamedWaterField::loaded_cell_coordinates() const {
    std::set<CellCoord> cells;
    for (const auto& entry : meshes_) cells.insert(entry.first);
    return cells;
}

std::vector<WaterRenderVertex> StreamedWaterField::build_render_vertices() const {
    std::vector<WaterRenderVertex> vertices;
    std::size_t total = 0;
    for (const auto& entry : meshes_) total += entry.second.vertices.size();
    vertices.reserve(total);
    for (const auto& entry : meshes_) {
        vertices.insert(vertices.end(), entry.second.vertices.begin(), entry.second.vertices.end());
    }
    return vertices;
}

} // namespace engine
