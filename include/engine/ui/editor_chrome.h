#pragma once

#include "engine/ui/editor_ui_hotspots.h"

#include <filesystem>
#include <string>

struct ImGuiStyle;

namespace engine {

/// Pencil tokens from `context/design/rpg-engine-ui.pen` (shared with World Forge chrome).
namespace EditorChrome {
inline constexpr float kHeaderHeight = 56.0f;

void apply_style(ImGuiStyle& style);
void push_panel_colors();
void pop_panel_colors();

/// Branded strip under the main menu: RPG ENGINE / project / active area / Save status.
void draw_app_header(const std::filesystem::path& project_root, const char* active_area, bool scene_dirty,
    bool world_forge_dirty, const std::string& status_line, bool* request_save,
    EditorUiHotspotRegistry* hotspots = nullptr);
} // namespace EditorChrome

} // namespace engine
