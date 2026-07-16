#include "engine/assets/ui_canvas_mutate.h"

#include "engine/assets/hud_asset.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>

namespace engine {
namespace {

EngineError mutate_error(std::string code, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Validation, "ui_canvas_mutate",
        std::move(message), ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

HudWidget* find_widget(UiCanvasAsset& canvas, const std::string& id) {
    for (auto& widget : canvas.widgets) {
        if (widget.id == id) return &widget;
    }
    return nullptr;
}

Result<HudWidgetType> parse_type(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "bar") return Result<HudWidgetType>::success(HudWidgetType::Bar);
    if (key == "text") return Result<HudWidgetType>::success(HudWidgetType::Text);
    if (key == "panel") return Result<HudWidgetType>::success(HudWidgetType::Panel);
    if (key == "button") return Result<HudWidgetType>::success(HudWidgetType::Button);
    if (key == "toggle") return Result<HudWidgetType>::success(HudWidgetType::Toggle);
    if (key == "slider") return Result<HudWidgetType>::success(HudWidgetType::Slider);
    if (key == "image") return Result<HudWidgetType>::success(HudWidgetType::Image);
    return Result<HudWidgetType>::failure(mutate_error("UICANVAS-MUTATE-TYPE", "Unsupported widget type: " + raw,
        "Use bar, text, panel, button, toggle, slider, or image."));
}

Result<HudImageMode> parse_image_mode(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "stretch") return Result<HudImageMode>::success(HudImageMode::Stretch);
    if (key == "contain") return Result<HudImageMode>::success(HudImageMode::Contain);
    return Result<HudImageMode>::failure(
        mutate_error("UICANVAS-MUTATE-IMAGE-MODE", "Unsupported imageMode: " + raw, "Use stretch or contain."));
}

Result<HudAnchor> parse_anchor(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "top_left" || key == "topleft") return Result<HudAnchor>::success(HudAnchor::TopLeft);
    if (key == "top_right" || key == "topright") return Result<HudAnchor>::success(HudAnchor::TopRight);
    if (key == "bottom_left" || key == "bottomleft") return Result<HudAnchor>::success(HudAnchor::BottomLeft);
    if (key == "bottom_right" || key == "bottomright") return Result<HudAnchor>::success(HudAnchor::BottomRight);
    if (key == "center") return Result<HudAnchor>::success(HudAnchor::Center);
    return Result<HudAnchor>::failure(
        mutate_error("UICANVAS-MUTATE-ANCHOR", "Unsupported anchor: " + raw, "Use top_left..center."));
}

Result<HudTextAlign> parse_text_align(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "left") return Result<HudTextAlign>::success(HudTextAlign::Left);
    if (key == "center" || key == "centre") return Result<HudTextAlign>::success(HudTextAlign::Center);
    if (key == "right") return Result<HudTextAlign>::success(HudTextAlign::Right);
    return Result<HudTextAlign>::failure(
        mutate_error("UICANVAS-MUTATE-TEXT-ALIGN", "Unsupported textAlign: " + raw, "Use left, center, or right."));
}

Result<HudTextVAlign> parse_text_v_align(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "top") return Result<HudTextVAlign>::success(HudTextVAlign::Top);
    if (key == "middle" || key == "center" || key == "centre") return Result<HudTextVAlign>::success(HudTextVAlign::Middle);
    if (key == "bottom") return Result<HudTextVAlign>::success(HudTextVAlign::Bottom);
    return Result<HudTextVAlign>::failure(
        mutate_error("UICANVAS-MUTATE-TEXT-VALIGN", "Unsupported textVAlign: " + raw, "Use top, middle, or bottom."));
}

} // namespace

Result<UiCanvasAsset> mutate_ui_canvas_asset(UiCanvasAsset canvas, const std::string& action_raw,
    const std::string& params_json) {
    try {
        const auto action = lower_copy(action_raw);
        const auto params = params_json.empty() ? nlohmann::json::object() : nlohmann::json::parse(params_json);
        if (!params.is_object()) {
            return Result<UiCanvasAsset>::failure(
                mutate_error("UICANVAS-MUTATE-PARAMS", "params must be a JSON object", "Pass an object payload."));
        }

        if (action == "add") {
            HudWidget widget;
            widget.id = params.value("id", std::string{});
            if (widget.id.empty()) {
                return Result<UiCanvasAsset>::failure(
                    mutate_error("UICANVAS-MUTATE-ID", "add requires id", "Set widget id."));
            }
            if (find_widget(canvas, widget.id)) {
                return Result<UiCanvasAsset>::failure(
                    mutate_error("UICANVAS-MUTATE-DUP", "Widget id already exists: " + widget.id, "Choose a unique id."));
            }
            const auto type = parse_type(params.value("type", std::string{"panel"}));
            if (!type) return Result<UiCanvasAsset>::failure(type.error());
            widget.type = type.value();
            const auto anchor = parse_anchor(params.value("anchor", std::string{"top_left"}));
            if (!anchor) return Result<UiCanvasAsset>::failure(anchor.error());
            widget.anchor = anchor.value();
            if (params.contains("offset") && params["offset"].is_array() && params["offset"].size() >= 2) {
                widget.offset[0] = params["offset"][0].get<float>();
                widget.offset[1] = params["offset"][1].get<float>();
            }
            if (params.contains("size") && params["size"].is_array() && params["size"].size() >= 2) {
                widget.size[0] = params["size"][0].get<float>();
                widget.size[1] = params["size"][1].get<float>();
            } else {
                widget.size = {{160.0f, 40.0f}};
            }
            if (!(widget.size[0] > 0.0f) || !(widget.size[1] > 0.0f)) {
                return Result<UiCanvasAsset>::failure(
                    mutate_error("UICANVAS-MUTATE-SIZE", "size must be positive", "Provide size [w,h] > 0."));
            }
            widget.bind = params.value("bind", std::string{});
            widget.max_bind = params.value("maxBind", std::string{});
            widget.label = params.value("label", std::string{});
            widget.text = params.value("text", std::string{});
            widget.image = params.value("image", std::string{});
            if (params.contains("imageMode")) {
                const auto mode = parse_image_mode(params["imageMode"].get<std::string>());
                if (!mode) return Result<UiCanvasAsset>::failure(mode.error());
                widget.image_mode = mode.value();
            }
            if (params.contains("color") && params["color"].is_array() && params["color"].size() >= 3) {
                widget.color[0] = params["color"][0].get<float>();
                widget.color[1] = params["color"][1].get<float>();
                widget.color[2] = params["color"][2].get<float>();
                widget.color[3] = params["color"].size() >= 4 ? params["color"][3].get<float>() : 255.0f;
            }
            widget.font_size = params.value("fontSize", 0.0f);
            widget.opacity = std::clamp(params.value("opacity", 1.0f), 0.0f, 1.0f);
            widget.visible = params.value("visible", true);
            if (params.contains("enabled")) widget.enabled = params["enabled"].get<bool>();
            else if (params.contains("active")) widget.enabled = params["active"].get<bool>();
            if (params.contains("textAlign")) {
                const auto align = parse_text_align(params["textAlign"].get<std::string>());
                if (!align) return Result<UiCanvasAsset>::failure(align.error());
                widget.text_align = align.value();
            }
            if (params.contains("textVAlign")) {
                const auto valign = parse_text_v_align(params["textVAlign"].get<std::string>());
                if (!valign) return Result<UiCanvasAsset>::failure(valign.error());
                widget.text_v_align = valign.value();
            }
            if (widget.type == HudWidgetType::Bar && widget.bind.empty()) widget.bind = "value";
            if (widget.type == HudWidgetType::Text && widget.bind.empty()) widget.bind = widget.id + ".text";
            if (widget.type == HudWidgetType::Button && widget.bind.empty()) widget.bind = widget.id;
            if (widget.type == HudWidgetType::Toggle && widget.bind.empty()) widget.bind = widget.id;
            if (widget.type == HudWidgetType::Slider && widget.bind.empty()) widget.bind = widget.id;
            canvas.widgets.push_back(std::move(widget));
            return Result<UiCanvasAsset>::success(std::move(canvas));
        }

        const auto id = params.value("id", std::string{});
        if (action != "list" && id.empty()) {
            return Result<UiCanvasAsset>::failure(
                mutate_error("UICANVAS-MUTATE-ID", "id is required for " + action, "Pass widget id."));
        }
        auto* widget = id.empty() ? nullptr : find_widget(canvas, id);
        if (action != "list" && !widget) {
            return Result<UiCanvasAsset>::failure(
                mutate_error("UICANVAS-MUTATE-MISSING", "Widget not found: " + id, "Check canvas widget ids."));
        }

        if (action == "remove") {
            canvas.widgets.erase(std::remove_if(canvas.widgets.begin(), canvas.widgets.end(),
                                    [&](const HudWidget& entry) { return entry.id == id; }),
                canvas.widgets.end());
            return Result<UiCanvasAsset>::success(std::move(canvas));
        }

        if (action == "move") {
            if (params.contains("offset") && params["offset"].is_array() && params["offset"].size() >= 2) {
                widget->offset[0] = params["offset"][0].get<float>();
                widget->offset[1] = params["offset"][1].get<float>();
            } else if (params.contains("delta") && params["delta"].is_array() && params["delta"].size() >= 2) {
                widget->offset[0] += params["delta"][0].get<float>();
                widget->offset[1] += params["delta"][1].get<float>();
            } else {
                return Result<UiCanvasAsset>::failure(mutate_error("UICANVAS-MUTATE-MOVE",
                    "move requires offset [x,y] or delta [dx,dy]", "Provide design-space coordinates."));
            }
            return Result<UiCanvasAsset>::success(std::move(canvas));
        }

        if (action == "resize") {
            if (!params.contains("size") || !params["size"].is_array() || params["size"].size() < 2) {
                return Result<UiCanvasAsset>::failure(
                    mutate_error("UICANVAS-MUTATE-RESIZE", "resize requires size [w,h]", "Provide positive size."));
            }
            widget->size[0] = params["size"][0].get<float>();
            widget->size[1] = params["size"][1].get<float>();
            if (!(widget->size[0] > 0.0f) || !(widget->size[1] > 0.0f)) {
                return Result<UiCanvasAsset>::failure(
                    mutate_error("UICANVAS-MUTATE-SIZE", "size must be positive", "Provide size [w,h] > 0."));
            }
            return Result<UiCanvasAsset>::success(std::move(canvas));
        }

        if (action == "style") {
            if (params.contains("color") && params["color"].is_array() && params["color"].size() >= 3) {
                widget->color[0] = params["color"][0].get<float>();
                widget->color[1] = params["color"][1].get<float>();
                widget->color[2] = params["color"][2].get<float>();
                widget->color[3] = params["color"].size() >= 4 ? params["color"][3].get<float>() : 255.0f;
            }
            if (params.contains("fontSize")) widget->font_size = params["fontSize"].get<float>();
            if (params.contains("label")) widget->label = params["label"].get<std::string>();
            if (params.contains("text")) widget->text = params["text"].get<std::string>();
            if (params.contains("opacity"))
                widget->opacity = std::clamp(params["opacity"].get<float>(), 0.0f, 1.0f);
            if (params.contains("visible")) widget->visible = params["visible"].get<bool>();
            if (params.contains("enabled")) widget->enabled = params["enabled"].get<bool>();
            else if (params.contains("active")) widget->enabled = params["active"].get<bool>();
            if (params.contains("bind")) widget->bind = params["bind"].get<std::string>();
            if (params.contains("maxBind")) widget->max_bind = params["maxBind"].get<std::string>();
            if (params.contains("image")) widget->image = params["image"].get<std::string>();
            if (params.contains("imageMode")) {
                const auto mode = parse_image_mode(params["imageMode"].get<std::string>());
                if (!mode) return Result<UiCanvasAsset>::failure(mode.error());
                widget->image_mode = mode.value();
            }
            if (params.contains("anchor")) {
                const auto anchor = parse_anchor(params["anchor"].get<std::string>());
                if (!anchor) return Result<UiCanvasAsset>::failure(anchor.error());
                widget->anchor = anchor.value();
            }
            if (params.contains("textAlign")) {
                const auto align = parse_text_align(params["textAlign"].get<std::string>());
                if (!align) return Result<UiCanvasAsset>::failure(align.error());
                widget->text_align = align.value();
            }
            if (params.contains("textVAlign")) {
                const auto valign = parse_text_v_align(params["textVAlign"].get<std::string>());
                if (!valign) return Result<UiCanvasAsset>::failure(valign.error());
                widget->text_v_align = valign.value();
            }
            return Result<UiCanvasAsset>::success(std::move(canvas));
        }

        if (action == "list") {
            return Result<UiCanvasAsset>::success(std::move(canvas));
        }

        return Result<UiCanvasAsset>::failure(mutate_error("UICANVAS-MUTATE-ACTION", "Unknown action: " + action,
            "Use add, remove, move, resize, style, or list."));
    } catch (const std::exception& ex) {
        return Result<UiCanvasAsset>::failure(
            mutate_error("UICANVAS-MUTATE-PARSE", "Failed to parse mutate params", std::string(ex.what())));
    }
}

Result<UiCanvasAsset> mutate_ui_canvas_file(const std::filesystem::path& absolute_path, const std::string& action,
    const std::string& params_json) {
    auto loaded = UiCanvasAsset::load(absolute_path);
    if (!loaded) return Result<UiCanvasAsset>::failure(loaded.error());
    auto mutated = mutate_ui_canvas_asset(std::move(loaded.value()), action, params_json);
    if (!mutated) return mutated;
    const auto written = write_ui_canvas_json_atomic(absolute_path, mutated.value().to_json());
    if (!written) return Result<UiCanvasAsset>::failure(written.error());
    return mutated;
}

} // namespace engine
