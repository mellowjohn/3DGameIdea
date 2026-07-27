#pragma once

#include "engine/ui/editor_ui_hotspots.h"

#include <cstdint>
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

/// Branded strip under the main menu: icon + RPG ENGINE / project / area · world / Save status.
/// `app_icon_tex` is an ImGui/D3D12 texture id (0 = text-only fallback).
void draw_app_header(const std::filesystem::path& project_root, const char* active_area, bool scene_dirty,
    bool world_forge_dirty, const std::string& status_line, bool* request_save,
    EditorUiHotspotRegistry* hotspots = nullptr, const char* world_stem = nullptr,
    std::uint64_t app_icon_tex = 0);

/// Full-screen boot splash (Unity-style) shown before editor chrome is ready.
void draw_boot_overlay(const char* stage, const char* detail, float progress01, std::uint64_t app_icon_tex = 0);
} // namespace EditorChrome

} // namespace engine
