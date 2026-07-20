#include "engine/ui/editor_ui_hotspots.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>

namespace engine {
namespace {

std::string to_lower(std::string_view text) {
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

} // namespace

void EditorUiHotspotRegistry::clear() {
    items.clear();
}

void EditorUiHotspotRegistry::set_window_size(int w, int h) {
    window_w = (std::max)(1, w);
    window_h = (std::max)(1, h);
}

void EditorUiHotspotRegistry::add_rect(std::string id, float min_x, float min_y, float max_x, float max_y,
    std::string label) {
    if (id.empty()) return;
    if (max_x < min_x || max_y < min_y) return;
    EditorUiHotspot spot;
    spot.id = std::move(id);
    spot.label = std::move(label);
    spot.min_x = min_x;
    spot.min_y = min_y;
    spot.max_x = max_x;
    spot.max_y = max_y;
    spot.cx = 0.5f * (min_x + max_x);
    spot.cy = 0.5f * (min_y + max_y);
    // Replace prior same-id registration from this frame (last draw wins).
    for (auto& existing : items) {
        if (existing.id == spot.id) {
            existing = std::move(spot);
            return;
        }
    }
    items.push_back(std::move(spot));
}

const EditorUiHotspot* EditorUiHotspotRegistry::find_exact(std::string_view id) const {
    for (const auto& item : items) {
        if (item.id == id) return &item;
    }
    return nullptr;
}

std::vector<const EditorUiHotspot*> EditorUiHotspotRegistry::find_filter(std::string_view id_prefix,
    std::string_view contains) const {
    std::vector<const EditorUiHotspot*> out;
    const auto needle = to_lower(contains);
    for (const auto& item : items) {
        if (!id_prefix.empty() && item.id.rfind(id_prefix, 0) != 0) continue;
        if (!needle.empty()) {
            const auto hay_id = to_lower(item.id);
            const auto hay_label = to_lower(item.label);
            if (hay_id.find(needle) == std::string::npos && hay_label.find(needle) == std::string::npos) continue;
        }
        out.push_back(&item);
    }
    return out;
}

void register_ui_hotspot_last_item(EditorUiHotspotRegistry* registry, std::string_view id, std::string_view label) {
    if (!registry || id.empty()) return;
    if (!ImGui::IsItemVisible()) return;
    const ImVec2 mn = ImGui::GetItemRectMin();
    const ImVec2 mx = ImGui::GetItemRectMax();
    registry->add_rect(std::string(id), mn.x, mn.y, mx.x, mx.y, std::string(label));
}

} // namespace engine
