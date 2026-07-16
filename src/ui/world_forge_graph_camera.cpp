#include "engine/ui/world_forge_graph_camera.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace engine {

WorldForgeGraphBounds compute_graph_bounds(const std::unordered_map<std::string, std::array<float, 2>>& positions,
    const std::vector<std::string>* only_keys) {
    WorldForgeGraphBounds bounds;
    auto consider = [&](const std::array<float, 2>& p) {
        if (!bounds.valid) {
            bounds.min_x = bounds.max_x = p[0];
            bounds.min_y = bounds.max_y = p[1];
            bounds.valid = true;
        } else {
            bounds.min_x = (std::min)(bounds.min_x, p[0]);
            bounds.max_x = (std::max)(bounds.max_x, p[0]);
            bounds.min_y = (std::min)(bounds.min_y, p[1]);
            bounds.max_y = (std::max)(bounds.max_y, p[1]);
        }
    };
    if (only_keys) {
        for (const auto& key : *only_keys) {
            const auto it = positions.find(key);
            if (it != positions.end()) consider(it->second);
        }
    } else {
        for (const auto& entry : positions) consider(entry.second);
    }
    return bounds;
}

void fit_graph_camera_to_bounds(WorldForgeGraphCamera& camera, float canvas_w, float canvas_h,
    const WorldForgeGraphBounds& bounds, float pad) {
    if (!bounds.valid || canvas_w < 1.0f || canvas_h < 1.0f) {
        camera.zoom = 1.0f;
        camera.pan = {0.0f, 0.0f};
        return;
    }
    const float content_w = (std::max)(bounds.max_x - bounds.min_x, 80.0f);
    const float content_h = (std::max)(bounds.max_y - bounds.min_y, 80.0f);
    const float zoom_x = (canvas_w - 2.0f * pad) / content_w;
    const float zoom_y = (canvas_h - 2.0f * pad) / content_h;
    camera.zoom = (std::clamp)((std::min)(zoom_x, zoom_y), camera.min_zoom, camera.max_zoom);
    const float cx = (bounds.min_x + bounds.max_x) * 0.5f;
    const float cy = (bounds.min_y + bounds.max_y) * 0.5f;
    camera.pan[0] = canvas_w * 0.5f - cx * camera.zoom;
    camera.pan[1] = canvas_h * 0.5f - cy * camera.zoom;
}

void center_graph_camera_on(WorldForgeGraphCamera& camera, float canvas_w, float canvas_h,
    const std::array<float, 2>& world, float zoom) {
    camera.zoom = (std::clamp)(zoom, camera.min_zoom, camera.max_zoom);
    camera.pan[0] = canvas_w * 0.5f - world[0] * camera.zoom;
    camera.pan[1] = canvas_h * 0.5f - world[1] * camera.zoom;
}

void apply_graph_zoom_at_local(WorldForgeGraphCamera& camera, float local_x, float local_y, float wheel_delta) {
    if (wheel_delta == 0.0f) return;
    const float old_zoom = camera.zoom;
    camera.zoom = (std::clamp)(camera.zoom * (wheel_delta > 0.0f ? 1.1f : 1.0f / 1.1f), camera.min_zoom, camera.max_zoom);
    const float wx = (local_x - camera.pan[0]) / old_zoom;
    const float wy = (local_y - camera.pan[1]) / old_zoom;
    camera.pan[0] = local_x - wx * camera.zoom;
    camera.pan[1] = local_y - wy * camera.zoom;
}

std::array<float, 2> graph_screen_to_world(const WorldForgeGraphCamera& camera, float local_x, float local_y) {
    return {(local_x - camera.pan[0]) / camera.zoom, (local_y - camera.pan[1]) / camera.zoom};
}

std::array<float, 2> graph_world_to_screen_local(const WorldForgeGraphCamera& camera, float world_x, float world_y) {
    return {world_x * camera.zoom + camera.pan[0], world_y * camera.zoom + camera.pan[1]};
}

float graph_point_segment_distance(float px, float py, float ax, float ay, float bx, float by) {
    const float abx = bx - ax;
    const float aby = by - ay;
    const float apx = px - ax;
    const float apy = py - ay;
    const float ab_len2 = abx * abx + aby * aby;
    float t = 0.0f;
    if (ab_len2 > 1e-8f) t = (std::clamp)((apx * abx + apy * aby) / ab_len2, 0.0f, 1.0f);
    const float cx = ax + abx * t;
    const float cy = ay + aby * t;
    const float dx = px - cx;
    const float dy = py - cy;
    return std::sqrt(dx * dx + dy * dy);
}

bool draw_world_forge_graph_minimap(ImDrawList* draw, float canvas_x, float canvas_y, float canvas_w, float canvas_h,
    WorldForgeGraphCamera& camera, const WorldForgeGraphBounds& content_bounds,
    const std::unordered_map<std::string, std::array<float, 2>>& positions, float mouse_x, float mouse_y,
    bool mouse_clicked, float minimap_size) {
    if (!draw || !content_bounds.valid || canvas_w < 64.0f || canvas_h < 64.0f) return false;

    const float margin = 8.0f;
    const float mm = (std::min)(minimap_size, (std::min)(canvas_w, canvas_h) * 0.35f);
    const ImVec2 mm_min{canvas_x + canvas_w - mm - margin, canvas_y + canvas_h - mm - margin};
    const ImVec2 mm_max{mm_min.x + mm, mm_min.y + mm};
    draw->AddRectFilled(mm_min, mm_max, IM_COL32(18, 20, 24, 220), 4.0f);
    draw->AddRect(mm_min, mm_max, IM_COL32(90, 96, 110, 255), 4.0f);

    const float content_w = (std::max)(content_bounds.max_x - content_bounds.min_x, 1.0f);
    const float content_h = (std::max)(content_bounds.max_y - content_bounds.min_y, 1.0f);
    const float pad = 6.0f;
    const float scale = (std::min)((mm - 2.0f * pad) / content_w, (mm - 2.0f * pad) / content_h);
    auto to_mm = [&](float wx, float wy) {
        return ImVec2(mm_min.x + pad + (wx - content_bounds.min_x) * scale,
            mm_min.y + pad + (wy - content_bounds.min_y) * scale);
    };

    for (const auto& entry : positions) {
        const ImVec2 p = to_mm(entry.second[0], entry.second[1]);
        draw->AddCircleFilled(p, 2.0f, IM_COL32(180, 190, 210, 255));
    }

    const auto view0 = graph_screen_to_world(camera, 0.0f, 0.0f);
    const auto view1 = graph_screen_to_world(camera, canvas_w, canvas_h);
    const ImVec2 v0 = to_mm(view0[0], view0[1]);
    const ImVec2 v1 = to_mm(view1[0], view1[1]);
    draw->AddRect(ImVec2((std::min)(v0.x, v1.x), (std::min)(v0.y, v1.y)),
        ImVec2((std::max)(v0.x, v1.x), (std::max)(v0.y, v1.y)), IM_COL32(255, 200, 80, 220), 0.0f, 0, 1.5f);

    const bool hovered = mouse_x >= mm_min.x && mouse_x <= mm_max.x && mouse_y >= mm_min.y && mouse_y <= mm_max.y;
    if (hovered && mouse_clicked) {
        const float nx = (std::clamp)((mouse_x - mm_min.x - pad) / scale + content_bounds.min_x, content_bounds.min_x,
            content_bounds.max_x);
        const float ny = (std::clamp)((mouse_y - mm_min.y - pad) / scale + content_bounds.min_y, content_bounds.min_y,
            content_bounds.max_y);
        center_graph_camera_on(camera, canvas_w, canvas_h, {nx, ny}, camera.zoom);
        return true;
    }
    return false;
}

} // namespace engine
