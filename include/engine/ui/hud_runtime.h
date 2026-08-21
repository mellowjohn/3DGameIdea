#pragma once

#include "engine/assets/ui_canvas_asset.h"
#include "engine/core/result.h"

#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

struct ImDrawList;
struct ImVec2;

namespace engine {

class UiTextureCache;
struct UiThemeAsset;

class HudRuntime final {
public:
    void set_texture_cache(UiTextureCache* cache) noexcept { textures_ = cache; }
    [[nodiscard]] UiTextureCache* texture_cache() const noexcept { return textures_; }
    void set_theme(const UiThemeAsset* theme) noexcept { theme_ = theme; }
    [[nodiscard]] const UiThemeAsset* theme() const noexcept { return theme_; }

    [[nodiscard]] Result<void> load(const std::filesystem::path& path);
    [[nodiscard]] Result<void> load_from_json(const std::string& text, const std::string& source_name = "hud.json");
    void clear();

    void reset_player_health(double current = 100.0, double max = 100.0);
    /// Secondary shared resource (stamina) — dodge + melee. All archetypes use stamina.
    void set_resource(double current, double max);
    /// Magicka / focus bar — visible while a magic weapon is held (Terraria-style).
    void set_magicka(double current, double max);
    /// Runecaster discrete cast charges (0..max, max typically 5). Syncs pip widgets.
    void set_rune_charges(int current, int max = 5);
    [[nodiscard]] int rune_charges() const;
    [[nodiscard]] int rune_charges_max() const;
    /// Seeds stamina bar for all lanes; shows regenerating rune pips for Runecaster only.
    void apply_archetype_hud(const std::string& archetype_id);
    void set_number(const std::string& bind, double value);
    void set_bool(const std::string& bind, bool value);
    void set_text(const std::string& bind, std::string value);
    /// Runtime project-relative PNG path for widgets with `imageBind` (empty clears).
    void set_image(const std::string& bind, std::string path);
    /// Like set_text, but reveals characters over time (typewriter). Used for dialogue body.
    void set_text_typed(const std::string& bind, std::string value, float chars_per_second = 48.0f);
    void tick_typewriter(float delta_seconds);
    [[nodiscard]] bool typewriter_complete(const std::string& bind) const;
    /// If typing, finish instantly and return true. If already complete, return false.
    [[nodiscard]] bool skip_typewriter(const std::string& bind);
    void apply_text_scroll(const std::string& widget_id, float wheel_delta);

    void set_visible(const std::string& widget_id, bool visible);
    void set_enabled(const std::string& widget_id, bool enabled);
    /// Runtime RGBA 0–255 override for a widget's fill/text color (cleared on load).
    void set_color(const std::string& widget_id, float r, float g, float b, float a = 255.0f);
    void clear_color(const std::string& widget_id);
    /// Runtime design-space offset override (cleared on load). Used for cinematic pans.
    void set_offset(const std::string& widget_id, float x, float y);
    void clear_offset(const std::string& widget_id);
    /// Runtime design-space size override (cleared on load). Used for Ken Burns zoom.
    void set_size(const std::string& widget_id, float w, float h);
    void clear_size(const std::string& widget_id);
    /// Runtime draw opacity override 0–1 (cleared on load). Multiplies authored opacity.
    void set_opacity(const std::string& widget_id, float opacity01);
    void clear_opacity(const std::string& widget_id);
    void set_health(double current, double max);

    [[nodiscard]] std::optional<double> get_number(const std::string& bind) const;
    [[nodiscard]] std::optional<bool> get_bool(const std::string& bind) const;
    [[nodiscard]] std::optional<std::string> get_text(const std::string& bind) const;
    [[nodiscard]] std::optional<std::string> get_image(const std::string& bind) const;
    [[nodiscard]] bool is_visible(const std::string& widget_id) const;
    [[nodiscard]] bool is_enabled(const std::string& widget_id) const;
    [[nodiscard]] const UiCanvasAsset& asset() const noexcept { return asset_; }
    [[nodiscard]] bool has_widgets() const noexcept { return !asset_.widgets.empty(); }

    void draw_overlay(ImDrawList* draw_list, const ImVec2& image_min, const ImVec2& image_max,
        const std::optional<std::string>& focused_widget_id = std::nullopt) const;

    [[nodiscard]] std::vector<std::string> focusable_widget_ids() const;
    [[nodiscard]] std::optional<std::string> hit_test_widget(const ImVec2& image_min, const ImVec2& image_max,
        const ImVec2& mouse_pos) const;
    /// Sets a focused slider's bind from mouse X along the track. Returns false if not a slider hit.
    [[nodiscard]] bool apply_slider_click(const ImVec2& image_min, const ImVec2& image_max, const std::string& widget_id,
        const ImVec2& mouse_pos);
    [[nodiscard]] std::string widget_display_label(const HudWidget& widget) const;

private:
    void reset_widget_flags_from_asset();

    UiCanvasAsset asset_;
    std::map<std::string, double> numbers_;
    std::map<std::string, bool> bools_;
    std::map<std::string, std::string> texts_;
    std::map<std::string, std::string> images_;
    std::map<std::string, std::string> typed_full_;
    // Number of UTF-8 code points revealed (rather than bytes). Keeping this separate
    // from the UTF-8 storage avoids showing partial multi-byte characters mid-line.
    std::map<std::string, float> typed_chars_;
    std::map<std::string, float> typed_cps_;
    mutable std::map<std::string, float> text_scroll_y_;
    std::map<std::string, bool> visibility_;
    std::map<std::string, bool> enabled_;
    std::map<std::string, std::array<float, 4>> color_overrides_;
    std::map<std::string, std::array<float, 2>> offset_overrides_;
    std::map<std::string, std::array<float, 2>> size_overrides_;
    std::map<std::string, float> opacity_overrides_;
    /// 0..1 hover amount per widget id (eased each draw).
    mutable std::map<std::string, float> button_hover_t_;
    /// 0 = idle; else 0..1 click-bounce progress keyed by pick button id.
    mutable std::map<std::string, float> button_bounce_t_;
    /// ImGui frame stamp so hover/bounce advance once per pick even if art+rim both draw.
    mutable std::map<std::string, int> button_card_fx_frame_;
    UiTextureCache* textures_ = nullptr;
    const UiThemeAsset* theme_ = nullptr;
};

} // namespace engine
