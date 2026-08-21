#pragma once

#include "engine/core/result.h"
#include "engine/ui/hud_runtime.h"
#include "engine/assets/ui_theme_asset.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct ImDrawList;
struct ImVec2;

namespace engine {

class InventoryRuntime;
class LuaRuntime;
class UiTextureCache;

struct UiScreenPoint {
    float x = 0.0f;
    float y = 0.0f;
};

struct UiCanvasInputEvent {
    UiScreenPoint viewport_min{};
    UiScreenPoint viewport_max{};
    UiScreenPoint mouse_pos{};
    bool mouse_clicked = false;
    bool mouse_down = false;
    bool mouse_released = false;
    bool mouse_held = false;
    bool nav_next = false;
    bool nav_prev = false;
    bool activate_pressed = false;
    bool cancel_pressed = false;
    bool adjust_left = false;
    bool adjust_right = false;
    /// When set (1–4), activates that dialogue choice slot if visible.
    std::optional<int> digit_slot;
};

struct UiCanvasInputResult {
    bool handled = false;
    bool modal_popped = false;
    std::optional<std::string> activated_bind;
    /// Set when a slot-to-slot drag completed (inventory.select.* → inventory.select.*).
    std::optional<std::string> drag_from_bind;
    std::optional<std::string> drag_to_bind;
    std::string canvas_id;
    std::string widget_id;
};

// Engine-owned UI canvas stack ([DEC-0025](../../context/decisions/index.md)): always-on HUD layer +
// modal screen stack. MCP and Lua are equal clients.
class UiCanvasStack final {
public:
    void set_texture_cache(UiTextureCache* cache);
    [[nodiscard]] UiTextureCache* texture_cache() const noexcept { return textures_; }
    [[nodiscard]] Result<void> load_theme(const std::filesystem::path& path);
    [[nodiscard]] Result<void> save_theme() const;
    [[nodiscard]] UiThemeAsset& theme() noexcept { return theme_; }
    [[nodiscard]] const UiThemeAsset& theme() const noexcept { return theme_; }
    [[nodiscard]] const std::filesystem::path& theme_path() const noexcept { return theme_path_; }
    /// Optional live inventory for hover tooltips on `inventory.select.*` slots.
    void set_inventory_runtime(InventoryRuntime* inventory) noexcept { inventory_ = inventory; }
    [[nodiscard]] InventoryRuntime* inventory_runtime() const noexcept { return inventory_; }

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
    /// Gameplay HUD layer visibility. Menu-only presentation (main menu preview,
    /// front-end screens) hides it while modal canvases still draw.
    void set_hud_visible(bool visible) noexcept { hud_visible_ = visible; }
    [[nodiscard]] bool hud_visible() const noexcept { return hud_visible_; }
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
    void tick_typewriters(float delta_seconds);

    void draw_overlay(ImDrawList* draw_list, const ImVec2& image_min, const ImVec2& image_max) const;

private:
    [[nodiscard]] Result<void> ensure_loaded(const std::string& id);
    void ensure_modal_focus(const HudRuntime& runtime);
    void clear_drag_state() noexcept;

    HudRuntime hud_;
    UiThemeAsset theme_ = UiThemeAsset::built_in();
    std::filesystem::path theme_path_;
    std::map<std::string, std::filesystem::path> paths_;
    std::map<std::string, HudRuntime> canvases_;
    std::vector<std::string> modal_stack_;
    std::optional<std::string> modal_focus_widget_id_;
    UiTextureCache* textures_ = nullptr;
    InventoryRuntime* inventory_ = nullptr;
    bool hud_visible_ = true;

    struct ModalDragState {
        bool active = false;
        bool past_threshold = false;
        std::string widget_id;
        std::string bind;
        std::string image_path;
        float start_x = 0.0f;
        float start_y = 0.0f;
        float mouse_x = 0.0f;
        float mouse_y = 0.0f;
    };
    ModalDragState modal_drag_{};
};

} // namespace engine
