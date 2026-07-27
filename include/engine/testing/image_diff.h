#pragma once

#include "engine/core/result.h"

#include <cstdint>
#include <span>

namespace engine {

struct ImageDiffStats {
    double mean_abs_rgb = 0.0;
    double max_abs_rgb = 0.0;
    std::uint64_t compared_pixels = 0;
};

/// Mean / max absolute RGB error over tightly packed RGBA8 buffers (alpha ignored).
[[nodiscard]] Result<ImageDiffStats> mean_abs_rgb_diff(std::uint32_t width, std::uint32_t height,
    std::span<const std::uint8_t> actual_rgba, std::span<const std::uint8_t> baseline_rgba);

} // namespace engine
