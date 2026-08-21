#include "engine/ui/world_ui_billboard.h"

#include "engine/ui/game_fonts.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>

namespace engine {
namespace {

constexpr const char* kInteractBillboardId = "interact_prompt";

ImU32 to_col(const std::array<float, 4>& rgba) {
    return IM_COL32(static_cast<int>(std::lround(std::clamp(rgba[0], 0.0f, 255.0f))),
        static_cast<int>(std::lround(std::clamp(rgba[1], 0.0f, 255.0f))),
        static_cast<int>(std::lround(std::clamp(rgba[2], 0.0f, 255.0f))),
        static_cast<int>(std::lround(std::clamp(rgba[3], 0.0f, 255.0f))));
}

ImU32 lerp_col(ImU32 a, ImU32 b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const auto channel = [t](ImU32 x, ImU32 y, int shift) {
        const int ca = static_cast<int>((x >> shift) & 0xFFu);
        const int cb = static_cast<int>((y >> shift) & 0xFFu);
        return static_cast<int>(std::lround(static_cast<float>(ca) + (static_cast<float>(cb - ca) * t)));
    };
    return IM_COL32(channel(a, b, IM_COL32_R_SHIFT), channel(a, b, IM_COL32_G_SHIFT),
        channel(a, b, IM_COL32_B_SHIFT), channel(a, b, IM_COL32_A_SHIFT));
}

float approach(float current, float target, float dt, float duration_seconds) {
    const float dur = std::max(0.01f, duration_seconds);
    const float step = std::clamp(dt / dur, 0.0f, 1.0f);
    return current + (target - current) * step;
}

void apply_hit_juice(WorldUiBillboard& billboard, double prior_fill) {
    billboard.bar_ghost = std::max({prior_fill, billboard.bar_ghost, billboard.bar_current});
    billboard.hit_flash = 1.0f;
    billboard.pulse_scale = 1.16f;
}

void merge_bar_juice_on_upsert(WorldUiBillboard& billboard, const WorldUiBillboard* previous, bool force_hit_juice) {
    if (!billboard.has_bar) return;
    if (previous && previous->has_bar) {
        const double old_current = previous->bar_current;
        const double old_ghost = std::max(previous->bar_ghost, old_current);
        if (force_hit_juice || billboard.bar_current + 1e-6 < old_current) {
            apply_hit_juice(billboard, old_ghost);
        } else {
            billboard.bar_ghost = std::max(billboard.bar_current, previous->bar_ghost);
            billboard.hit_flash = previous->hit_flash;
            billboard.pulse_scale = previous->pulse_scale;
        }
        return;
    }
    if (force_hit_juice) {
        const double prior = billboard.bar_max > 0.0 ? billboard.bar_max : billboard.bar_current;
        apply_hit_juice(billboard, prior);
    } else {
        billboard.bar_ghost = billboard.bar_current;
        billboard.hit_flash = 0.0f;
        billboard.pulse_scale = 1.0f;
    }
}

void scale_rect_about_center(ImVec2& min, ImVec2& max, float pulse) {
    if (pulse <= 1.001f) return;
    const float cx = (min.x + max.x) * 0.5f;
    const float cy = (min.y + max.y) * 0.5f;
    min.x = cx + (min.x - cx) * pulse;
    min.y = cy + (min.y - cy) * pulse;
    max.x = cx + (max.x - cx) * pulse;
    max.y = cy + (max.y - cy) * pulse;
}

} // namespace

void WorldUiBillboardRuntime::clear() { billboards_.clear(); }

void WorldUiBillboardRuntime::upsert(WorldUiBillboard billboard) {
    if (billboard.id.empty()) return;
    const auto it = billboards_.find(billboard.id);
    const WorldUiBillboard* previous = it != billboards_.end() ? &it->second : nullptr;
    const bool force_hit_juice = billboard.request_hit_juice;
    billboard.request_hit_juice = false;
    merge_bar_juice_on_upsert(billboard, previous, force_hit_juice);
    billboards_[billboard.id] = std::move(billboard);
}

void WorldUiBillboardRuntime::trigger_hit_juice(const std::string& id) {
    const auto it = billboards_.find(id);
    if (it == billboards_.end() || !it->second.has_bar) return;
    const double prior = std::max(it->second.bar_ghost, it->second.bar_current);
    apply_hit_juice(it->second, prior > it->second.bar_current ? prior : it->second.bar_max);
}

void WorldUiBillboardRuntime::remove(const std::string& id) {
    if (id.empty()) return;
    billboards_.erase(id);
}

void WorldUiBillboardRuntime::set_visible(const std::string& id, bool visible) {
    const auto it = billboards_.find(id);
    if (it == billboards_.end()) return;
    it->second.visible = visible;
}

void WorldUiBillboardRuntime::set_text(const std::string& id, std::string text) {
    const auto it = billboards_.find(id);
    if (it == billboards_.end()) return;
    it->second.text = std::move(text);
}

void WorldUiBillboardRuntime::set_world_position(const std::string& id, float x, float y, float z) {
    const auto it = billboards_.find(id);
    if (it == billboards_.end()) return;
    it->second.world_x = x;
    it->second.world_y = y;
    it->second.world_z = z;
}

void WorldUiBillboardRuntime::set_bar(const std::string& id, double current, double max) {
    const auto it = billboards_.find(id);
    if (it == billboards_.end()) return;
    if (!(max > 0.0) || !std::isfinite(max)) max = 1.0;
    if (!std::isfinite(current)) current = 0.0;
    current = std::clamp(current, 0.0, max);
    const bool had_bar = it->second.has_bar;
    const double old_current = it->second.bar_current;
    const double old_ghost = std::max(it->second.bar_ghost, old_current);
    it->second.has_bar = true;
    it->second.bar_max = max;
    it->second.bar_current = current;
    if (had_bar && current + 1e-6 < old_current) {
        apply_hit_juice(it->second, old_ghost);
    } else if (!had_bar) {
        it->second.bar_ghost = current;
    } else {
        it->second.bar_ghost = std::max(current, it->second.bar_ghost);
    }
}

void WorldUiBillboardRuntime::clear_bar(const std::string& id) {
    const auto it = billboards_.find(id);
    if (it == billboards_.end()) return;
    it->second.has_bar = false;
    it->second.bar_ghost = 0.0;
    it->second.hit_flash = 0.0f;
    it->second.pulse_scale = 1.0f;
}

void WorldUiBillboardRuntime::tick(float dt_seconds) {
    const float dt = std::clamp(dt_seconds, 0.0f, 0.1f);
    if (dt <= 0.0f) return;
    for (auto& [_, billboard] : billboards_) {
        if (billboard.hit_flash > 0.0f) {
            billboard.hit_flash = std::max(0.0f, billboard.hit_flash - dt / 0.32f);
        }
        if (billboard.pulse_scale > 1.001f) {
            billboard.pulse_scale = approach(billboard.pulse_scale, 1.0f, dt, 0.24f);
        } else {
            billboard.pulse_scale = 1.0f;
        }
        if (!billboard.has_bar) continue;
        if (billboard.bar_ghost > billboard.bar_current + 1e-6) {
            const double rate = std::max(48.0, billboard.bar_max * 2.2);
            billboard.bar_ghost = std::max(billboard.bar_current, billboard.bar_ghost - rate * static_cast<double>(dt));
        } else {
            billboard.bar_ghost = billboard.bar_current;
        }
    }
}

std::optional<WorldUiBillboard> WorldUiBillboardRuntime::get(const std::string& id) const {
    const auto it = billboards_.find(id);
    if (it == billboards_.end()) return std::nullopt;
    return it->second;
}

std::vector<std::string> WorldUiBillboardRuntime::ids() const {
    std::vector<std::string> out;
    out.reserve(billboards_.size());
    for (const auto& [id, _] : billboards_) out.push_back(id);
    return out;
}

void WorldUiBillboardRuntime::sync_interact_prompt(bool show, const std::string& label, float x, float y, float z) {
    if (!show) {
        remove(kInteractBillboardId);
        return;
    }
    WorldUiBillboard chip;
    chip.id = kInteractBillboardId;
    chip.world_x = x;
    chip.world_y = y;
    chip.world_z = z;
    chip.text = label.empty() ? "Press E to interact" : label;
    chip.visible = true;
    chip.font_size = 22.0f;
    upsert(std::move(chip));
}

void WorldUiBillboardRuntime::draw(ImDrawList* draw_list, const std::array<float, 16>& view_projection,
    const ViewportRect& viewport) const {
    if (!draw_list || billboards_.empty()) return;

    ImFont* font = GameFonts::display() ? GameFonts::display() : ImGui::GetFont();
    for (const auto& [_, billboard] : billboards_) {
        if (!billboard.visible) continue;
        if (billboard.text.empty() && !billboard.has_bar) continue;

        float sx = 0.0f;
        float sy = 0.0f;
        float depth = 0.0f;
        if (!project_world_to_screen(view_projection, viewport, billboard.world_x, billboard.world_y, billboard.world_z,
                sx, sy, depth) ||
            depth <= 0.0f) {
            continue;
        }

        const float font_sz = billboard.font_size > 0.0f ? billboard.font_size : 22.0f;
        const float pulse = std::clamp(billboard.pulse_scale, 1.0f, 1.35f);
        const float flash = std::clamp(billboard.hit_flash, 0.0f, 1.0f);
        const ImU32 panel = to_col(billboard.panel_color);
        const ImU32 border_base = to_col(billboard.border_color);
        const ImU32 border = lerp_col(border_base, IM_COL32(255, 248, 220, 255), flash);
        const ImU32 text_col = to_col(billboard.text_color);

        ImVec2 text_size{0.0f, 0.0f};
        if (!billboard.text.empty()) {
            text_size = font->CalcTextSizeA(font_sz, FLT_MAX, 0.0f, billboard.text.c_str());
        }

        // Text-only prompts keep the classic full plate. Health chips split:
        // label plate (optional) + independently bordered bar (no shared grey panel).
        if (!billboard.has_bar) {
            const ImVec2 pad{10.0f, 6.0f};
            ImVec2 box_min{sx - text_size.x * 0.5f - pad.x, sy - text_size.y - pad.y - 8.0f};
            ImVec2 box_max{sx + text_size.x * 0.5f + pad.x, sy - 4.0f};
            scale_rect_about_center(box_min, box_max, pulse);
            draw_list->AddRectFilled(box_min, box_max, panel, 6.0f);
            draw_list->AddRect(box_min, box_max, border, 6.0f, 0, 1.5f);
            const float text_x = box_min.x + (box_max.x - box_min.x - text_size.x) * 0.5f;
            const float text_y = box_min.y + (box_max.y - box_min.y - text_size.y) * 0.5f;
            draw_list->AddText(font, font_sz, ImVec2{text_x, text_y}, text_col, billboard.text.c_str());
            continue;
        }

        const float bar_w = 110.0f;
        const float bar_h = 12.0f;
        const float bar_border = 1.75f;
        const float gap = 5.0f;
        const ImVec2 label_pad{8.0f, 4.0f};

        float stack_h = bar_h + bar_border * 2.0f;
        if (!billboard.text.empty()) {
            stack_h += text_size.y + label_pad.y * 2.0f + gap;
        }
        float cursor_y = sy - stack_h - 8.0f;

        if (!billboard.text.empty()) {
            ImVec2 label_min{sx - text_size.x * 0.5f - label_pad.x, cursor_y};
            ImVec2 label_max{sx + text_size.x * 0.5f + label_pad.x, cursor_y + text_size.y + label_pad.y * 2.0f};
            scale_rect_about_center(label_min, label_max, pulse);
            draw_list->AddRectFilled(label_min, label_max, panel, 5.0f);
            draw_list->AddRect(label_min, label_max, border, 5.0f, 0, 1.35f + flash * 0.75f);
            const float text_x = label_min.x + (label_max.x - label_min.x - text_size.x * pulse) * 0.5f;
            const float text_y = label_min.y + (label_max.y - label_min.y - text_size.y * pulse) * 0.5f;
            draw_list->AddText(font, font_sz * pulse, ImVec2{text_x, text_y}, text_col, billboard.text.c_str());
            cursor_y = label_max.y + gap;
        }

        const float scaled_bar_w = bar_w * pulse;
        const float scaled_bar_h = bar_h * pulse;
        ImVec2 track_min{sx - scaled_bar_w * 0.5f, cursor_y + bar_border};
        ImVec2 track_max{sx + scaled_bar_w * 0.5f, cursor_y + bar_border + scaled_bar_h};
        ImVec2 frame_min{track_min.x - bar_border, track_min.y - bar_border};
        ImVec2 frame_max{track_max.x + bar_border, track_max.y + bar_border};

        if (flash > 0.02f) {
            const float glow = 2.5f + 4.0f * flash;
            draw_list->AddRect(ImVec2{frame_min.x - glow, frame_min.y - glow},
                ImVec2{frame_max.x + glow, frame_max.y + glow},
                IM_COL32(255, 220, 140, static_cast<int>(120.0f * flash)), 4.0f, 0, 2.25f);
        }

        // Separate bar chrome: dark track well + own gold/iron frame (not the old shared plate).
        draw_list->AddRectFilled(frame_min, frame_max, IM_COL32(22, 18, 14, 230), 4.0f);
        draw_list->AddRect(frame_min, frame_max, border, 4.0f, 0, bar_border + flash * 0.85f);
        draw_list->AddRectFilled(track_min, track_max, IM_COL32(30, 26, 22, 235), 3.0f);

        const double max = billboard.bar_max > 0.0 ? billboard.bar_max : 1.0;
        const float ghost_fill = static_cast<float>(std::clamp(billboard.bar_ghost / max, 0.0, 1.0));
        const float fill = static_cast<float>(std::clamp(billboard.bar_current / max, 0.0, 1.0));
        if (ghost_fill > fill + 0.001f) {
            draw_list->AddRectFilled(track_min, ImVec2{track_min.x + scaled_bar_w * ghost_fill, track_max.y},
                IM_COL32(235, 200, 120, 210), 3.0f);
        }
        if (fill > 0.0f) {
            const ImU32 bar = lerp_col(to_col(billboard.bar_color), IM_COL32(255, 210, 190, 255), flash * 0.85f);
            draw_list->AddRectFilled(track_min, ImVec2{track_min.x + scaled_bar_w * fill, track_max.y}, bar, 3.0f);
        }
        if (flash > 0.05f) {
            draw_list->AddRect(frame_min, frame_max, IM_COL32(255, 245, 210, static_cast<int>(210.0f * flash)), 4.0f, 0,
                1.75f);
        }
    }
}

} // namespace engine
