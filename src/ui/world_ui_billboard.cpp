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

} // namespace

void WorldUiBillboardRuntime::clear() { billboards_.clear(); }

void WorldUiBillboardRuntime::upsert(WorldUiBillboard billboard) {
    if (billboard.id.empty()) return;
    billboards_[billboard.id] = std::move(billboard);
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
    it->second.has_bar = true;
    it->second.bar_max = max;
    it->second.bar_current = std::clamp(current, 0.0, max);
}

void WorldUiBillboardRuntime::clear_bar(const std::string& id) {
    const auto it = billboards_.find(id);
    if (it == billboards_.end()) return;
    it->second.has_bar = false;
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
        const ImVec2 pad{10.0f, 6.0f};
        float content_w = 0.0f;
        float content_h = 0.0f;
        ImVec2 text_size{0.0f, 0.0f};
        if (!billboard.text.empty()) {
            text_size = font->CalcTextSizeA(font_sz, FLT_MAX, 0.0f, billboard.text.c_str());
            content_w = text_size.x;
            content_h = text_size.y;
        }
        const float bar_w = 96.0f;
        const float bar_h = 10.0f;
        if (billboard.has_bar) {
            content_w = std::max(content_w, bar_w);
            content_h += (billboard.text.empty() ? 0.0f : 6.0f) + bar_h;
        }

        const ImVec2 box_min{sx - content_w * 0.5f - pad.x, sy - content_h - pad.y - 8.0f};
        const ImVec2 box_max{sx + content_w * 0.5f + pad.x, sy - 4.0f};
        draw_list->AddRectFilled(box_min, box_max, to_col(billboard.panel_color), 6.0f);
        draw_list->AddRect(box_min, box_max, to_col(billboard.border_color), 6.0f, 0, 1.5f);

        float cursor_y = box_min.y + pad.y;
        if (!billboard.text.empty()) {
            const float text_x = box_min.x + (box_max.x - box_min.x - text_size.x) * 0.5f;
            draw_list->AddText(font, font_sz, ImVec2{text_x, cursor_y}, to_col(billboard.text_color),
                billboard.text.c_str());
            cursor_y += text_size.y + (billboard.has_bar ? 6.0f : 0.0f);
        }
        if (billboard.has_bar) {
            const float track_x = box_min.x + (box_max.x - box_min.x - bar_w) * 0.5f;
            const ImVec2 track_min{track_x, cursor_y};
            const ImVec2 track_max{track_x + bar_w, cursor_y + bar_h};
            draw_list->AddRectFilled(track_min, track_max, IM_COL32(30, 26, 22, 220), 3.0f);
            const double max = billboard.bar_max > 0.0 ? billboard.bar_max : 1.0;
            const float fill = static_cast<float>(std::clamp(billboard.bar_current / max, 0.0, 1.0));
            if (fill > 0.0f) {
                draw_list->AddRectFilled(track_min, ImVec2{track_min.x + bar_w * fill, track_max.y},
                    to_col(billboard.bar_color), 3.0f);
            }
        }
    }
}

} // namespace engine
