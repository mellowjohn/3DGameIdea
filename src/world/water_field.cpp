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

    // One height sample per grid point — the old path re-sampled the same XZ many times per quad
    // (wet + corners + skirts) and spiked ~40 ms per streamed cell.
    std::vector<float> heights(static_cast<std::size_t>(k_resolution) * k_resolution);
    std::vector<std::uint8_t> wet(static_cast<std::size_t>(k_resolution) * k_resolution, 0);
    std::size_t wet_count = 0;
    for (std::uint32_t z = 0; z < k_resolution; ++z) {
        for (std::uint32_t x = 0; x < k_resolution; ++x) {
            const std::size_t index = static_cast<std::size_t>(z) * k_resolution + x;
            const float wx = origin_x + static_cast<float>(x) * step;
            const float wz = origin_z + static_cast<float>(z) * step;
            const float h = sample_terrain_height(wx, wz);
            heights[index] = h;
            if (h >= waterline) continue;
            if (mask) {
                const std::uint8_t fill = mask->fill[index];
                if (fill >= WaterStore::k_fill_threshold) {
                    wet[index] = 1;
                    ++wet_count;
                    continue;
                }
            }
            if (store->in_sea_region(wx, wz)) {
                wet[index] = 1;
                ++wet_count;
            }
        }
    }
    if (wet_count == 0) return mesh;

    // Full wet cell ~6k surface verts + skirts; reserve to avoid Debug realloc storms.
    mesh.vertices.reserve(wet_count * 6 + 256);

    const auto height_at = [&](std::uint32_t x, std::uint32_t z) -> float {
        return heights[static_cast<std::size_t>(z) * k_resolution + x];
    };
    const auto wet_at = [&](std::uint32_t x, std::uint32_t z) -> bool {
        return wet[static_cast<std::size_t>(z) * k_resolution + x] != 0;
    };
    const auto depth_from_h = [&](float h) { return std::max(0.0f, sea - h); };
    const auto push_colored = [&](float x, float y, float z, float r, float g, float b, float depth) {
        mesh.vertices.push_back({x, y, z, r, g, b, depth});
    };
    const auto push_surface_h = [&](const XZ& p, float h) {
        push_colored(p.x, sea, p.z, k_surface_r, k_surface_g, k_surface_b, depth_from_h(h));
    };
    const auto push_skirt_edge_h = [&](const XZ& a, float y0, const XZ& b, float y1) {
        if (sea - y0 < k_min_depth && sea - y1 < k_min_depth) return;
        const float d0 = depth_from_h(y0);
        const float d1 = depth_from_h(y1);
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

    struct PolyPoint {
        XZ xz{};
        float h = 0.0f;
    };

    const auto emit_polygon = [&](const std::vector<PolyPoint>& poly) {
        if (poly.size() < 3) return;
        for (std::size_t i = 1; i + 1 < poly.size(); ++i) {
            push_surface_h(poly[0].xz, poly[0].h);
            push_surface_h(poly[i].xz, poly[i].h);
            push_surface_h(poly[i + 1].xz, poly[i + 1].h);
        }
        for (std::size_t i = 0; i < poly.size(); ++i) {
            const PolyPoint& a = poly[i];
            const PolyPoint& b = poly[(i + 1) % poly.size()];
            const float mid_y = 0.5f * (a.h + b.h);
            if (sea - mid_y < 2.5f) push_skirt_edge_h(a.xz, a.h, b.xz, b.h);
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
            const std::array<bool, 4> wet_corners = {
                wet_at(x, z),
                wet_at(x + 1, z),
                wet_at(x + 1, z + 1),
                wet_at(x, z + 1),
            };
            const std::array<float, 4> height = {
                height_at(x, z),
                height_at(x + 1, z),
                height_at(x + 1, z + 1),
                height_at(x, z + 1),
            };

            int mask_bits = 0;
            for (int i = 0; i < 4; ++i)
                if (wet_corners[static_cast<std::size_t>(i)]) mask_bits |= (1 << i);
            if (mask_bits == 0) continue;

            if (mask_bits == 0xF) {
                push_surface_h(corners[0], height[0]);
                push_surface_h(corners[1], height[1]);
                push_surface_h(corners[2], height[2]);
                push_surface_h(corners[0], height[0]);
                push_surface_h(corners[2], height[2]);
                push_surface_h(corners[3], height[3]);
                const auto neighbor_dry = [&](int edge) {
                    switch (edge) {
                    case 0:
                        return z == 0 || !wet_at(x, z - 1) || !wet_at(x + 1, z - 1);
                    case 1:
                        return x + 2 >= k_resolution || !wet_at(x + 2, z) || !wet_at(x + 2, z + 1);
                    case 2:
                        return z + 2 >= k_resolution || !wet_at(x, z + 2) || !wet_at(x + 1, z + 2);
                    case 3:
                        return x == 0 || !wet_at(x - 1, z) || !wet_at(x - 1, z + 1);
                    default:
                        return false;
                    }
                };
                if (neighbor_dry(0)) push_skirt_edge_h(corners[0], height[0], corners[1], height[1]);
                if (neighbor_dry(1)) push_skirt_edge_h(corners[1], height[1], corners[2], height[2]);
                if (neighbor_dry(2)) push_skirt_edge_h(corners[2], height[2], corners[3], height[3]);
                if (neighbor_dry(3)) push_skirt_edge_h(corners[3], height[3], corners[0], height[0]);
                continue;
            }

            std::vector<PolyPoint> poly;
            poly.reserve(6);
            for (int i = 0; i < 4; ++i) {
                const int j = (i + 1) & 3;
                const bool wi = wet_corners[static_cast<std::size_t>(i)];
                const bool wj = wet_corners[static_cast<std::size_t>(j)];
                if (wi)
                    poly.push_back(PolyPoint{corners[static_cast<std::size_t>(i)], height[static_cast<std::size_t>(i)]});
                if (wi != wj) {
                    const float ya = height[static_cast<std::size_t>(i)];
                    const float yb = height[static_cast<std::size_t>(j)];
                    const float da = waterline - ya;
                    const float db = waterline - yb;
                    float t = 0.5f;
                    if ((da > 0.0f) != (db > 0.0f) && std::abs(da - db) > 1.0e-5f) t = da / (da - db);
                    t = std::clamp(t, 0.0f, 1.0f);
                    poly.push_back(PolyPoint{edge_crossing(corners[static_cast<std::size_t>(i)], ya, wi,
                                                    corners[static_cast<std::size_t>(j)], yb, wj),
                        ya + (yb - ya) * t});
                }
            }
            emit_polygon(poly);
        }
    }
    return mesh;
}

Result<void> StreamedWaterField::update(const std::array<float, 3>& camera_position, std::uint32_t radius,
    const WaterStore* store, std::size_t max_new_cells) {
    const auto center = terrain_cell_for_position(camera_position[0], camera_position[2], k_cell_size);
    const auto wanted = terrain_cells_in_radius(center, radius);
    const std::set<CellCoord> wanted_set(wanted.begin(), wanted.end());

    if (!(center.x == focus_.x && center.z == focus_.z && wanted_set == desired_)) {
        for (auto it = meshes_.begin(); it != meshes_.end();) {
            if (wanted_set.find(it->first) == wanted_set.end()) {
                render_removed_cells_.insert(it->first);
                render_dirty_cells_.erase(it->first);
                it = meshes_.erase(it);
            } else {
                ++it;
            }
        }
        focus_ = center;
        desired_ = wanted_set;
    }

    std::size_t built = 0;
    for (const auto& cell : desired_) {
        if (meshes_.find(cell) != meshes_.end()) continue;
        if (built >= max_new_cells) break;
        meshes_.emplace(cell, build_cell_mesh(cell, store));
        render_removed_cells_.erase(cell);
        render_dirty_cells_.insert(cell);
        ++built;
    }

    return Result<void>::success();
}

Result<void> StreamedWaterField::reload_cells(const std::set<CellCoord>& cells, const WaterStore* store) {
    for (const auto& cell : cells) {
        const auto found = meshes_.find(cell);
        if (found == meshes_.end()) continue;
        found->second = build_cell_mesh(cell, store);
        render_dirty_cells_.insert(cell);
    }
    return Result<void>::success();
}

std::set<CellCoord> StreamedWaterField::loaded_cell_coordinates() const {
    std::set<CellCoord> cells;
    for (const auto& entry : meshes_) cells.insert(entry.first);
    return cells;
}

bool StreamedWaterField::stream_pending() const noexcept {
    for (const auto& cell : desired_) {
        if (meshes_.find(cell) == meshes_.end()) return true;
    }
    return false;
}

std::set<CellCoord> StreamedWaterField::take_render_dirty_cells(std::size_t max_cells) {
    std::set<CellCoord> taken;
    for (auto it = render_dirty_cells_.begin(); it != render_dirty_cells_.end() && taken.size() < max_cells;) {
        taken.insert(*it);
        it = render_dirty_cells_.erase(it);
    }
    return taken;
}

std::set<CellCoord> StreamedWaterField::take_render_removed_cells() {
    std::set<CellCoord> taken;
    taken.swap(render_removed_cells_);
    return taken;
}

std::vector<WaterRenderVertex> StreamedWaterField::build_cell_render_vertices(CellCoord cell) const {
    const auto found = meshes_.find(cell);
    if (found == meshes_.end()) return {};
    return found->second.vertices;
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
