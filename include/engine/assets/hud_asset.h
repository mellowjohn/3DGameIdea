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
enum class HudImageMode : std::uint8_t { Stretch, Contain };

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
    /// How `image` fits the widget rect (GPU texture draw is follow-on; placeholder until then).
    HudImageMode image_mode = HudImageMode::Stretch;
    /// Optional RGBA 0–255. Alpha 0 means "use draw defaults".
    std::array<float, 4> color{{0.0f, 0.0f, 0.0f, 0.0f}};
    /// Optional design-space font size; 0 means use widget height / defaults.
    float font_size = 0.0f;
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
    [[nodiscard]] bool has_color() const noexcept { return color[3] > 0.0f; }
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
