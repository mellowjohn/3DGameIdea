#pragma once

#include <array>

namespace engine {

/// Axis-aligned viewport rect in screen pixels (Game image region).
struct ViewportRect {
    float min_x = 0.0f;
    float min_y = 0.0f;
    float max_x = 0.0f;
    float max_y = 0.0f;

    [[nodiscard]] float width() const noexcept { return max_x - min_x; }
    [[nodiscard]] float height() const noexcept { return max_y - min_y; }
};

/// Project a world point through a row-major view-projection matrix into screen pixels.
/// Returns false when the point is behind the camera or outside the clip depth range.
[[nodiscard]] bool project_world_to_screen(const std::array<float, 16>& view_projection, const ViewportRect& viewport,
    float world_x, float world_y, float world_z, float& screen_x, float& screen_y, float& depth);

} // namespace engine
