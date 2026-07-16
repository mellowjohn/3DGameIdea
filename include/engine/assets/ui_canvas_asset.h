#pragma once

#include "engine/assets/hud_asset.h"
#include "engine/core/result.h"

#include <array>
#include <filesystem>
#include <string>

namespace engine {

enum class UiCanvasScaleMode : std::uint8_t { Letterbox, FillEdges };

struct UiCanvasLetterbox {
    float scale = 1.0f;
    float content_min_x = 0.0f;
    float content_min_y = 0.0f;
    float content_max_x = 0.0f;
    float content_max_y = 0.0f;
    float design_w = 1920.0f;
    float design_h = 1080.0f;
};

struct UiCanvasAsset {
    int schema_version = 1;
    std::string id;
    std::array<float, 2> design_resolution{{1920.0f, 1080.0f}};
    UiCanvasScaleMode scale_mode = UiCanvasScaleMode::Letterbox;
    std::vector<HudWidget> widgets;

    [[nodiscard]] static Result<UiCanvasAsset> load(const std::filesystem::path& path);
    [[nodiscard]] static Result<UiCanvasAsset> parse(const std::string& text,
        const std::string& source_name = "canvas.uicanvas.json");
    [[nodiscard]] static UiCanvasAsset from_hud(const HudAsset& hud);
    [[nodiscard]] Result<void> save_atomic(const std::filesystem::path& path) const;
    [[nodiscard]] std::string to_json() const;
};

[[nodiscard]] std::filesystem::path default_player_ui_canvas_path(const std::filesystem::path& project_root);
[[nodiscard]] Result<std::string> write_ui_canvas_json_atomic(const std::filesystem::path& absolute_path,
    const std::string& source);

[[nodiscard]] UiCanvasLetterbox compute_ui_canvas_letterbox(float view_min_x, float view_min_y, float view_max_x,
    float view_max_y, float design_w, float design_h);

[[nodiscard]] UiCanvasLetterbox compute_ui_canvas_layout(float view_min_x, float view_min_y, float view_max_x,
    float view_max_y, float design_w, float design_h, UiCanvasScaleMode scale_mode);

} // namespace engine
