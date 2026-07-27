#pragma once

#include "engine/rendering/viewport_math.h"

#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct ImDrawList;

namespace engine {

/// World-anchored UI chip (Roblox BillboardGui-style): text and optional bar, projected each frame.
struct WorldUiBillboard {
    std::string id;
    float world_x = 0.0f;
    float world_y = 0.0f;
    float world_z = 0.0f;
    std::string text;
    bool has_bar = false;
    double bar_current = 0.0;
    double bar_max = 100.0;
    bool visible = true;
    float font_size = 22.0f;
    /// RGBA 0–255
    std::array<float, 4> panel_color{18.0f, 16.0f, 12.0f, 210.0f};
    std::array<float, 4> border_color{213.0f, 185.0f, 120.0f, 230.0f};
    std::array<float, 4> text_color{245.0f, 236.0f, 214.0f, 255.0f};
    std::array<float, 4> bar_color{139.0f, 58.0f, 58.0f, 255.0f};
};

class WorldUiBillboardRuntime final {
public:
    void clear();
    void upsert(WorldUiBillboard billboard);
    void remove(const std::string& id);
    void set_visible(const std::string& id, bool visible);
    void set_text(const std::string& id, std::string text);
    void set_world_position(const std::string& id, float x, float y, float z);
    void set_bar(const std::string& id, double current, double max);
    void clear_bar(const std::string& id);

    [[nodiscard]] std::optional<WorldUiBillboard> get(const std::string& id) const;
    [[nodiscard]] std::size_t size() const noexcept { return billboards_.size(); }
    [[nodiscard]] std::vector<std::string> ids() const;

    /// Dedicated interact prompt slot driven by Lua `interact.*` blackboard keys.
    void sync_interact_prompt(bool show, const std::string& label, float x, float y, float z);

    void draw(ImDrawList* draw_list, const std::array<float, 16>& view_projection, const ViewportRect& viewport) const;

private:
    std::map<std::string, WorldUiBillboard> billboards_;
};

} // namespace engine
