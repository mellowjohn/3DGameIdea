#pragma once

#include "engine/assets/ui_canvas_asset.h"
#include "engine/core/result.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>

struct ImDrawList;
struct ImVec2;

namespace engine {

class HudRuntime final {
public:
    [[nodiscard]] Result<void> load(const std::filesystem::path& path);
    [[nodiscard]] Result<void> load_from_json(const std::string& text, const std::string& source_name = "hud.json");
    void clear();

    void reset_player_health(double current = 100.0, double max = 100.0);
    void set_number(const std::string& bind, double value);
    void set_bool(const std::string& bind, bool value);
    void set_text(const std::string& bind, std::string value);
    void set_visible(const std::string& widget_id, bool visible);
    void set_enabled(const std::string& widget_id, bool enabled);
    void set_health(double current, double max);

    [[nodiscard]] std::optional<double> get_number(const std::string& bind) const;
    [[nodiscard]] std::optional<bool> get_bool(const std::string& bind) const;
    [[nodiscard]] std::optional<std::string> get_text(const std::string& bind) const;
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
    std::map<std::string, bool> visibility_;
    std::map<std::string, bool> enabled_;
};

} // namespace engine
