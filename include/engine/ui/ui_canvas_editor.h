#pragma once

#include "engine/assets/ui_canvas_asset.h"
#include "engine/core/result.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

namespace engine {

enum class UiCanvasEditMode : std::uint8_t { Idle, Move, Resize };
enum class UiCanvasResizeHandle : std::uint8_t { None, N, S, E, W, NE, NW, SE, SW };

struct UiCanvasEditorSession {
    std::filesystem::path path;
    UiCanvasAsset canvas;
    std::optional<std::string> selected_id;
    UiCanvasEditMode edit_mode = UiCanvasEditMode::Idle;
    UiCanvasResizeHandle resize_handle = UiCanvasResizeHandle::None;
    float drag_start_mouse_x = 0.0f;
    float drag_start_mouse_y = 0.0f;
    std::array<float, 2> drag_start_offset{{0.0f, 0.0f}};
    std::array<float, 4> drag_start_rect{{0.0f, 0.0f, 0.0f, 0.0f}}; // x,y,w,h design space
    bool dirty = false;
    /// When true, Save applies this canvas as the play-test HUD; otherwise registers by canvas.id as a modal.
    bool apply_as_hud = true;
    bool show_new_popup = false;
    std::array<char, 64> new_canvas_id{{}};

    [[nodiscard]] Result<void> load(const std::filesystem::path& absolute_path);
    [[nodiscard]] Result<void> save();
    [[nodiscard]] Result<void> create_new(const std::filesystem::path& project_root, const std::string& canvas_id);
};

/// Draw UI Canvas authoring into the active Viewports tab content region.
void draw_ui_canvas_viewport(UiCanvasEditorSession& session, const std::filesystem::path& project_root,
    const std::function<void(const std::filesystem::path&)>& on_saved);

} // namespace engine
