#pragma once

#include "engine/rendering/viewport_math.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct ImDrawList;

namespace engine {

enum class CombatTextKind : std::uint8_t { Hit, Crit, Bleed, Poison, Burn, Heal };

[[nodiscard]] const char* combat_text_kind_id(CombatTextKind kind) noexcept;
[[nodiscard]] CombatTextKind combat_text_kind_from_id(const std::string& id) noexcept;

/// Ephemeral floating combat number. World-anchored; C++ owns juice.
struct CombatTextFloater {
    float world_x = 0.0f;
    float world_y = 0.0f;
    float world_z = 0.0f;
    std::string text;
    CombatTextKind kind = CombatTextKind::Hit;
    float age = 0.0f;
    float lifetime = 0.9f;
    float rise_height = 0.72f;
    float rest_scale = 1.0f;
    float impact_peak = 1.42f;
    float base_font = 28.0f;
    std::array<float, 4> color{241.0f, 238.0f, 232.0f, 255.0f};
};

class CombatTextRuntime final {
public:
    void clear();
    void spawn(float x, float y, float z, double amount, CombatTextKind kind = CombatTextKind::Hit);
    void spawn_text(float x, float y, float z, std::string text, CombatTextKind kind = CombatTextKind::Hit);
    /// Legacy helpers.
    void spawn(float x, float y, float z, double amount, bool crit);
    void spawn_text(float x, float y, float z, std::string text, bool crit);

    void tick(float dt_seconds);
    void draw(ImDrawList* draw_list, const std::array<float, 16>& view_projection,
        const ViewportRect& viewport) const;

    [[nodiscard]] std::size_t size() const noexcept { return floaters_.size(); }
    [[nodiscard]] const std::vector<CombatTextFloater>& floaters() const noexcept { return floaters_; }

private:
    std::vector<CombatTextFloater> floaters_;
    std::uint32_t spawn_salt_ = 0;
};

} // namespace engine
