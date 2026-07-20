#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace engine {

/// Cartography stroke style ids for Map Canvas image-stamp rendering.
/// Mountains are not a stroke style — they live in discrete map plates.
enum class CartographyStrokeStyle : std::uint8_t {
    PoliticalBorder = 0,
    Track,
    Road,
    Highway,
    Ferry,
    River,
    Count
};

struct CartographyStrokeStyleInfo {
    CartographyStrokeStyle style = CartographyStrokeStyle::Road;
    /// Texture key in WorldForgeEditorSession::cartography_tex (e.g. "stroke-road").
    const char* texture_key = "";
    /// Project-relative PNG under assets/ui/cartography/strokes/.
    const char* file_name = "";
    /// Horizontal repeat length of the tile in pixels (matches generated assets).
    float repeat_px = 256.0f;
    /// Half-height of the ribbon in screen pixels at zoom 1 (scaled by caller zoom).
    float half_width_px = 4.0f;
    /// When true, multiply stamp by faction/route tint; when false, draw white.
    bool accepts_tint = true;
};

[[nodiscard]] const CartographyStrokeStyleInfo& cartography_stroke_style_info(CartographyStrokeStyle style);
[[nodiscard]] const char* cartography_stroke_style_id(CartographyStrokeStyle style);
[[nodiscard]] CartographyStrokeStyle cartography_stroke_style_from_id(std::string_view id);

struct CartographyStrokePoint {
    float x = 0.0f;
    float y = 0.0f;
};

/// One textured quad along a polyline (screen space). UV u spans the tile repeat.
struct CartographyStrokeStamp {
    CartographyStrokePoint p0{}; // A - N
    CartographyStrokePoint p1{}; // B - N
    CartographyStrokePoint p2{}; // B + N
    CartographyStrokePoint p3{}; // A + N
    float u0 = 0.0f;
    float u1 = 1.0f;
};

struct CartographyStrokeBuildStats {
    std::size_t segment_count = 0;
    std::size_t stamp_count = 0;
    float total_length_px = 0.0f;
};

/// Build textured ribbon stamps for a polyline. Degenerate / zero-length segments are skipped.
/// `half_width_px` is the ribbon half-thickness in the same units as the points.
[[nodiscard]] std::vector<CartographyStrokeStamp> build_cartography_stroke_stamps(
    const std::vector<CartographyStrokePoint>& points, float half_width_px, float repeat_px,
    CartographyStrokeBuildStats* stats = nullptr);

/// Distance from point to polyline in 2D (same metric as map hit-testing).
[[nodiscard]] float cartography_stroke_point_polyline_distance(float px, float py,
    const std::vector<CartographyStrokePoint>& points);

} // namespace engine
