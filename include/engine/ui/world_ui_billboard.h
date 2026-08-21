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
    /// Delayed fill that trails behind `bar_current` after damage (hit juice).
    double bar_ghost = 0.0;
    /// 0 = idle, 1 = just hit; decays in `tick`.
    float hit_flash = 0.0f;
    /// Chip scale pulse after a hit (1 = rest).
    float pulse_scale = 1.0f;
    /// One-shot: upsert seeds ghost trail + flash + pulse (cleared during merge).
    bool request_hit_juice = false;
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
    /// Force hit juice (ghost trail + flash + pulse). Uses prior fill when present, else `bar_max`.
    void trigger_hit_juice(const std::string& id);
    void remove(const std::string& id);
    void set_visible(const std::string& id, bool visible);
    void set_text(const std::string& id, std::string text);
    void set_world_position(const std::string& id, float x, float y, float z);
    void set_bar(const std::string& id, double current, double max);
    void clear_bar(const std::string& id);
    /// Advance ghost drain / flash / pulse. Call once per frame before `draw`.
    void tick(float dt_seconds);

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
