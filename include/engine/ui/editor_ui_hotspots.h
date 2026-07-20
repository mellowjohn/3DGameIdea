#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace engine {

/// Named ImGui widget bounds for MCP UI query / click-by-id (no CV).
struct EditorUiHotspot {
    std::string id;
    std::string label;
    float min_x = 0.0f;
    float min_y = 0.0f;
    float max_x = 0.0f;
    float max_y = 0.0f;
    float cx = 0.0f;
    float cy = 0.0f;
};

struct EditorUiHotspotRegistry {
    std::vector<EditorUiHotspot> items;
    int window_w = 1;
    int window_h = 1;

    void clear();
    void set_window_size(int w, int h);
    void add_rect(std::string id, float min_x, float min_y, float max_x, float max_y, std::string label = {});
    [[nodiscard]] const EditorUiHotspot* find_exact(std::string_view id) const;
    [[nodiscard]] std::vector<const EditorUiHotspot*> find_filter(std::string_view id_prefix,
        std::string_view contains) const;
};

/// Capture `ImGui::GetItemRect*` for the last submitted item (no-op if registry null / invisible).
void register_ui_hotspot_last_item(EditorUiHotspotRegistry* registry, std::string_view id,
    std::string_view label = {});

} // namespace engine
