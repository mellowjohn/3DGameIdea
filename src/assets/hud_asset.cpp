#include "engine/assets/hud_asset.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>

namespace engine {
namespace {

EngineError hud_error(std::string code, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Validation, "hud", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

Result<HudWidgetType> parse_widget_type(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "bar") return Result<HudWidgetType>::success(HudWidgetType::Bar);
    if (key == "text") return Result<HudWidgetType>::success(HudWidgetType::Text);
    if (key == "panel") return Result<HudWidgetType>::success(HudWidgetType::Panel);
    if (key == "button") return Result<HudWidgetType>::success(HudWidgetType::Button);
    if (key == "toggle") return Result<HudWidgetType>::success(HudWidgetType::Toggle);
    if (key == "slider") return Result<HudWidgetType>::success(HudWidgetType::Slider);
    if (key == "image") return Result<HudWidgetType>::success(HudWidgetType::Image);
    return Result<HudWidgetType>::failure(hud_error("HUD-WIDGET-TYPE", "Unsupported HUD widget type: " + raw,
        "Use bar, text, panel, button, toggle, slider, or image."));
}

Result<HudAnchor> parse_anchor(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "top_left" || key == "topleft") return Result<HudAnchor>::success(HudAnchor::TopLeft);
    if (key == "top_right" || key == "topright") return Result<HudAnchor>::success(HudAnchor::TopRight);
    if (key == "bottom_left" || key == "bottomleft") return Result<HudAnchor>::success(HudAnchor::BottomLeft);
    if (key == "bottom_right" || key == "bottomright") return Result<HudAnchor>::success(HudAnchor::BottomRight);
    if (key == "center") return Result<HudAnchor>::success(HudAnchor::Center);
    return Result<HudAnchor>::failure(
        hud_error("HUD-ANCHOR", "Unsupported HUD anchor: " + raw, "Use top_left, top_right, bottom_left, bottom_right, or center."));
}

const char* widget_type_name(HudWidgetType type) {
    switch (type) {
    case HudWidgetType::Bar: return "bar";
    case HudWidgetType::Text: return "text";
    case HudWidgetType::Panel: return "panel";
    case HudWidgetType::Button: return "button";
    case HudWidgetType::Toggle: return "toggle";
    case HudWidgetType::Slider: return "slider";
    case HudWidgetType::Image: return "image";
    }
    return "bar";
}

Result<HudImageMode> parse_image_mode(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "stretch") return Result<HudImageMode>::success(HudImageMode::Stretch);
    if (key == "contain") return Result<HudImageMode>::success(HudImageMode::Contain);
    return Result<HudImageMode>::failure(
        hud_error("HUD-IMAGE-MODE", "Unsupported imageMode: " + raw, "Use stretch or contain."));
}

const char* image_mode_name(HudImageMode mode) {
    switch (mode) {
    case HudImageMode::Stretch: return "stretch";
    case HudImageMode::Contain: return "contain";
    }
    return "stretch";
}

const char* anchor_name(HudAnchor anchor) {
    switch (anchor) {
    case HudAnchor::TopLeft: return "top_left";
    case HudAnchor::TopRight: return "top_right";
    case HudAnchor::BottomLeft: return "bottom_left";
    case HudAnchor::BottomRight: return "bottom_right";
    case HudAnchor::Center: return "center";
    }
    return "top_left";
}

Result<HudTextAlign> parse_text_align(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "left") return Result<HudTextAlign>::success(HudTextAlign::Left);
    if (key == "center" || key == "centre") return Result<HudTextAlign>::success(HudTextAlign::Center);
    if (key == "right") return Result<HudTextAlign>::success(HudTextAlign::Right);
    return Result<HudTextAlign>::failure(
        hud_error("HUD-TEXT-ALIGN", "Unsupported textAlign: " + raw, "Use left, center, or right."));
}

Result<HudTextVAlign> parse_text_v_align(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "top") return Result<HudTextVAlign>::success(HudTextVAlign::Top);
    if (key == "middle" || key == "center" || key == "centre") return Result<HudTextVAlign>::success(HudTextVAlign::Middle);
    if (key == "bottom") return Result<HudTextVAlign>::success(HudTextVAlign::Bottom);
    return Result<HudTextVAlign>::failure(
        hud_error("HUD-TEXT-VALIGN", "Unsupported textVAlign: " + raw, "Use top, middle, or bottom."));
}

const char* text_align_name(HudTextAlign align) {
    switch (align) {
    case HudTextAlign::Left: return "left";
    case HudTextAlign::Center: return "center";
    case HudTextAlign::Right: return "right";
    }
    return "left";
}

const char* text_v_align_name(HudTextVAlign align) {
    switch (align) {
    case HudTextVAlign::Top: return "top";
    case HudTextVAlign::Middle: return "middle";
    case HudTextVAlign::Bottom: return "bottom";
    }
    return "top";
}

} // namespace

std::filesystem::path default_player_hud_path(const std::filesystem::path& project_root) {
    // DEC-0025: sample migrated to *.uicanvas.json; keep this name for call-site compatibility.
    return project_root / "assets" / "ui" / "player.uicanvas.json";
}

Result<HudAsset> HudAsset::parse(const std::string& text, const std::string& source_name) {
    try {
        const auto json = nlohmann::json::parse(text);
        if (!json.is_object()) {
            return Result<HudAsset>::failure(
                hud_error("HUD-ROOT", source_name + " must be a JSON object", "Wrap widgets in an object with schemaVersion."));
        }
        HudAsset asset;
        asset.schema_version = json.value("schemaVersion", 1);
        if (asset.schema_version != 1) {
            return Result<HudAsset>::failure(
                hud_error("HUD-SCHEMA", "Unsupported HUD schemaVersion", "Use schemaVersion 1."));
        }
        asset.id = json.value("id", std::string{});
        const auto widgets = json.value("widgets", nlohmann::json::array());
        if (!widgets.is_array()) {
            return Result<HudAsset>::failure(
                hud_error("HUD-WIDGETS", "widgets must be an array", "Provide a widgets array."));
        }
        for (const auto& node : widgets) {
            if (!node.is_object()) {
                return Result<HudAsset>::failure(
                    hud_error("HUD-WIDGET", "Each widget must be an object", "Fix widget entries."));
            }
            HudWidget widget;
            widget.id = node.value("id", std::string{});
            if (widget.id.empty()) {
                return Result<HudAsset>::failure(
                    hud_error("HUD-WIDGET-ID", "Widget id is required", "Set a unique id per widget."));
            }
            const auto type = parse_widget_type(node.value("type", std::string{}));
            if (!type) return Result<HudAsset>::failure(type.error());
            widget.type = type.value();
            const auto anchor = parse_anchor(node.value("anchor", std::string{"top_left"}));
            if (!anchor) return Result<HudAsset>::failure(anchor.error());
            widget.anchor = anchor.value();
            if (node.contains("offset") && node["offset"].is_array() && node["offset"].size() >= 2) {
                widget.offset[0] = node["offset"][0].get<float>();
                widget.offset[1] = node["offset"][1].get<float>();
            }
            if (node.contains("size") && node["size"].is_array() && node["size"].size() >= 2) {
                widget.size[0] = node["size"][0].get<float>();
                widget.size[1] = node["size"][1].get<float>();
            }
            if (widget.size[0] <= 0.0f || widget.size[1] <= 0.0f) {
                return Result<HudAsset>::failure(
                    hud_error("HUD-SIZE", "Widget size must be positive: " + widget.id, "Set size [width, height] > 0."));
            }
            widget.bind = node.value("bind", std::string{});
            widget.max_bind = node.value("maxBind", std::string{});
            widget.label = node.value("label", std::string{});
            widget.text = node.value("text", std::string{});
            widget.image = node.value("image", std::string{});
            if (node.contains("imageMode")) {
                const auto mode = parse_image_mode(node["imageMode"].get<std::string>());
                if (!mode) return Result<HudAsset>::failure(mode.error());
                widget.image_mode = mode.value();
            }
            if (node.contains("color") && node["color"].is_array() && node["color"].size() >= 3) {
                widget.color[0] = node["color"][0].get<float>();
                widget.color[1] = node["color"][1].get<float>();
                widget.color[2] = node["color"][2].get<float>();
                widget.color[3] = node["color"].size() >= 4 ? node["color"][3].get<float>() : 255.0f;
            }
            widget.font_size = node.value("fontSize", 0.0f);
            if (widget.font_size < 0.0f) {
                return Result<HudAsset>::failure(
                    hud_error("HUD-FONT", "fontSize must be >= 0: " + widget.id, "Omit fontSize or set a positive size."));
            }
            widget.opacity = node.value("opacity", 1.0f);
            if (!(widget.opacity >= 0.0f) || !(widget.opacity <= 1.0f) || !std::isfinite(widget.opacity)) {
                return Result<HudAsset>::failure(
                    hud_error("HUD-OPACITY", "opacity must be in [0, 1]: " + widget.id, "Set opacity between 0 and 1."));
            }
            widget.visible = node.value("visible", true);
            // `active` accepted as an alias for enabled (inactive = !enabled).
            if (node.contains("enabled")) widget.enabled = node["enabled"].get<bool>();
            else if (node.contains("active")) widget.enabled = node["active"].get<bool>();
            else widget.enabled = true;
            if (node.contains("textAlign")) {
                const auto align = parse_text_align(node["textAlign"].get<std::string>());
                if (!align) return Result<HudAsset>::failure(align.error());
                widget.text_align = align.value();
            }
            if (node.contains("textVAlign")) {
                const auto valign = parse_text_v_align(node["textVAlign"].get<std::string>());
                if (!valign) return Result<HudAsset>::failure(valign.error());
                widget.text_v_align = valign.value();
            }
            if (widget.type == HudWidgetType::Bar && widget.bind.empty()) {
                return Result<HudAsset>::failure(
                    hud_error("HUD-BAR-BIND", "Bar widget requires bind: " + widget.id, "Set bind to a number key."));
            }
            if (widget.type == HudWidgetType::Text && widget.bind.empty()) {
                return Result<HudAsset>::failure(
                    hud_error("HUD-TEXT-BIND", "Text widget requires bind: " + widget.id, "Set bind to a text key."));
            }
            if (widget.type == HudWidgetType::Button && widget.bind.empty()) {
                return Result<HudAsset>::failure(
                    hud_error("HUD-BUTTON-BIND", "Button widget requires bind: " + widget.id,
                        "Set bind to a uiButtons binding id."));
            }
            if (widget.type == HudWidgetType::Toggle && widget.bind.empty()) {
                return Result<HudAsset>::failure(
                    hud_error("HUD-TOGGLE-BIND", "Toggle widget requires bind: " + widget.id,
                        "Set bind to a bool key."));
            }
            if (widget.type == HudWidgetType::Slider && widget.bind.empty()) {
                return Result<HudAsset>::failure(
                    hud_error("HUD-SLIDER-BIND", "Slider widget requires bind: " + widget.id,
                        "Set bind to a number key."));
            }
            asset.widgets.push_back(std::move(widget));
        }
        return Result<HudAsset>::success(std::move(asset));
    } catch (const std::exception& ex) {
        return Result<HudAsset>::failure(hud_error("HUD-PARSE", "Failed to parse " + source_name, std::string(ex.what())));
    }
}

Result<HudAsset> HudAsset::load(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return Result<HudAsset>::failure(
            hud_error("HUD-IO", "HUD file not found: " + path.generic_string(), "Create assets/ui/*.hud.json."));
    }
    std::ifstream input(path);
    if (!input) {
        return Result<HudAsset>::failure(
            hud_error("HUD-IO", "Failed to read HUD file: " + path.generic_string(), "Check permissions."));
    }
    const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    return parse(text, path.generic_string());
}

std::string HudAsset::to_json() const {
    nlohmann::json json;
    json["schemaVersion"] = schema_version;
    json["id"] = id;
    nlohmann::json widgets_json = nlohmann::json::array();
    for (const auto& widget : this->widgets) {
        nlohmann::json node;
        node["id"] = widget.id;
        node["type"] = widget_type_name(widget.type);
        node["anchor"] = anchor_name(widget.anchor);
        node["offset"] = {widget.offset[0], widget.offset[1]};
        node["size"] = {widget.size[0], widget.size[1]};
        if (!widget.bind.empty()) node["bind"] = widget.bind;
        if (!widget.max_bind.empty()) node["maxBind"] = widget.max_bind;
        if (!widget.label.empty()) node["label"] = widget.label;
        if (!widget.text.empty()) node["text"] = widget.text;
        if (!widget.image.empty()) node["image"] = widget.image;
        if (widget.image_mode != HudImageMode::Stretch) node["imageMode"] = image_mode_name(widget.image_mode);
        if (widget.has_color())
            node["color"] = {widget.color[0], widget.color[1], widget.color[2], widget.color[3]};
        if (widget.font_size > 0.0f) node["fontSize"] = widget.font_size;
        if (widget.opacity < 1.0f) node["opacity"] = widget.opacity;
        if (!widget.visible) node["visible"] = false;
        if (!widget.enabled) node["enabled"] = false;
        if (widget.text_align != HudTextAlign::Left) node["textAlign"] = text_align_name(widget.text_align);
        if (widget.text_v_align != HudTextVAlign::Top) node["textVAlign"] = text_v_align_name(widget.text_v_align);
        widgets_json.push_back(std::move(node));
    }
    json["widgets"] = std::move(widgets_json);
    return json.dump(2);
}

Result<void> HudAsset::save_atomic(const std::filesystem::path& path) const {
    const auto written = write_hud_json_atomic(path, to_json());
    if (!written) return Result<void>::failure(written.error());
    return Result<void>::success();
}

Result<std::string> write_hud_json_atomic(const std::filesystem::path& absolute_path, const std::string& source) {
    const auto validated = HudAsset::parse(source, absolute_path.generic_string());
    if (!validated) return Result<std::string>::failure(validated.error());
    const auto parent = absolute_path.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    const auto temp = absolute_path.string() + ".tmp";
    const auto backup = absolute_path.string() + ".bak";
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out) {
            return Result<std::string>::failure(
                hud_error("HUD-IO", "Failed to open temp HUD file", "Check permissions."));
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
