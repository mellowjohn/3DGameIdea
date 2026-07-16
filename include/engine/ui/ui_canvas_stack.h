#pragma once

#include "engine/core/result.h"
#include "engine/ui/hud_runtime.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct ImDrawList;
struct ImVec2;

namespace engine {

class LuaRuntime;

struct UiScreenPoint {
    float x = 0.0f;
    float y = 0.0f;
};

struct UiCanvasInputEvent {
    UiScreenPoint viewport_min{};
    UiScreenPoint viewport_max{};
    UiScreenPoint mouse_pos{};
    bool mouse_clicked = false;
    bool nav_next = false;
    bool nav_prev = false;
    bool activate_pressed = false;
    bool cancel_pressed = false;
    bool adjust_left = false;
    bool adjust_right = false;
};

struct UiCanvasInputResult {
    bool handled = false;
    bool modal_popped = false;
    std::optional<std::string> activated_bind;
    std::string canvas_id;
    std::string widget_id;
};

// Engine-owned UI canvas stack ([DEC-0025](../../context/decisions/index.md)): always-on HUD layer +
// modal screen stack. MCP and Lua are equal clients.
class UiCanvasStack final {
public:
    [[nodiscard]] Result<void> set_hud(const std::filesystem::path& path);
    [[nodiscard]] Result<void> register_canvas(std::string id, const std::filesystem::path& path);

    [[nodiscard]] Result<void> push(const std::string& id);
    [[nodiscard]] Result<void> pop();
    [[nodiscard]] Result<void> show(const std::string& id);
    [[nodiscard]] Result<void> hide(const std::string& id);
    void clear_modals();

    [[nodiscard]] HudRuntime& hud() noexcept { return hud_; }
    [[nodiscard]] const HudRuntime& hud() const noexcept { return hud_; }
    [[nodiscard]] bool has_hud() const noexcept { return hud_.has_widgets(); }
    [[nodiscard]] HudRuntime* find_canvas(const std::string& id);
    [[nodiscard]] const HudRuntime* find_canvas(const std::string& id) const;

    [[nodiscard]] bool has_modal() const noexcept { return !modal_stack_.empty(); }
    [[nodiscard]] std::optional<std::string> top_modal() const;
    [[nodiscard]] std::vector<std::string> modal_ids() const { return modal_stack_; }
    [[nodiscard]] bool is_registered(const std::string& id) const;
    [[nodiscard]] const std::optional<std::string>& modal_focus_widget_id() const noexcept {
        return modal_focus_widget_id_;
    }

    void reset_modal_focus();
    [[nodiscard]] UiCanvasInputResult handle_modal_input(const UiCanvasInputEvent& event, LuaRuntime* lua_runtime);

    void draw_overlay(ImDrawList* draw_list, const ImVec2& image_min, const ImVec2& image_max) const;

private:
    [[nodiscard]] Result<void> ensure_loaded(const std::string& id);
    void ensure_modal_focus(const HudRuntime& runtime);

    HudRuntime hud_;
    std::map<std::string, std::filesystem::path> paths_;
    std::map<std::string, HudRuntime> canvases_;
    std::vector<std::string> modal_stack_;
    std::optional<std::string> modal_focus_widget_id_;
};

} // namespace engine
