#pragma once

#include "engine/core/result.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace engine {

/** Decoded 8-bit RGBA image. `rgba.size() == width * height * 4`, top-down rows. */
struct PngImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgba;

    [[nodiscard]] bool empty() const noexcept { return rgba.empty() || width == 0 || height == 0; }
};

/** Decode a PNG file into RGBA8 using Windows Imaging Component. */
[[nodiscard]] Result<PngImage> decode_png_file(const std::filesystem::path& path);

/** Decode PNG bytes (e.g. from a `data:image/png;base64,` payload) into RGBA8 using WIC. */
[[nodiscard]] Result<PngImage> decode_png_bytes(std::span<const std::uint8_t> bytes);

} // namespace engine
