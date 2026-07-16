#include "engine/assets/ui_canvas_asset.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>

namespace engine {
namespace {

EngineError canvas_error(std::string code, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Validation, "ui_canvas", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

// Reuse HUD widget parse by wrapping into a temporary HudAsset JSON document.
Result<HudAsset> parse_widgets_via_hud(const nlohmann::json& widgets, const std::string& source_name) {
    nlohmann::json wrapper;
    wrapper["schemaVersion"] = 1;
    wrapper["id"] = "widgets";
    wrapper["widgets"] = widgets;
    return HudAsset::parse(wrapper.dump(), source_name);
}

Result<UiCanvasScaleMode> parse_scale_mode(const std::string& raw) {
    std::string key = raw;
    std::transform(key.begin(), key.end(), key.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (key == "letterbox") return Result<UiCanvasScaleMode>::success(UiCanvasScaleMode::Letterbox);
    if (key == "fill_edges" || key == "filledges" || key == "cover")
        return Result<UiCanvasScaleMode>::success(UiCanvasScaleMode::FillEdges);
    return Result<UiCanvasScaleMode>::failure(
        canvas_error("UICANVAS-SCALE", "Unsupported scaleMode: " + raw, "Use letterbox or fill_edges."));
}

const char* scale_mode_name(UiCanvasScaleMode mode) {
    switch (mode) {
    case UiCanvasScaleMode::Letterbox: return "letterbox";
    case UiCanvasScaleMode::FillEdges: return "fill_edges";
    }
    return "letterbox";
}

} // namespace

std::filesystem::path default_player_ui_canvas_path(const std::filesystem::path& project_root) {
    return project_root / "assets" / "ui" / "player.uicanvas.json";
}

UiCanvasLetterbox compute_ui_canvas_letterbox(float view_min_x, float view_min_y, float view_max_x, float view_max_y,
    float design_w, float design_h) {
    return compute_ui_canvas_layout(view_min_x, view_min_y, view_max_x, view_max_y, design_w, design_h,
        UiCanvasScaleMode::Letterbox);
}

UiCanvasLetterbox compute_ui_canvas_layout(float view_min_x, float view_min_y, float view_max_x, float view_max_y,
    float design_w, float design_h, UiCanvasScaleMode scale_mode) {
    UiCanvasLetterbox box;
    box.design_w = design_w > 0.0f ? design_w : 1920.0f;
    box.design_h = design_h > 0.0f ? design_h : 1080.0f;
    const float view_w = std::max(0.0f, view_max_x - view_min_x);
    const float view_h = std::max(0.0f, view_max_y - view_min_y);
    if (view_w <= 0.0f || view_h <= 0.0f) {
        box.scale = 0.0f;
        box.content_min_x = view_min_x;
        box.content_min_y = view_min_y;
        box.content_max_x = view_max_x;
        box.content_max_y = view_max_y;
        return box;
    }
    if (scale_mode == UiCanvasScaleMode::FillEdges) {
        box.scale = std::max(view_w / box.design_w, view_h / box.design_h);
    } else {
        box.scale = std::min(view_w / box.design_w, view_h / box.design_h);
    }
    const float content_w = box.design_w * box.scale;
    const float content_h = box.design_h * box.scale;
    const float pad_x = (view_w - content_w) * 0.5f;
    const float pad_y = (view_h - content_h) * 0.5f;
    box.content_min_x = view_min_x + pad_x;
    box.content_min_y = view_min_y + pad_y;
    box.content_max_x = box.content_min_x + content_w;
    box.content_max_y = box.content_min_y + content_h;
    return box;
}

UiCanvasAsset UiCanvasAsset::from_hud(const HudAsset& hud) {
    UiCanvasAsset canvas;
    canvas.schema_version = 1;
    canvas.id = hud.id;
    canvas.design_resolution = {{1920.0f, 1080.0f}};
    canvas.scale_mode = UiCanvasScaleMode::Letterbox;
    canvas.widgets = hud.widgets;
    return canvas;
}

Result<UiCanvasAsset> UiCanvasAsset::parse(const std::string& text, const std::string& source_name) {
    try {
        const auto json = nlohmann::json::parse(text);
        if (!json.is_object()) {
            return Result<UiCanvasAsset>::failure(
                canvas_error("UICANVAS-ROOT", source_name + " must be a JSON object",
                    "Wrap widgets in an object with schemaVersion and designResolution."));
        }
        UiCanvasAsset asset;
        asset.schema_version = json.value("schemaVersion", 1);
        if (asset.schema_version != 1) {
            return Result<UiCanvasAsset>::failure(
                canvas_error("UICANVAS-SCHEMA", "Unsupported UI canvas schemaVersion", "Use schemaVersion 1."));
        }
        asset.id = json.value("id", std::string{});
        if (json.contains("designResolution") && json["designResolution"].is_array() &&
            json["designResolution"].size() >= 2) {
            asset.design_resolution[0] = json["designResolution"][0].get<float>();
            asset.design_resolution[1] = json["designResolution"][1].get<float>();
        }
        if (!(asset.design_resolution[0] > 0.0f) || !(asset.design_resolution[1] > 0.0f) ||
            !std::isfinite(asset.design_resolution[0]) || !std::isfinite(asset.design_resolution[1])) {
            return Result<UiCanvasAsset>::failure(
                canvas_error("UICANVAS-DESIGN", "designResolution must be positive [width, height]",
                    "Example: [1920, 1080]."));
        }
        if (json.contains("scaleMode")) {
            const auto mode = parse_scale_mode(json["scaleMode"].get<std::string>());
            if (!mode) return Result<UiCanvasAsset>::failure(mode.error());
            asset.scale_mode = mode.value();
        }
        const auto widgets = json.value("widgets", nlohmann::json::array());
        if (!widgets.is_array()) {
            return Result<UiCanvasAsset>::failure(
                canvas_error("UICANVAS-WIDGETS", "widgets must be an array", "Provide a widgets array."));
        }
        auto hud_widgets = parse_widgets_via_hud(widgets, source_name);
        if (!hud_widgets) return Result<UiCanvasAsset>::failure(hud_widgets.error());
        asset.widgets = std::move(hud_widgets.value().widgets);
        return Result<UiCanvasAsset>::success(std::move(asset));
    } catch (const std::exception& ex) {
        return Result<UiCanvasAsset>::failure(
            canvas_error("UICANVAS-PARSE", "Failed to parse " + source_name, std::string(ex.what())));
    }
}

Result<UiCanvasAsset> UiCanvasAsset::load(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return Result<UiCanvasAsset>::failure(
            canvas_error("UICANVAS-IO", "UI canvas file not found: " + path.generic_string(),
                "Create assets/ui/*.uicanvas.json."));
    }
    std::ifstream input(path);
    if (!input) {
        return Result<UiCanvasAsset>::failure(
            canvas_error("UICANVAS-IO", "Failed to read UI canvas: " + path.generic_string(), "Check permissions."));
    }
    const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    return parse(text, path.generic_string());
}

std::string UiCanvasAsset::to_json() const {
    nlohmann::json json;
    json["schemaVersion"] = schema_version;
    json["id"] = id;
    json["designResolution"] = {design_resolution[0], design_resolution[1]};
    if (scale_mode != UiCanvasScaleMode::Letterbox) json["scaleMode"] = scale_mode_name(scale_mode);
    // Serialize widgets with the same shape as HudAsset.
    HudAsset hud;
    hud.schema_version = 1;
    hud.id = id;
    hud.widgets = widgets;
    const auto hud_json = nlohmann::json::parse(hud.to_json());
    json["widgets"] = hud_json["widgets"];
    return json.dump(2);
}

Result<void> UiCanvasAsset::save_atomic(const std::filesystem::path& path) const {
    const auto written = write_ui_canvas_json_atomic(path, to_json());
    if (!written) return Result<void>::failure(written.error());
    return Result<void>::success();
}

Result<std::string> write_ui_canvas_json_atomic(const std::filesystem::path& absolute_path, const std::string& source) {
    const auto validated = UiCanvasAsset::parse(source, absolute_path.generic_string());
    if (!validated) return Result<std::string>::failure(validated.error());
    const auto parent = absolute_path.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    const auto temp = absolute_path.string() + ".tmp";
    const auto backup = absolute_path.string() + ".bak";
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out) {
            return Result<std::string>::failure(
                canvas_error("UICANVAS-IO", "Failed to open temp UI canvas file", "Check permissions."));
        }
        out << source;
        if (!source.empty() && source.back() != '\n') out << '\n';
    }
    if (std::filesystem::exists(absolute_path))
        std::filesystem::copy_file(absolute_path, backup, std::filesystem::copy_options::overwrite_existing);
    std::filesystem::rename(temp, absolute_path);
    if (std::filesystem::exists(backup)) std::filesystem::remove(backup);
    return Result<std::string>::success(absolute_path.generic_string());
}

} // namespace engine
