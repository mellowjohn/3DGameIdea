#pragma once

#include "engine/core/result.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace engine {

/// Write tightly packed 8-bit RGBA pixels to a PNG under `project_root/out/` (or absolute `out_path` override).
[[nodiscard]] Result<std::filesystem::path> write_rgba_png(const std::filesystem::path& project_root,
    const std::string& filename_stem, std::uint32_t width, std::uint32_t height,
    std::span<const std::uint8_t> rgba_bytes);

/// Capture the editor HWND client area (or full window) to a PNG under project `out/`.
/// Prefer GPU backbuffer capture from the live editor when available; this GDI path is the fallback.
[[nodiscard]] Result<std::filesystem::path> capture_window_png(void* hwnd, const std::filesystem::path& project_root,
    const std::string& filename_stem, bool client_area_only = true);

} // namespace engine
