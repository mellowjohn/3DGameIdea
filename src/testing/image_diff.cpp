#include "engine/testing/image_diff.h"

#include "engine/core/error.h"

#include <algorithm>
#include <cmath>

namespace engine {

Result<ImageDiffStats> mean_abs_rgb_diff(std::uint32_t width, std::uint32_t height,
    std::span<const std::uint8_t> actual_rgba, std::span<const std::uint8_t> baseline_rgba) {
    const std::size_t expected =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
    if (width == 0 || height == 0 || actual_rgba.size() < expected || baseline_rgba.size() < expected) {
        return Result<ImageDiffStats>::failure(
            EngineError{"VREG-SIZE", Severity::Error, ErrorCategory::Validation, "visual_regression",
                "RGBA buffers do not match width/height", std::nullopt, {},
                "Capture and baseline must share resolution and tightly packed RGBA8."});
    }

    double sum = 0.0;
    double max_err = 0.0;
    const std::uint64_t pixels = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
    for (std::uint64_t i = 0; i < pixels; ++i) {
        const std::size_t o = static_cast<std::size_t>(i) * 4u;
        const double dr = std::abs(static_cast<int>(actual_rgba[o + 0]) - static_cast<int>(baseline_rgba[o + 0]));
        const double dg = std::abs(static_cast<int>(actual_rgba[o + 1]) - static_cast<int>(baseline_rgba[o + 1]));
        const double db = std::abs(static_cast<int>(actual_rgba[o + 2]) - static_cast<int>(baseline_rgba[o + 2]));
        const double pixel_mean = (dr + dg + db) / 3.0;
        sum += pixel_mean;
        max_err = std::max(max_err, std::max({dr, dg, db}));
    }

    ImageDiffStats stats{};
    stats.compared_pixels = pixels;
    stats.mean_abs_rgb = pixels > 0 ? sum / static_cast<double>(pixels) : 0.0;
    stats.max_abs_rgb = max_err;
    return Result<ImageDiffStats>::success(stats);
}

} // namespace engine
