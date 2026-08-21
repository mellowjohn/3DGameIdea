#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine {

enum class StatusEffectKind : std::uint8_t { Poison, Bleed, Burn, Slow };

/// Soft cap so repeated hits do not unbounded-scale DoT DPS.
inline constexpr int kStatusEffectMaxStacks = 10;
/// Frost Slow wish scale on hostile chase (1 = full speed).
inline constexpr float kStatusEffectSlowWishScale = 0.4f;

[[nodiscard]] const char* status_effect_kind_id(StatusEffectKind kind) noexcept;
[[nodiscard]] StatusEffectKind status_effect_kind_from_id(const std::string& id) noexcept;
[[nodiscard]] bool is_known_status_effect_id(const std::string& id) noexcept;
[[nodiscard]] bool status_effect_deals_tick_damage(StatusEffectKind kind) noexcept;

struct StatusEffectDef {
    StatusEffectKind kind = StatusEffectKind::Bleed;
    float damage_per_tick = 1.0f;
    float duration_seconds = 6.0f;
    float tick_interval_seconds = 1.0f;
};

struct StatusEffectInstance {
    StatusEffectKind kind = StatusEffectKind::Bleed;
    /// Base damage per tick; total tick damage = damage_per_tick * stacks.
    float damage_per_tick = 1.0f;
    float remaining_seconds = 0.0f;
    float duration_seconds = 0.0f;
    float tick_interval_seconds = 1.0f;
    float tick_accum = 0.0f;
    int stacks = 1;
};

struct StatusTargetState {
    std::string target_id;
    float world_x = 0.0f;
    float world_y = 0.0f;
    float world_z = 0.0f;
    std::vector<StatusEffectInstance> effects;
};

struct StatusTickEvent {
    std::string target_id;
    StatusEffectKind kind = StatusEffectKind::Bleed;
    float amount = 0.0f;
    int stacks = 1;
    float world_x = 0.0f;
    float world_y = 0.0f;
    float world_z = 0.0f;
};

/// One active status for HUD / world-chip presentation.
struct StatusHudChip {
    std::string target_id;
    StatusEffectKind kind = StatusEffectKind::Bleed;
    int stacks = 1;
    float remaining_seconds = 0.0f;
    float duration_seconds = 0.0f;
    float world_x = 0.0f;
    float world_y = 0.0f;
    float world_z = 0.0f;
};

/// C++ owns DoT timers; Lua/catalog apply. Combat text + HUD consume tick events.
/// Same kind stacks (up to kStatusEffectMaxStacks); different kinds run together.
class StatusEffectRuntime final {
public:
    void clear();
    void clear_target(const std::string& target_id);
    void set_target_anchor(const std::string& target_id, float x, float y, float z);
    /// Add a stack of `kind` (or start at 1). Same kind stacks; poison + bleed coexist.
    void apply(const std::string& target_id, const StatusEffectDef& def);
    void apply(const std::string& target_id, StatusEffectKind kind, float damage_per_tick, float duration_seconds,
        float tick_interval_seconds);

    /// Advance timers; returns ticks that fired this frame (combat text / damage).
    [[nodiscard]] std::vector<StatusTickEvent> tick(float dt_seconds);

    [[nodiscard]] bool has_effect(const std::string& target_id, StatusEffectKind kind) const;
    [[nodiscard]] int stack_count(const std::string& target_id, StatusEffectKind kind) const;
    [[nodiscard]] float remaining_seconds(const std::string& target_id, StatusEffectKind kind) const;
    [[nodiscard]] float duration_seconds(const std::string& target_id, StatusEffectKind kind) const;
    [[nodiscard]] std::vector<StatusEffectKind> active_kinds(const std::string& target_id) const;
    [[nodiscard]] std::string status_line(const std::string& target_id) const;
    /// All active effects with anchors (for world/HUD icon chips).
    [[nodiscard]] std::vector<StatusHudChip> hud_chips() const;
    [[nodiscard]] std::size_t target_count() const noexcept { return targets_.size(); }

private:
    std::unordered_map<std::string, StatusTargetState> targets_;
};

} // namespace engine
