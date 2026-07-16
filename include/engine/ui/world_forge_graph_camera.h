#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

struct ImDrawList;
struct ImVec2;

namespace engine {

/// Shared World Forge graph camera (TICKET-0165 / DEC-0027).
struct WorldForgeGraphCamera {
    float zoom = 1.0f;
    std::array<float, 2> pan{{0.0f, 0.0f}};
    float min_zoom = 0.2f;
    float max_zoom = 2.0f;
};

struct WorldForgeGraphBounds {
    float min_x = 0.0f;
    float max_x = 0.0f;
    float min_y = 0.0f;
    float max_y = 0.0f;
    bool valid = false;
};

[[nodiscard]] WorldForgeGraphBounds compute_graph_bounds(
    const std::unordered_map<std::string, std::array<float, 2>>& positions,
    const std::vector<std::string>* only_keys = nullptr);

void fit_graph_camera_to_bounds(WorldForgeGraphCamera& camera, float canvas_w, float canvas_h,
    const WorldForgeGraphBounds& bounds, float pad = 48.0f);

void center_graph_camera_on(WorldForgeGraphCamera& camera, float canvas_w, float canvas_h,
    const std::array<float, 2>& world, float zoom);

void apply_graph_zoom_at_local(WorldForgeGraphCamera& camera, float local_x, float local_y, float wheel_delta);

[[nodiscard]] std::array<float, 2> graph_screen_to_world(const WorldForgeGraphCamera& camera, float local_x,
    float local_y);
[[nodiscard]] std::array<float, 2> graph_world_to_screen_local(const WorldForgeGraphCamera& camera, float world_x,
    float world_y);

[[nodiscard]] float graph_point_segment_distance(float px, float py, float ax, float ay, float bx, float by);

/// Draw a corner minimap; returns true if the click changed pan (caller should stop other hit tests).
bool draw_world_forge_graph_minimap(ImDrawList* draw, float canvas_x, float canvas_y, float canvas_w, float canvas_h,
    WorldForgeGraphCamera& camera, const WorldForgeGraphBounds& content_bounds,
    const std::unordered_map<std::string, std::array<float, 2>>& positions, float mouse_x, float mouse_y,
    bool mouse_clicked, float minimap_size = 120.0f);

} // namespace engine
