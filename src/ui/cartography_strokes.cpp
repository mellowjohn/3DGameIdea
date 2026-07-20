#include "engine/ui/cartography_strokes.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace engine {
namespace {

constexpr CartographyStrokeStyleInfo k_styles[static_cast<std::size_t>(CartographyStrokeStyle::Count)] = {
    {CartographyStrokeStyle::PoliticalBorder, "stroke-political-border", "stroke-political-border.png", 256.0f, 3.5f,
        true},
    {CartographyStrokeStyle::Track, "stroke-track", "stroke-track.png", 256.0f, 2.5f, true},
    {CartographyStrokeStyle::Road, "stroke-road", "stroke-road.png", 256.0f, 3.0f, true},
    {CartographyStrokeStyle::Highway, "stroke-highway", "stroke-highway.png", 256.0f, 4.5f, true},
    {CartographyStrokeStyle::Ferry, "stroke-ferry", "stroke-ferry.png", 256.0f, 3.0f, false},
    {CartographyStrokeStyle::River, "stroke-river", "stroke-river.png", 256.0f, 4.0f, false},
};

float segment_distance(float px, float py, float ax, float ay, float bx, float by) {
    const float dx = bx - ax;
    const float dy = by - ay;
    const float len2 = dx * dx + dy * dy;
    if (len2 < 1e-8f) {
        const float ex = px - ax;
        const float ey = py - ay;
        return std::sqrt(ex * ex + ey * ey);
    }
    float t = ((px - ax) * dx + (py - ay) * dy) / len2;
    t = std::clamp(t, 0.0f, 1.0f);
    const float qx = ax + dx * t;
    const float qy = ay + dy * t;
    const float ex = px - qx;
    const float ey = py - qy;
    return std::sqrt(ex * ex + ey * ey);
}

} // namespace

const CartographyStrokeStyleInfo& cartography_stroke_style_info(CartographyStrokeStyle style) {
    const auto index = static_cast<std::size_t>(style);
    if (index >= static_cast<std::size_t>(CartographyStrokeStyle::Count)) {
        return k_styles[static_cast<std::size_t>(CartographyStrokeStyle::Road)];
    }
    return k_styles[index];
}

const char* cartography_stroke_style_id(CartographyStrokeStyle style) {
    switch (style) {
    case CartographyStrokeStyle::PoliticalBorder: return "political_border";
    case CartographyStrokeStyle::Track: return "track";
    case CartographyStrokeStyle::Road: return "road";
    case CartographyStrokeStyle::Highway: return "highway";
    case CartographyStrokeStyle::Ferry: return "ferry";
    case CartographyStrokeStyle::River: return "river";
    case CartographyStrokeStyle::Count: break;
    }
    return "road";
}

CartographyStrokeStyle cartography_stroke_style_from_id(std::string_view id) {
    if (id == "political_border" || id == "border" || id == "stroke-political-border")
        return CartographyStrokeStyle::PoliticalBorder;
    if (id == "track" || id == "stroke-track") return CartographyStrokeStyle::Track;
    if (id == "road" || id == "stroke-road") return CartographyStrokeStyle::Road;
    if (id == "highway" || id == "stroke-highway") return CartographyStrokeStyle::Highway;
    if (id == "ferry" || id == "stroke-ferry") return CartographyStrokeStyle::Ferry;
    if (id == "river" || id == "stroke-river") return CartographyStrokeStyle::River;
    return CartographyStrokeStyle::Road;
}

std::vector<CartographyStrokeStamp> build_cartography_stroke_stamps(const std::vector<CartographyStrokePoint>& points,
    float half_width_px, float repeat_px, CartographyStrokeBuildStats* stats) {
    std::vector<CartographyStrokeStamp> stamps;
    CartographyStrokeBuildStats local{};
    if (stats) *stats = local;
    if (points.size() < 2 || half_width_px <= 0.0f) return stamps;
    const float repeat = repeat_px > 1.0f ? repeat_px : 256.0f;
    float u_cursor = 0.0f;
    for (std::size_t i = 1; i < points.size(); ++i) {
        const auto& a = points[i - 1];
        const auto& b = points[i];
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.5f) continue;
        ++local.segment_count;
        local.total_length_px += len;
        const float inv = 1.0f / len;
        const float nx = -dy * inv * half_width_px;
        const float ny = dx * inv * half_width_px;
        CartographyStrokeStamp stamp{};
        stamp.p0 = {a.x + nx, a.y + ny};
        stamp.p1 = {b.x + nx, b.y + ny};
        stamp.p2 = {b.x - nx, b.y - ny};
        stamp.p3 = {a.x - nx, a.y - ny};
        stamp.u0 = u_cursor / repeat;
        stamp.u1 = (u_cursor + len) / repeat;
        stamps.push_back(stamp);
        ++local.stamp_count;
        u_cursor += len;
    }
    if (stats) *stats = local;
    return stamps;
}

float cartography_stroke_point_polyline_distance(float px, float py,
    const std::vector<CartographyStrokePoint>& points) {
    if (points.size() < 2) return 1.0e9f;
    float best = 1.0e9f;
    for (std::size_t i = 1; i < points.size(); ++i) {
        best = std::min(best,
            segment_distance(px, py, points[i - 1].x, points[i - 1].y, points[i].x, points[i].y));
    }
    return best;
}

} // namespace engine
