#pragma once

#include "engine/core/result.h"

#include <filesystem>
#include <string>

namespace engine {

/// Capture the editor HWND client area (or full window) to a PNG under project `out/`.
[[nodiscard]] Result<std::filesystem::path> capture_window_png(void* hwnd, const std::filesystem::path& project_root,
    const std::string& filename_stem, bool client_area_only = true);

} // namespace engine
