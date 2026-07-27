#include "engine/ui/hud_runtime.h"

#include "engine/assets/hud_asset.h"
#include "engine/ui/game_fonts.h"
#include "engine/ui/ui_texture_cache.h"

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

float ease_out_cubic(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const float u = 1.0f - t;
    return 1.0f - u * u * u;
}

float approach(float current, float target, float dt, float duration_seconds) {
    const float dur = std::max(0.01f, duration_seconds);
    const float step = std::clamp(dt / dur, 0.0f, 1.0f);
    return current + (target - current) * step;
}

ImU32 brighten_fill(ImU32 fill, float amount) {
    amount = std::clamp(amount, 0.0f, 1.0f);
    const int r = static_cast<int>((fill >> IM_COL32_R_SHIFT) & 0xFF);
    const int g = static_cast<int>((fill >> IM_COL32_G_SHIFT) & 0xFF);
    const int b = static_cast<int>((fill >> IM_COL32_B_SHIFT) & 0xFF);
    const int a = static_cast<int>((fill >> IM_COL32_A_SHIFT) & 0xFF);
    const int nr = std::min(255, r + static_cast<int>(48.0f * amount));
    const int ng = std::min(255, g + static_cast<int>(42.0f * amount));
    const int nb = std::min(255, b + static_cast<int>(36.0f * amount));
    return IM_COL32(nr, ng, nb, a);
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
    color_overrides_.clear();
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

void HudRuntime::set_resource(double current, double max) {
    if (!(max > 0.0) || !std::isfinite(max)) max = 1.0;
    if (!std::isfinite(current)) current = 0.0;
    current = std::clamp(current, 0.0, max);
    set_number("player.resource", current);
    set_number("player.resourceMax", max);
    std::ostringstream text;
    text << static_cast<int>(std::lround(current)) << "/" << static_cast<int>(std::lround(max));
    set_text("player.resourceText", text.str());
}

void HudRuntime::apply_archetype_hud(const std::string& archetype_id) {
    std::string id = archetype_id;
    std::transform(id.begin(), id.end(), id.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const bool magic = id == "runecaster" || id.find("magic") != std::string::npos;
    const char* kind = magic ? "magic" : "stamina";
    const char* label = magic ? "Magic" : "Stamina";
    set_text("player.archetypeId", archetype_id.empty() ? "ashfell_blade" : archetype_id);
    set_text("player.resourceKind", kind);
    set_text("player.resourceLabel", label);
    set_resource(100.0, 100.0);
    set_visible("player_resource", true);
    set_visible("player_resource_text", true);
    set_visible("player_resource_label", true);
    if (magic) {
        set_color("player_resource", 70.0f, 90.0f, 160.0f, 255.0f);
    } else {
        // Gold stamina rail — matches combat HUD chrome (player-hud.pen)
        set_color("player_resource", 196.0f, 162.0f, 74.0f, 255.0f);
    }
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
    typed_full_.erase(bind);
    typed_chars_.erase(bind);
    typed_cps_.erase(bind);
    if (bind == "quest.objectiveText") {
        const bool show = !texts_[bind].empty();
        set_visible("quest_objective_panel", show);
        set_visible("quest_objective_eyebrow", show);
        set_visible("quest_objective_text", show);
    }
}

void HudRuntime::set_text_typed(const std::string& bind, std::string value, float chars_per_second) {
    if (bind.empty()) return;
    const float cps = std::max(8.0f, chars_per_second);
    if (const auto it = typed_full_.find(bind); it != typed_full_.end() && it->second == value) return;
    typed_full_[bind] = value;
    typed_chars_[bind] = 0.0f;
    typed_cps_[bind] = cps;
    texts_[bind].clear();
    if (value.empty()) {
        typed_full_.erase(bind);
        typed_chars_.erase(bind);
        typed_cps_.erase(bind);
    }
}

void HudRuntime::tick_typewriter(float delta_seconds) {
    if (!(delta_seconds > 0.0f)) return;
    for (auto& [bind, full] : typed_full_) {
        auto& revealed = typed_chars_[bind];
        const float cps = typed_cps_.count(bind) ? typed_cps_[bind] : 48.0f;
        revealed = std::min(static_cast<float>(full.size()), revealed + cps * delta_seconds);
        const auto count = static_cast<std::size_t>(revealed);
        texts_[bind] = full.substr(0, count);
    }
}

bool HudRuntime::typewriter_complete(const std::string& bind) const {
    const auto full = typed_full_.find(bind);
    if (full == typed_full_.end()) return true;
    const auto chars = typed_chars_.find(bind);
    if (chars == typed_chars_.end()) return false;
    return chars->second + 0.001f >= static_cast<float>(full->second.size());
}

bool HudRuntime::skip_typewriter(const std::string& bind) {
    const auto full = typed_full_.find(bind);
    if (full == typed_full_.end()) return false;
    if (typewriter_complete(bind)) return false;
    typed_chars_[bind] = static_cast<float>(full->second.size());
    texts_[bind] = full->second;
    return true;
}

void HudRuntime::apply_text_scroll(const std::string& widget_id, float wheel_delta) {
    if (widget_id.empty() || !(wheel_delta != 0.0f)) return;
    text_scroll_y_[widget_id] = text_scroll_y_[widget_id] - wheel_delta * 28.0f;
}

void HudRuntime::set_visible(const std::string& widget_id, bool visible) {
    if (widget_id.empty()) return;
    visibility_[widget_id] = visible;
}

void HudRuntime::set_enabled(const std::string& widget_id, bool enabled) {
    if (widget_id.empty()) return;
    enabled_[widget_id] = enabled;
}

void HudRuntime::set_color(const std::string& widget_id, float r, float g, float b, float a) {
    if (widget_id.empty()) return;
    color_overrides_[widget_id] = {r, g, b, a};
}

void HudRuntime::clear_color(const std::string& widget_id) {
    if (widget_id.empty()) return;
    color_overrides_.erase(widget_id);
}

void HudRuntime::set_health(double current, double max) {
    if (!(max > 0.0) || !std::isfinite(max)) max = 1.0;
    if (!std::isfinite(current)) current = 0.0;
    current = std::clamp(current, 0.0, max);
    set_number("player.health", current);
    set_number("player.healthMax", max);
    std::ostringstream text;
    text << static_cast<int>(std::lround(current)) << "/" << static_cast<int>(std::lround(max));
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

    // Defaults align with `context/design/rpg-engine-ui.pen` (parchment / bronze / gold).
    constexpr ImU32 panel_col_default = IM_COL32(201, 184, 150, 235); // $canvas
    constexpr ImU32 button_col_default = IM_COL32(213, 185, 120, 245); // $gold
    constexpr ImU32 button_border_default = IM_COL32(102, 86, 60, 255); // #66563C
    constexpr ImU32 focus_ring_col = IM_COL32(213, 185, 120, 255);
    constexpr ImU32 bar_bg = IM_COL32(30, 28, 24, 235); // dark iron track (combat HUD)
    constexpr ImU32 bar_fill_default = IM_COL32(139, 58, 58, 245);
    constexpr ImU32 bar_border = IM_COL32(213, 185, 120, 220); // muted gold
    constexpr ImU32 text_col_default = IM_COL32(42, 36, 32, 255); // $ink on parchment panels
    constexpr ImU32 label_col_default = IM_COL32(42, 36, 32, 255);
    constexpr ImU32 chrome_text_col = IM_COL32(241, 238, 232, 255); // $text on dark chrome / gold buttons
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
    const auto widget_rgba = [&](const HudWidget& widget) -> std::array<float, 4> {
        const auto it = color_overrides_.find(widget.id);
        if (it != color_overrides_.end()) return it->second;
        return widget.color;
    };

    ImFont* font = GameFonts::ui() ? GameFonts::ui() : ImGui::GetFont();
    const auto readable_font_size = [scale](float design_px) {
        // Prefer near 1:1 with atlas size to avoid soft upscaling of Cinzel.
        return std::max(14.0f, design_px * scale);
    };
    const auto draw_text = [&](const ImVec2& pos, float font_size, ImU32 color, const char* text, float design_px,
                                float wrap_width = 0.0f) {
        ImFont* use_font = GameFonts::for_design_size(design_px);
        if (!use_font) use_font = font;
        // Thin outline only — heavy outlines + scaled glyphs look fuzzy on parchment panels.
        const float outline = std::max(0.75f, font_size * 0.035f);
        const ImU32 outline_col = IM_COL32(20, 16, 12, 160);
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                if (dx != 0 && dy != 0) continue; // 4-dir, less smear than 8-dir
                draw_list->AddText(use_font, font_size,
                    ImVec2{pos.x + static_cast<float>(dx) * outline, pos.y + static_cast<float>(dy) * outline},
                    outline_col, text, nullptr, wrap_width);
            }
        }
        draw_list->AddText(use_font, font_size, pos, color, text, nullptr, wrap_width);
    };
    const auto aligned_text_pos = [&](const ImVec2& box_min, const ImVec2& box_max, float font_size, float design_px,
                                      HudTextAlign align, HudTextVAlign valign, const char* text,
                                      float wrap_width = 0.0f) {
        ImFont* use_font = GameFonts::for_design_size(design_px);
        if (!use_font) use_font = font;
        const ImVec2 text_size = use_font->CalcTextSizeA(font_size, FLT_MAX, wrap_width, text);
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
    const auto draw_widget_image = [&](const ImVec2& origin, const ImVec2& max, float rounding, float opacity,
        const std::string& image_path, HudImageMode image_mode, ImU32 fallback_fill) -> bool {
        if (image_path.empty()) {
            draw_list->AddRectFilled(origin, max, with_opacity(fallback_fill, opacity), rounding);
            return true;
        }
        if (textures_) {
            if (const auto tex = textures_->get_or_load(image_path)) {
                float draw_min_x = origin.x, draw_min_y = origin.y, draw_max_x = max.x, draw_max_y = max.y;
                hud_image_fit_rect(origin.x, origin.y, max.x, max.y, static_cast<float>(tex->width),
                    static_cast<float>(tex->height), image_mode, draw_min_x, draw_min_y, draw_max_x, draw_max_y);
                const int alpha = static_cast<int>(std::lround(std::clamp(opacity, 0.0f, 1.0f) * 255.0f));
                const ImU32 tint = IM_COL32(255, 255, 255, alpha);
                draw_list->AddImageRounded(static_cast<ImTextureID>(tex->imgui_tex_id), ImVec2{draw_min_x, draw_min_y},
                    ImVec2{draw_max_x, draw_max_y}, ImVec2{0.0f, 0.0f}, ImVec2{1.0f, 1.0f}, tint, rounding);
                return true;
            }
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
        return false;
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
            const bool circle_panel = widget.id == "hud_face_outer" || widget.id == "hud_face_mid" ||
                widget.id == "hud_face_viewport" || widget.id == "hud_minimap_outer" ||
                widget.id == "hud_minimap_feed" || widget.id == "hud_minimap_dot" ||
                widget.id == "dialogue_portrait_outer" || widget.id == "dialogue_portrait_mid" ||
                widget.id == "dialogue_portrait_viewport";
            if (!widget.image.empty()) {
                draw_widget_image(origin, max, rounding, opacity, widget.image, widget.image_mode, panel_col_default);
            } else if (circle_panel) {
                const ImVec2 center{(origin.x + max.x) * 0.5f, (origin.y + max.y) * 0.5f};
                const float radius = std::min(width, height) * 0.5f;
                const ImU32 fill = with_opacity(to_col(widget_rgba(widget), panel_col_default), opacity);
                draw_list->AddCircleFilled(center, radius, fill, 48);
                if (widget.id != "hud_minimap_dot" && widget.id != "hud_face_viewport" &&
                    widget.id != "dialogue_portrait_viewport") {
                    draw_list->AddCircle(center, radius, with_opacity(bar_border, opacity), 48,
                        std::max(1.5f, scale * 1.25f));
                }
            } else {
                draw_list->AddRectFilled(origin, max, with_opacity(to_col(widget_rgba(widget), panel_col_default), opacity),
                    rounding);
                const bool hotbar_slot =
                    widget.id.rfind("hud_hotbar_", 0) == 0 && widget.id.find("_key") == std::string::npos;
                const bool dialogue_key =
                    widget.id.rfind("dialogue_choice_", 0) == 0 && widget.id.size() > 5 &&
                    widget.id.compare(widget.id.size() - 4, 4, "_key") == 0;
                if (hotbar_slot || dialogue_key) {
                    draw_list->AddRect(origin, max, with_opacity(bar_border, opacity), rounding, 0,
                        std::max(1.0f, scale));
                }
            }
            continue;
        }

        if (widget.type == HudWidgetType::Image) {
            draw_widget_image(origin, max, rounding, opacity, widget.image, widget.image_mode, panel_col_default);
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
            const ImU32 bar_fill = with_opacity(to_col(widget_rgba(widget), bar_fill_default), opacity);
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
                const bool dialogue_body = widget.bind == "dialogue.body";
                const bool dialogue_speaker = widget.bind == "dialogue.speaker";
                const bool hud_vital_text = widget.bind == "player.name" ||
                    widget.bind == "hud.healthLabel" || widget.bind == "player.healthText" ||
                    widget.bind == "player.resourceLabel" || widget.bind == "player.resourceText" ||
                    widget.bind == "quest.objectiveText";
                const bool hud_value_text = widget.bind == "player.healthText" ||
                    widget.bind == "player.resourceText";
                const bool hotbar_key = widget.id.rfind("hud_hotbar_", 0) == 0 &&
                    widget.id.size() > 4 && widget.id.compare(widget.id.size() - 4, 4, "_key") == 0;
                const bool dialogue_chip = widget.bind.rfind("dialogue.choice_", 0) == 0 &&
                    (widget.bind.find("_tone") != std::string::npos ||
                        widget.bind.find("_standing") != std::string::npos ||
                        widget.bind.find("_key") != std::string::npos);
                float screen_font = readable_font_size(design_px);
                // A letterboxed editor Game viewport can shrink authored copy below a usable size.
                // Preserve its reading hierarchy without globally enlarging menus and tool UI.
                if (dialogue_body) screen_font = std::max(screen_font, 18.0f);
                else if (dialogue_speaker) screen_font = std::max(screen_font, 17.0f);
                else if (hud_vital_text) screen_font = std::max(screen_font, 15.0f);
                else if (hotbar_key) screen_font = std::max(screen_font, 16.0f);
                ImFont* use_font = dialogue_body
                    ? (GameFonts::body() ? GameFonts::body() : font)
                    : (GameFonts::ui() ? GameFonts::ui() : font);
                if (widget.bind == "dialogue.speaker" && GameFonts::display()) use_font = GameFonts::display();
                if (widget.bind == "player.name" && GameFonts::display()) use_font = GameFonts::display();
                const float scroll_gutter = dialogue_body ? 18.0f * scale : 0.0f;
                // Numeric vital values must stay on one line: wrapping a final digit is
                // visually indistinguishable from clipping in a compact HUD.
                const float wrap_w = hud_value_text ? 0.0f : std::max(8.0f, width - scroll_gutter);
                const ImVec2 text_size = use_font->CalcTextSizeA(screen_font, FLT_MAX, wrap_w, content.c_str());
                float& scroll = text_scroll_y_[widget.id];
                const float max_scroll = std::max(0.0f, text_size.y - height);
                const ImVec2 hit_max{max.x - scroll_gutter, max.y};
                if (ImGui::IsMouseHoveringRect(origin, hit_max) ||
                    (scroll_gutter > 0.0f && ImGui::IsMouseHoveringRect(ImVec2{max.x - scroll_gutter, origin.y}, max))) {
                    scroll -= ImGui::GetIO().MouseWheel * 28.0f;
                }
                if (scroll_gutter > 0.0f && max_scroll > 0.0f && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
                    ImGui::IsMouseHoveringRect(ImVec2{max.x - scroll_gutter, origin.y}, max)) {
                    const float track_h = height;
                    const float rel = std::clamp((ImGui::GetIO().MousePos.y - origin.y) / std::max(1.0f, track_h), 0.0f, 1.0f);
                    scroll = rel * max_scroll;
                }
                scroll = std::clamp(scroll, 0.0f, max_scroll);
                // Inset right-aligned HUD values so glyph AA / outline isn't clipped by the widget edge.
                const float align_pad = (!dialogue_body && widget.text_align == HudTextAlign::Right)
                    ? std::max(2.0f, screen_font * 0.12f)
                    : 0.0f;
                const ImVec2 align_max{hit_max.x - align_pad, hit_max.y};
                ImVec2 text_pos =
                    aligned_text_pos(origin, align_max, screen_font, design_px, widget.text_align, widget.text_v_align,
                        content.c_str(), wrap_w);
                if (widget.text_v_align == HudTextVAlign::Top) text_pos.y = origin.y - scroll;
                const float clip_pad = std::max(2.0f, screen_font * 0.08f);
                draw_list->PushClipRect(ImVec2{origin.x - clip_pad, origin.y - clip_pad},
                    ImVec2{hit_max.x + clip_pad, hit_max.y + clip_pad}, true);
                const ImU32 authored = to_col(widget_rgba(widget), text_col_default);
                const ImU32 text_col = dialogue_body
                    ? with_opacity(IM_COL32(72, 62, 48, 255), opacity) // softer ink — less muddy than near-black
                    : with_opacity(authored, opacity);
                // Body: light 4-dir outline. Chips/labels: none (outline made chips unreadable).
                if (dialogue_body) {
                    const float outline = std::max(0.6f, screen_font * 0.03f);
                    const ImU32 outline_col = IM_COL32(201, 184, 150, 180);
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            if (dx == 0 && dy == 0) continue;
                            if (dx != 0 && dy != 0) continue;
                            draw_list->AddText(use_font, screen_font,
                                ImVec2{text_pos.x + static_cast<float>(dx) * outline,
                                    text_pos.y + static_cast<float>(dy) * outline},
                                outline_col, content.c_str(), nullptr, wrap_w);
                        }
                    }
                } else if (!dialogue_chip) {
                    const float outline = std::max(0.5f, screen_font * 0.025f);
                    const ImU32 outline_col = IM_COL32(201, 184, 150, 120);
                    for (int axis = 0; axis < 4; ++axis) {
                        const float dx = (axis == 0) ? -outline : (axis == 1) ? outline : 0.0f;
                        const float dy = (axis == 2) ? -outline : (axis == 3) ? outline : 0.0f;
                        draw_list->AddText(use_font, screen_font, ImVec2{text_pos.x + dx, text_pos.y + dy}, outline_col,
                            content.c_str(), nullptr, wrap_w);
                    }
                }
                draw_list->AddText(use_font, screen_font, text_pos, text_col, content.c_str(), nullptr, wrap_w);
                draw_list->PopClipRect();

                if (scroll_gutter > 0.0f && max_scroll > 0.001f) {
                    const float track_x0 = max.x - scroll_gutter + 4.0f * scale;
                    const float track_x1 = max.x - 2.0f * scale;
                    draw_list->AddRectFilled(ImVec2{track_x0, origin.y}, ImVec2{track_x1, max.y},
                        with_opacity(IM_COL32(90, 78, 62, 90), opacity), 3.0f * scale);
                    const float thumb_h = std::max(24.0f * scale, height * (height / (height + max_scroll)));
                    const float thumb_t = (max_scroll > 0.0f) ? (scroll / max_scroll) : 0.0f;
                    const float thumb_y0 = origin.y + (height - thumb_h) * thumb_t;
                    draw_list->AddRectFilled(ImVec2{track_x0, thumb_y0}, ImVec2{track_x1, thumb_y0 + thumb_h},
                        with_opacity(IM_COL32(60, 48, 36, 220), opacity), 3.0f * scale);
                }
            }
            continue;
        }

        if (widget.type == HudWidgetType::Button) {
            const bool focused = focused_widget_id && *focused_widget_id == widget.id;
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const bool hovered = is_enabled(widget.id) && mouse.x >= origin.x && mouse.x <= max.x && mouse.y >= origin.y &&
                mouse.y <= max.y;
            float& hover_t = button_hover_t_[widget.id];
            hover_t = approach(hover_t, hovered ? 1.0f : 0.0f, ImGui::GetIO().DeltaTime, 0.12f);
            const float hover_e = ease_out_cubic(hover_t);
            const float grow = 2.0f * scale * hover_e;
            const ImVec2 draw_min{origin.x - grow, origin.y - grow};
            const ImVec2 draw_max{max.x + grow, max.y + grow};
            if (!widget.image.empty()) {
                draw_widget_image(draw_min, draw_max, rounding, opacity, widget.image, widget.image_mode,
                    button_col_default);
            } else {
                const ImU32 base_fill = to_col(widget_rgba(widget), button_col_default);
                const ImU32 fill = with_opacity(brighten_fill(base_fill, hover_e), opacity);
                const ImU32 border = with_opacity(button_border_default, opacity);
                draw_list->AddRectFilled(draw_min, draw_max, fill, rounding);
                draw_list->AddRect(draw_min, draw_max, border, rounding, 0, std::max(1.0f, scale));
            }
            if (focused) draw_focus_ring(draw_min, draw_max, rounding);
            const std::string label = widget_display_label(widget);
            if (!label.empty()) {
                const float design_px = widget.font_size > 0.0f ? widget.font_size
                                                              : (widget.size[1] > 0.0f ? widget.size[1] * 0.65f : 24.0f);
                const bool dialogue_choice = widget.bind.rfind("dialogue.choice_", 0) == 0 &&
                    widget.bind.find('_', std::string("dialogue.choice_").size()) == std::string::npos;
                float screen_font = readable_font_size(design_px);
                if (dialogue_choice) screen_font = std::max(screen_font, 16.0f);
                const float pad = 16.0f * scale;
                // Keycap on the left; tone/standing tags reserved on the right with chip gap + edge margin.
                const float left_pad = dialogue_choice ? (12.0f + 32.0f + 12.0f) * scale : pad;
                const float right_pad = dialogue_choice ? (16.0f + 168.0f + 12.0f + 100.0f) * scale : pad;
                const float wrap_w = std::max(8.0f, (draw_max.x - draw_min.x) - left_pad - right_pad);
                ImFont* btn_font = dialogue_choice && GameFonts::ui() ? GameFonts::ui()
                    : (GameFonts::for_design_size(design_px) ? GameFonts::for_design_size(design_px) : font);
                const ImVec2 text_size = btn_font->CalcTextSizeA(screen_font, FLT_MAX, wrap_w, label.c_str());
                (void)text_size;
                const ImVec2 text_pos = aligned_text_pos(ImVec2{draw_min.x + left_pad, draw_min.y},
                    ImVec2{draw_max.x - right_pad, draw_max.y}, screen_font, design_px, widget.text_align,
                    dialogue_choice ? HudTextVAlign::Middle : widget.text_v_align, label.c_str(), wrap_w);
                const ImU32 fill = to_col(widget_rgba(widget), button_col_default);
                const int lum = static_cast<int>((fill >> IM_COL32_R_SHIFT) & 0xFF) +
                    static_cast<int>((fill >> IM_COL32_G_SHIFT) & 0xFF) +
                    static_cast<int>((fill >> IM_COL32_B_SHIFT) & 0xFF);
                // Softer ink on choice rows — near-black + outline looked bold/muddy.
                // Image buttons use ink on light plates (authored gold/parchment chrome).
                const ImU32 btn_text = dialogue_choice ? IM_COL32(74, 64, 50, 255)
                    : (!widget.image.empty() || lum > 420 ? text_col_default : chrome_text_col);
                draw_list->AddText(btn_font, screen_font, text_pos, with_opacity(btn_text, opacity), label.c_str(),
                    nullptr, wrap_w);
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
                draw_text(text_pos, screen_font,
                    with_opacity(to_col(widget_rgba(widget), chrome_text_col), opacity), label.c_str(), design_px);
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
                    with_opacity(to_col(widget_rgba(widget), slider_fill), opacity), track_h * 0.5f);
            }
            const float thumb_w = std::max(10.0f * scale, height * 0.55f);
            const float thumb_h = height;
            const float thumb_x = origin.x + width * t - thumb_w * 0.5f;
            const float thumb_hi = std::max(origin.x, max.x - thumb_w);
            const ImVec2 thumb_min{std::clamp(thumb_x, origin.x, thumb_hi), origin.y};
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
    // Runtime set_text(bind) wins so lobby Ready labels can refresh without mutating asset.
    if (!widget.bind.empty()) {
        if (const auto bound = get_text(widget.bind)) return *bound;
    }
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
