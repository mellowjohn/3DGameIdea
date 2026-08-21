#pragma once

#include "engine/core/result.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace engine {

enum class HudWidgetType : std::uint8_t { Bar, Text, Panel, Button, Toggle, Slider, Image };
enum class HudAnchor : std::uint8_t { TopLeft, TopRight, BottomLeft, BottomRight, Center };
enum class HudTextAlign : std::uint8_t { Left, Center, Right };
enum class HudTextVAlign : std::uint8_t { Top, Middle, Bottom };
enum class HudImageMode : std::uint8_t { Stretch, Contain, NineSlice };

struct HudWidget {
    std::string id;
    HudWidgetType type = HudWidgetType::Bar;
    HudAnchor anchor = HudAnchor::TopLeft;
    std::array<float, 2> offset{{0.0f, 0.0f}};
    std::array<float, 2> size{{100.0f, 16.0f}};
    std::string bind;
    std::string max_bind;
    std::string label;
    /// Authored default string for text widgets (seeded into the bind on load).
    std::string text;
    /// Optional project-relative image path (PNG). Empty = no image / solid fill.
    std::string image;
    /// Optional bind key for a runtime image path (`hud_set_image` / `ui_canvas_set_image`).
    /// When set, the bound path overrides authored `image` (empty bind value = no image).
    std::string image_bind;
    /// How `image` fits the widget rect (`stretch` fills; `contain` letterboxes;
    /// `nine_slice` preserves border pixels via `imageSlice`).
    HudImageMode image_mode = HudImageMode::Stretch;
    /// Texture-space border insets [left, top, right, bottom] for `nine_slice`.
    std::array<float, 4> image_slice{{0.0f, 0.0f, 0.0f, 0.0f}};
    /// Design-space content insets [left, top, right, bottom] for text/button labels
    /// (and wrap/clip). Keeps copy inside ornate plate/card chrome without shrinking the
    /// hit/draw rect for the backdrop image itself.
    std::array<float, 4> padding{{0.0f, 0.0f, 0.0f, 0.0f}};
    /// Optional RGBA 0–255. Alpha 0 means "use draw defaults".
    std::array<float, 4> color{{0.0f, 0.0f, 0.0f, 0.0f}};
    /// Optional named role from `assets/ui/ui-theme.json` (`primaryButton`, `title`, …).
    std::string theme_role;
    /// Optional token name for fill (panels/buttons) or text (labels) when `color` is unset.
    std::string color_token;
    /// Optional token name for button/toggle label color (does not affect fill).
    std::string text_color_token;
    /// Optional RGBA 0–255 for labels. Alpha 0 means resolve from theme / contrast.
    std::array<float, 4> text_color{{0.0f, 0.0f, 0.0f, 0.0f}};
    /// Optional design-space font size; 0 means use widget height / defaults.
    float font_size = 0.0f;
    /// When true, shrink screen font (down to `minFontSize`) so wrapped text fits the
    /// padded content rect instead of clipping or overflowing the plate.
    bool fit_text = false;
    /// Design-space floor for `fitText` (0 = default 11).
    float min_font_size = 0.0f;
    /// Multiplies draw alpha (and color alpha when set). Range 0–1; default opaque.
    float opacity = 1.0f;
    /// Authored default visibility (runtime may still toggle via set_visible).
    bool visible = true;
    /// When false, widget draws dimmed and is inactive for interaction.
    bool enabled = true;
    /// Horizontal text alignment within the widget box (text widgets).
    HudTextAlign text_align = HudTextAlign::Left;
    /// Vertical text alignment within the widget box (text widgets).
    HudTextVAlign text_v_align = HudTextVAlign::Top;
    /// Optional hover tooltip (iron/gold chip). Inventory `select.*` slots prefer live item text.
    std::string tooltip;
    [[nodiscard]] bool has_color() const noexcept { return color[3] > 0.0f; }
    [[nodiscard]] bool has_text_color() const noexcept { return text_color[3] > 0.0f; }
};

struct HudAsset {
    int schema_version = 1;
    std::string id;
    std::vector<HudWidget> widgets;

    [[nodiscard]] static Result<HudAsset> load(const std::filesystem::path& path);
    [[nodiscard]] static Result<HudAsset> parse(const std::string& text, const std::string& source_name = "hud.json");
    [[nodiscard]] Result<void> save_atomic(const std::filesystem::path& path) const;
    [[nodiscard]] std::string to_json() const;
};

[[nodiscard]] std::filesystem::path default_player_hud_path(const std::filesystem::path& project_root);
[[nodiscard]] Result<std::string> write_hud_json_atomic(const std::filesystem::path& absolute_path,
    const std::string& source);

} // namespace engine
