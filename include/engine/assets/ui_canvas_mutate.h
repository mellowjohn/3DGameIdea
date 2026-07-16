#pragma once

#include "engine/assets/ui_canvas_asset.h"
#include "engine/core/result.h"

#include <filesystem>
#include <string>

namespace engine {

/// Shared structural edits for `*.uicanvas.json` (MCP + editor Canvas).
/// `params_json` is a JSON object with action-specific fields.
[[nodiscard]] Result<UiCanvasAsset> mutate_ui_canvas_asset(UiCanvasAsset canvas, const std::string& action,
    const std::string& params_json);

/// Load → mutate → atomic save. Returns updated asset on success.
[[nodiscard]] Result<UiCanvasAsset> mutate_ui_canvas_file(const std::filesystem::path& absolute_path,
    const std::string& action, const std::string& params_json);

} // namespace engine
