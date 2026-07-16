#include "engine/ui/hud_runtime.h"

#include "engine/assets/hud_asset.h"
#include "engine/ui/game_fonts.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <sstream>

namespace engine {
namespace {

bool path_looks_like_ui_canvas(const std::filesystem::path& path) {
    const auto name = path.filename().generic_string();
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower.find(".uicanvas.json") != std::string::npos || lower.find(".canvas.json") != std::string::npos;
}

bool source_looks_like_ui_canvas(const std::string& source_name, const std::string& text) {
    std::string lower = source_name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower.find(".uicanvas.json") != std::string::npos || lower.find(".canvas.json") != std::string::npos)
        return true;
    return text.find("designResolution") != std::string::npos;
}

bool is_focusable_type(HudWidgetType type) {
    return type == HudWidgetType::Button || type == HudWidgetType::Toggle || type == HudWidgetType::Slider;
}

ImVec2 anchored_origin(HudAnchor anchor, const ImVec2& content_min, const ImVec2& content_max, float width,
    float height, float offset_x, float offset_y) {
    const float left = content_min.x;
    const float top = content_min.y;
    const float right = content_max.x;
    const float bottom = content_max.y;
    switch (anchor) {
    case HudAnchor::TopLeft:
        return ImVec2{left + offset_x, top + offset_y};
    case HudAnchor::TopRight:
        return ImVec2{right - width - offset_x, top + offset_y};
    case HudAnchor::BottomLeft:
        return ImVec2{left + offset_x, bottom - height - offset_y};
    case HudAnchor::BottomRight:
        return ImVec2{right - width - offset_x, bottom - height - offset_y};
    case HudAnchor::Center:
        return ImVec2{left + (right - left - width) * 0.5f + offset_x, top + (bottom - top - height) * 0.5f + offset_y};
    }
    return ImVec2{left + offset_x, top + offset_y};
}

ImU32 with_opacity(ImU32 color, float opacity) {
    const float clamped = std::clamp(opacity, 0.0f, 1.0f);
    const int alpha = static_cast<int>(std::lround(static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFF) * clamped));
    return (color & ~IM_COL32_A_MASK) | (static_cast<ImU32>(std::clamp(alpha, 0, 255)) << IM_COL32_A_SHIFT);
}

std::string image_stem_label(const std::string& image_path) {
    if (image_path.empty()) return {};
    return std::filesystem::path(image_path).stem().generic_string();
}

} // namespace

void HudRuntime::reset_widget_flags_from_asset() {
    visibility_.clear();
    enabled_.clear();
    for (const auto& widget : asset_.widgets) {
        visibility_[widget.id] = widget.visible;
        enabled_[widget.id] = widget.enabled;
        if (widget.type == HudWidgetType::Text && !widget.bind.empty() && !widget.text.empty())
            texts_[widget.bind] = widget.text;
        if (widget.type == HudWidgetType::Button && !widget.bind.empty() && !widget.text.empty())
            texts_[widget.bind] = widget.text;
        if (widget.type == HudWidgetType::Toggle && !widget.bind.empty() && bools_.find(widget.bind) == bools_.end())
            bools_[widget.bind] = false;
        if (widget.type == HudWidgetType::Slider && !widget.bind.empty() && numbers_.find(widget.bind) == numbers_.end())
            numbers_[widget.bind] = 0.0;
    }
}

Result<void> HudRuntime::load(const std::filesystem::path& path) {
    if (path_looks_like_ui_canvas(path)) {
        auto loaded = UiCanvasAsset::load(path);
        if (!loaded) return Result<void>::failure(loaded.error());
        asset_ = std::move(loaded.value());
    } else {
        auto hud = HudAsset::load(path);
        if (!hud) return Result<void>::failure(hud.error());
        asset_ = UiCanvasAsset::from_hud(hud.value());
    }
    reset_widget_flags_from_asset();
    return Result<void>::success();
}

Result<void> HudRuntime::load_from_json(const std::string& text, const std::string& source_name) {
    if (source_looks_like_ui_canvas(source_name, text)) {
        auto loaded = UiCanvasAsset::parse(text, source_name);
        if (!loaded) return Result<void>::failure(loaded.error());
        asset_ = std::move(loaded.value());
    } else {
        auto loaded = HudAsset::parse(text, source_name);
        if (!loaded) return Result<void>::failure(loaded.error());
        asset_ = UiCanvasAsset::from_hud(loaded.value());
    }
    reset_widget_flags_from_asset();
    return Result<void>::success();
}

void HudRuntime::clear() {
    asset_ = UiCanvasAsset{};
    numbers_.clear();
    bools_.clear();
    texts_.clear();
    visibility_.clear();
    enabled_.clear();
}

void HudRuntime::reset_player_health(double current, double max) {
    set_health(current, max);
}

void HudRuntime::set_number(const std::string& bind, double value) {
    if (bind.empty()) return;
    numbers_[bind] = value;
}

void HudRuntime::set_bool(const std::string& bind, bool value) {
    if (bind.empty()) return;
    bools_[bind] = value;
}

void HudRuntime::set_text(const std::string& bind, std::string value) {
    if (bind.empty()) return;
    texts_[bind] = std::move(value);
}

void HudRuntime::set_visible(const std::string& widget_id, bool visible) {
    if (widget_id.empty()) return;
    visibility_[widget_id] = visible;
}

void HudRuntime::set_enabled(const std::string& widget_id, bool enabled) {
    if (widget_id.empty()) return;
    enabled_[widget_id] = enabled;
}

void HudRuntime::set_health(double current, double max) {
    if (!(max > 0.0) || !std::isfinite(max)) max = 1.0;
    if (!std::isfinite(current)) current = 0.0;
    current = std::clamp(current, 0.0, max);
    set_number("player.health", current);
    set_number("player.healthMax", max);
    std::ostringstream text;
    text << static_cast<int>(std::lround(current)) << " / " << static_cast<int>(std::lround(max));
    set_text("player.healthText", text.str());
}

std::optional<double> HudRuntime::get_number(const std::string& bind) const {
    const auto it = numbers_.find(bind);
    if (it == numbers_.end()) return std::nullopt;
    return it->second;
}

std::optional<bool> HudRuntime::get_bool(const std::string& bind) const {
    const auto it = bools_.find(bind);
    if (it == bools_.end()) return std::nullopt;
    return it->second;
}

std::optional<std::string> HudRuntime::get_text(const std::string& bind) const {
    const auto it = texts_.find(bind);
    if (it == texts_.end()) return std::nullopt;
    return it->second;
}

bool HudRuntime::is_visible(const std::string& widget_id) const {
    const auto it = visibility_.find(widget_id);
    if (it == visibility_.end()) return true;
    return it->second;
}

bool HudRuntime::is_enabled(const std::string& widget_id) const {
    const auto it = enabled_.find(widget_id);
    if (it == enabled_.end()) return true;
    return it->second;
}

void HudRuntime::draw_overlay(ImDrawList* draw_list, const ImVec2& image_min, const ImVec2& image_max,
    const std::optional<std::string>& focused_widget_id) const {
    if (!draw_list || asset_.widgets.empty()) return;

    const auto layout = compute_ui_canvas_layout(image_min.x, image_min.y, image_max.x, image_max.y,
        asset_.design_resolution[0], asset_.design_resolution[1], asset_.scale_mode);
    if (!(layout.scale > 0.0f)) return;

    const ImVec2 content_min{layout.content_min_x, layout.content_min_y};
    const ImVec2 content_max{layout.content_max_x, layout.content_max_y};
    const float scale = layout.scale;

    draw_list->PushClipRect(image_min, image_max, true);

    constexpr ImU32 panel_col_default = IM_COL32(12, 14, 20, 210);
    constexpr ImU32 button_col_default = IM_COL32(36, 44, 58, 240);
    constexpr ImU32 button_border_default = IM_COL32(90, 110, 140, 255);
    constexpr ImU32 focus_ring_col = IM_COL32(255, 210, 80, 255);
    constexpr ImU32 bar_bg = IM_COL32(28, 34, 44, 235);
    constexpr ImU32 bar_fill_default = IM_COL32(170, 52, 52, 245);
    constexpr ImU32 bar_border = IM_COL32(110, 85, 85, 255);
    constexpr ImU32 text_col_default = IM_COL32(250, 252, 255, 255);
    constexpr ImU32 label_col_default = IM_COL32(235, 238, 245, 255);
    constexpr ImU32 text_outline = IM_COL32(0, 0, 0, 220);
    constexpr ImU32 toggle_box = IM_COL32(40, 48, 62, 255);
    constexpr ImU32 toggle_check = IM_COL32(90, 200, 120, 255);
    constexpr ImU32 slider_track = IM_COL32(40, 48, 62, 255);
    constexpr ImU32 slider_fill = IM_COL32(80, 140, 210, 255);
    constexpr ImU32 slider_thumb = IM_COL32(230, 235, 245, 255);
    constexpr ImU32 image_placeholder_fill = IM_COL32(48, 36, 72, 220);
    constexpr ImU32 image_placeholder_border = IM_COL32(180, 120, 220, 255);

    const auto to_col = [](const std::array<float, 4>& rgba, ImU32 fallback) {
        if (!(rgba[3] > 0.0f)) return fallback;
        return IM_COL32(static_cast<int>(std::clamp(rgba[0], 0.0f, 255.0f)),
            static_cast<int>(std::clamp(rgba[1], 0.0f, 255.0f)),
            static_cast<int>(std::clamp(rgba[2], 0.0f, 255.0f)),
            static_cast<int>(std::clamp(rgba[3], 0.0f, 255.0f)));
    };

    ImFont* font = GameFonts::ui() ? GameFonts::ui() : ImGui::GetFont();
    const auto readable_font_size = [scale](float design_px) {
        return std::max(16.0f, design_px * scale);
    };
    const auto draw_text = [&](const ImVec2& pos, float font_size, ImU32 color, const char* text, float design_px) {
        ImFont* use_font = GameFonts::for_design_size(design_px);
        if (!use_font) use_font = font;
        const float outline = std::max(1.0f, font_size * 0.06f);
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                draw_list->AddText(use_font, font_size,
                    ImVec2{pos.x + static_cast<float>(dx) * outline, pos.y + static_cast<float>(dy) * outline},
                    text_outline, text);
            }
        }
        draw_list->AddText(use_font, font_size, pos, color, text);
    };
    const auto aligned_text_pos = [&](const ImVec2& box_min, const ImVec2& box_max, float font_size, float design_px,
                                      HudTextAlign align, HudTextVAlign valign, const char* text) {
        ImFont* use_font = GameFonts::for_design_size(design_px);
        if (!use_font) use_font = font;
        const ImVec2 text_size = use_font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text);
        const float box_w = box_max.x - box_min.x;
        const float box_h = box_max.y - box_min.y;
        float x = box_min.x;
        float y = box_min.y;
        switch (align) {
        case HudTextAlign::Center: x = box_min.x + (box_w - text_size.x) * 0.5f; break;
        case HudTextAlign::Right: x = box_max.x - text_size.x; break;
        case HudTextAlign::Left: break;
        }
        switch (valign) {
        case HudTextVAlign::Middle: y = box_min.y + (box_h - text_size.y) * 0.5f; break;
        case HudTextVAlign::Bottom: y = box_max.y - text_size.y; break;
        case HudTextVAlign::Top: break;
        }
        return ImVec2{x, y};
    };
    const auto draw_image_placeholder = [&](const ImVec2& origin, const ImVec2& max, float rounding, float opacity,
        const std::string& image_path, ImU32 fallback_fill) {
        if (image_path.empty()) {
            draw_list->AddRectFilled(origin, max, with_opacity(fallback_fill, opacity), rounding);
            return;
        }
        draw_list->AddRectFilled(origin, max, with_opacity(image_placeholder_fill, opacity), rounding);
        draw_list->AddRect(origin, max, with_opacity(image_placeholder_border, opacity), rounding, 0,
            std::max(1.5f, scale));
        const std::string stem = image_stem_label(image_path);
        if (!stem.empty()) {
            const float font_sz = readable_font_size(18.0f);
            const ImVec2 text_pos = aligned_text_pos(origin, max, font_sz, 18.0f, HudTextAlign::Center, HudTextVAlign::Middle,
                stem.c_str());
            draw_text(text_pos, font_sz, with_opacity(text_col_default, opacity), stem.c_str(), 18.0f);
        }
    };
    const auto draw_focus_ring = [&](const ImVec2& origin, const ImVec2& max, float rounding) {
        const float pad = 2.0f * scale;
        draw_list->AddRect(ImVec2{origin.x - pad, origin.y - pad}, ImVec2{max.x + pad, max.y + pad}, focus_ring_col,
            rounding + pad, 0, std::max(2.0f, scale * 0.75f));
    };

    for (const auto& widget : asset_.widgets) {
        if (!is_visible(widget.id)) continue;
        const float opacity =
            std::clamp(widget.opacity, 0.0f, 1.0f) * (is_enabled(widget.id) ? 1.0f : 0.45f);
        if (opacity <= 0.001f) continue;

        const float width = widget.size[0] * scale;
        const float height = widget.size[1] * scale;
        const float ox = widget.offset[0] * scale;
        const float oy = widget.offset[1] * scale;
        const ImVec2 origin = anchored_origin(widget.anchor, content_min, content_max, width, height, ox, oy);
        const ImVec2 max{origin.x + width, origin.y + height};
        const float rounding = 3.0f * scale;

        if (widget.type == HudWidgetType::Panel) {
            if (!widget.image.empty()) {
                draw_image_placeholder(origin, max, rounding, opacity, widget.image, panel_col_default);
            } else {
                draw_list->AddRectFilled(origin, max, with_opacity(to_col(widget.color, panel_col_default), opacity),
                    rounding);
            }
            continue;
        }

        if (widget.type == HudWidgetType::Image) {
            draw_image_placeholder(origin, max, rounding, opacity, widget.image, panel_col_default);
            continue;
        }

        if (widget.type == HudWidgetType::Bar) {
            double current = 0.0;
            double max_value = 100.0;
            if (const auto value = get_number(widget.bind)) current = *value;
            if (!widget.max_bind.empty()) {
                if (const auto max_bound = get_number(widget.max_bind)) max_value = *max_bound;
            }
            if (!(max_value > 0.0)) max_value = 1.0;
            const float fill = static_cast<float>(std::clamp(current / max_value, 0.0, 1.0));
            const ImU32 bar_fill = with_opacity(to_col(widget.color, bar_fill_default), opacity);
            draw_list->AddRectFilled(origin, max, with_opacity(bar_bg, opacity), 2.0f * scale);
            if (fill > 0.0f) {
                const ImVec2 fill_max{origin.x + width * fill, max.y};
                draw_list->AddRectFilled(origin, fill_max, bar_fill, 2.0f * scale);
            }
            draw_list->AddRect(origin, max, with_opacity(bar_border, opacity), 2.0f * scale, 0, std::max(1.0f, scale));
            if (!widget.label.empty()) {
                const float design_label = widget.font_size > 0.0f ? widget.font_size : 22.0f;
                const float label_size = readable_font_size(design_label);
                draw_text(ImVec2{origin.x, origin.y - label_size - 2.0f * scale}, label_size,
                    with_opacity(label_col_default, opacity), widget.label.c_str(), design_label);
            }
            continue;
        }

        if (widget.type == HudWidgetType::Text) {
            std::string content;
            if (const auto value = get_text(widget.bind)) content = *value;
            else if (!widget.text.empty()) content = widget.text;
            else if (const auto number = get_number(widget.bind)) {
                std::ostringstream stream;
                stream << *number;
                content = stream.str();
            }
            if (!content.empty()) {
                const float design_px = widget.font_size > 0.0f ? widget.font_size
                                                              : (widget.size[1] > 0.0f ? widget.size[1] : 28.0f);
                const float screen_font = readable_font_size(design_px);
                const ImVec2 text_pos =
                    aligned_text_pos(origin, max, screen_font, design_px, widget.text_align, widget.text_v_align,
                        content.c_str());
                draw_text(text_pos, screen_font, with_opacity(to_col(widget.color, text_col_default), opacity),
                    content.c_str(), design_px);
            }
            continue;
        }

        if (widget.type == HudWidgetType::Button) {
            const bool focused = focused_widget_id && *focused_widget_id == widget.id;
            if (!widget.image.empty()) {
                draw_image_placeholder(origin, max, rounding, opacity, widget.image, button_col_default);
            } else {
                const ImU32 fill = with_opacity(to_col(widget.color, button_col_default), opacity);
                const ImU32 border = with_opacity(button_border_default, opacity);
                draw_list->AddRectFilled(origin, max, fill, rounding);
                draw_list->AddRect(origin, max, border, rounding, 0, std::max(1.0f, scale));
            }
            if (focused) draw_focus_ring(origin, max, rounding);
            const std::string label = widget_display_label(widget);
            if (!label.empty() && widget.image.empty()) {
                const float design_px = widget.font_size > 0.0f ? widget.font_size
                                                              : (widget.size[1] > 0.0f ? widget.size[1] * 0.65f : 24.0f);
                const float screen_font = readable_font_size(design_px);
                const ImVec2 text_pos = aligned_text_pos(origin, max, screen_font, design_px, widget.text_align,
                    widget.text_v_align, label.c_str());
                draw_text(text_pos, screen_font, with_opacity(text_col_default, opacity), label.c_str(), design_px);
            }
            continue;
        }

        if (widget.type == HudWidgetType::Toggle) {
            const bool focused = focused_widget_id && *focused_widget_id == widget.id;
            const bool on = get_bool(widget.bind).value_or(false);
            const float box_size = std::min(height, width * 0.35f);
            const ImVec2 box_min{origin.x, origin.y + (height - box_size) * 0.5f};
            const ImVec2 box_max{box_min.x + box_size, box_min.y + box_size};
            draw_list->AddRectFilled(box_min, box_max, with_opacity(toggle_box, opacity), 2.0f * scale);
            draw_list->AddRect(box_min, box_max, with_opacity(button_border_default, opacity), 2.0f * scale, 0,
                std::max(1.0f, scale));
            if (on) {
                const float inset = box_size * 0.22f;
                draw_list->AddRectFilled(ImVec2{box_min.x + inset, box_min.y + inset},
                    ImVec2{box_max.x - inset, box_max.y - inset}, with_opacity(toggle_check, opacity), 2.0f * scale);
            }
            if (focused) draw_focus_ring(origin, max, rounding);
            const std::string label = widget_display_label(widget);
            if (!label.empty()) {
                const float design_px = widget.font_size > 0.0f ? widget.font_size
                                                              : (widget.size[1] > 0.0f ? widget.size[1] * 0.55f : 22.0f);
                const float screen_font = readable_font_size(design_px);
                const ImVec2 label_min{box_max.x + 8.0f * scale, origin.y};
                const ImVec2 text_pos =
                    aligned_text_pos(label_min, max, screen_font, design_px, HudTextAlign::Left, HudTextVAlign::Middle,
                        label.c_str());
                draw_text(text_pos, screen_font, with_opacity(text_col_default, opacity), label.c_str(), design_px);
            }
            continue;
        }

        if (widget.type == HudWidgetType::Slider) {
            const bool focused = focused_widget_id && *focused_widget_id == widget.id;
            double current = get_number(widget.bind).value_or(0.0);
            double max_value = 1.0;
            if (!widget.max_bind.empty()) {
                if (const auto max_bound = get_number(widget.max_bind)) max_value = *max_bound;
            }
            if (!(max_value > 0.0)) max_value = 1.0;
            const float t = static_cast<float>(std::clamp(current / max_value, 0.0, 1.0));
            const float track_h = std::max(4.0f * scale, height * 0.35f);
            const float track_y = origin.y + (height - track_h) * 0.5f;
            const ImVec2 track_min{origin.x, track_y};
            const ImVec2 track_max{max.x, track_y + track_h};
            draw_list->AddRectFilled(track_min, track_max, with_opacity(slider_track, opacity), track_h * 0.5f);
            if (t > 0.0f) {
                draw_list->AddRectFilled(track_min, ImVec2{track_min.x + width * t, track_max.y},
                    with_opacity(to_col(widget.color, slider_fill), opacity), track_h * 0.5f);
            }
            const float thumb_w = std::max(10.0f * scale, height * 0.55f);
            const float thumb_h = height;
            const float thumb_x = origin.x + width * t - thumb_w * 0.5f;
            const ImVec2 thumb_min{std::clamp(thumb_x, origin.x, max.x - thumb_w), origin.y};
            const ImVec2 thumb_max{thumb_min.x + thumb_w, origin.y + thumb_h};
            draw_list->AddRectFilled(thumb_min, thumb_max, with_opacity(slider_thumb, opacity), 2.0f * scale);
            draw_list->AddRect(thumb_min, thumb_max, with_opacity(button_border_default, opacity), 2.0f * scale, 0,
                std::max(1.0f, scale));
            if (focused) draw_focus_ring(origin, max, rounding);
            if (!widget.label.empty()) {
                const float design_label = widget.font_size > 0.0f ? widget.font_size : 20.0f;
                const float label_size = readable_font_size(design_label);
                draw_text(ImVec2{origin.x, origin.y - label_size - 2.0f * scale}, label_size,
                    with_opacity(label_col_default, opacity), widget.label.c_str(), design_label);
            }
        }
    }

    draw_list->PopClipRect();
}

std::string HudRuntime::widget_display_label(const HudWidget& widget) const {
    if (!widget.label.empty()) return widget.label;
    if (!widget.text.empty()) return widget.text;
    return widget.bind;
}

std::vector<std::string> HudRuntime::focusable_widget_ids() const {
    std::vector<std::string> ids;
    for (const auto& widget : asset_.widgets) {
        if (!is_focusable_type(widget.type)) continue;
        if (!is_visible(widget.id) || !is_enabled(widget.id)) continue;
        ids.push_back(widget.id);
    }
    return ids;
}

std::optional<std::string> HudRuntime::hit_test_widget(const ImVec2& image_min, const ImVec2& image_max,
    const ImVec2& mouse_pos) const {
    const auto layout = compute_ui_canvas_layout(image_min.x, image_min.y, image_max.x, image_max.y,
        asset_.design_resolution[0], asset_.design_resolution[1], asset_.scale_mode);
    if (!(layout.scale > 0.0f)) return std::nullopt;
    if (mouse_pos.x < image_min.x || mouse_pos.x > image_max.x || mouse_pos.y < image_min.y || mouse_pos.y > image_max.y)
        return std::nullopt;
    const ImVec2 content_min{layout.content_min_x, layout.content_min_y};
    const ImVec2 content_max{layout.content_max_x, layout.content_max_y};
    const float scale = layout.scale;
    for (auto it = asset_.widgets.rbegin(); it != asset_.widgets.rend(); ++it) {
        if (!is_focusable_type(it->type)) continue;
        if (!is_visible(it->id) || !is_enabled(it->id)) continue;
        const float width = it->size[0] * scale;
        const float height = it->size[1] * scale;
        const ImVec2 origin = anchored_origin(it->anchor, content_min, content_max, width, height,
            it->offset[0] * scale, it->offset[1] * scale);
        const ImVec2 max{origin.x + width, origin.y + height};
        if (mouse_pos.x >= origin.x && mouse_pos.x <= max.x && mouse_pos.y >= origin.y && mouse_pos.y <= max.y)
            return it->id;
    }
    return std::nullopt;
}

bool HudRuntime::apply_slider_click(const ImVec2& image_min, const ImVec2& image_max, const std::string& widget_id,
    const ImVec2& mouse_pos) {
    const HudWidget* widget = nullptr;
    for (const auto& entry : asset_.widgets) {
        if (entry.id == widget_id) {
            widget = &entry;
            break;
        }
    }
    if (!widget || widget->type != HudWidgetType::Slider || widget->bind.empty()) return false;

    const auto layout = compute_ui_canvas_layout(image_min.x, image_min.y, image_max.x, image_max.y,
        asset_.design_resolution[0], asset_.design_resolution[1], asset_.scale_mode);
    if (!(layout.scale > 0.0f)) return false;
    const float scale = layout.scale;
    const ImVec2 content_min{layout.content_min_x, layout.content_min_y};
    const ImVec2 content_max{layout.content_max_x, layout.content_max_y};
    const float width = widget->size[0] * scale;
    const float height = widget->size[1] * scale;
    const ImVec2 origin = anchored_origin(widget->anchor, content_min, content_max, width, height,
        widget->offset[0] * scale, widget->offset[1] * scale);
    if (width <= 0.0f) return false;
    const float t = std::clamp((mouse_pos.x - origin.x) / width, 0.0f, 1.0f);
    double max_value = 1.0;
    if (!widget->max_bind.empty()) {
        if (const auto max_bound = get_number(widget->max_bind)) max_value = *max_bound;
    }
    if (!(max_value > 0.0)) max_value = 1.0;
    set_number(widget->bind, static_cast<double>(t) * max_value);
    return true;
}

} // namespace engine
