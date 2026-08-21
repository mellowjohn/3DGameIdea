#include "engine/combat/status_effect_runtime.h"

#include <algorithm>
#include <cmath>

namespace engine {

const char* status_effect_kind_id(StatusEffectKind kind) noexcept {
    switch (kind) {
    case StatusEffectKind::Poison:
        return "poison";
    case StatusEffectKind::Burn:
        return "burn";
    case StatusEffectKind::Slow:
        return "slow";
    case StatusEffectKind::Bleed:
        return "bleed";
    }
    return "bleed";
}

StatusEffectKind status_effect_kind_from_id(const std::string& id) noexcept {
    if (id == "poison") return StatusEffectKind::Poison;
    if (id == "burn") return StatusEffectKind::Burn;
    if (id == "slow") return StatusEffectKind::Slow;
    return StatusEffectKind::Bleed;
}

bool is_known_status_effect_id(const std::string& id) noexcept {
    return id == "poison" || id == "bleed" || id == "burn" || id == "slow";
}

bool status_effect_deals_tick_damage(StatusEffectKind kind) noexcept {
    return kind != StatusEffectKind::Slow;
}

void StatusEffectRuntime::clear() { targets_.clear(); }

void StatusEffectRuntime::clear_target(const std::string& target_id) {
    if (target_id.empty()) return;
    targets_.erase(target_id);
}

void StatusEffectRuntime::set_target_anchor(const std::string& target_id, float x, float y, float z) {
    if (target_id.empty()) return;
    auto& target = targets_[target_id];
    target.target_id = target_id;
    target.world_x = x;
    target.world_y = y;
    target.world_z = z;
}

void StatusEffectRuntime::apply(const std::string& target_id, const StatusEffectDef& def) {
    apply(target_id, def.kind, def.damage_per_tick, def.duration_seconds, def.tick_interval_seconds);
}

void StatusEffectRuntime::apply(const std::string& target_id, StatusEffectKind kind, float damage_per_tick,
    float duration_seconds, float tick_interval_seconds) {
    if (target_id.empty()) return;
    if (!(duration_seconds > 0.0f)) return;
    if (status_effect_deals_tick_damage(kind) && !(damage_per_tick > 0.0f)) return;
    const float dmg = status_effect_deals_tick_damage(kind) ? damage_per_tick : 0.0f;
    const float interval = tick_interval_seconds > 0.05f ? tick_interval_seconds : 1.0f;
    auto& target = targets_[target_id];
    target.target_id = target_id;
    for (auto& existing : target.effects) {
        if (existing.kind == kind) {
            existing.stacks = std::min(existing.stacks + 1, kStatusEffectMaxStacks);
            existing.damage_per_tick = std::max(existing.damage_per_tick, dmg);
            existing.remaining_seconds = duration_seconds;
            existing.duration_seconds = duration_seconds;
            existing.tick_interval_seconds = interval;
            existing.tick_accum = 0.0f;
            return;
        }
    }
    StatusEffectInstance inst;
    inst.kind = kind;
    inst.damage_per_tick = dmg;
    inst.remaining_seconds = duration_seconds;
    inst.duration_seconds = duration_seconds;
    inst.tick_interval_seconds = interval;
    inst.tick_accum = 0.0f;
    inst.stacks = 1;
    target.effects.push_back(inst);
}

std::vector<StatusTickEvent> StatusEffectRuntime::tick(float dt_seconds) {
    std::vector<StatusTickEvent> events;
    const float dt = std::clamp(dt_seconds, 0.0f, 0.1f);
    if (dt <= 0.0f) return events;

    for (auto it = targets_.begin(); it != targets_.end();) {
        auto& target = it->second;
        for (auto& effect : target.effects) {
            if (effect.remaining_seconds <= 0.0f) continue;
            effect.remaining_seconds -= dt;
            effect.tick_accum += dt;
            while (effect.tick_accum + 1e-6f >= effect.tick_interval_seconds) {
                effect.tick_accum -= effect.tick_interval_seconds;
                if (!status_effect_deals_tick_damage(effect.kind)) {
                    if (effect.remaining_seconds <= -effect.tick_interval_seconds) break;
                    continue;
                }
                StatusTickEvent ev;
                ev.target_id = target.target_id;
                ev.kind = effect.kind;
                ev.stacks = std::max(1, effect.stacks);
                ev.amount = effect.damage_per_tick * static_cast<float>(ev.stacks);
                ev.world_x = target.world_x;
                ev.world_y = target.world_y;
                ev.world_z = target.world_z;
                events.push_back(ev);
                // Allow the last interval to fire even if remaining just crossed zero.
                if (effect.remaining_seconds <= -effect.tick_interval_seconds) break;
            }
        }
        target.effects.erase(std::remove_if(target.effects.begin(), target.effects.end(),
                                  [](const StatusEffectInstance& e) { return e.remaining_seconds <= 0.0f; }),
            target.effects.end());
        if (target.effects.empty()) {
            it = targets_.erase(it);
        } else {
            ++it;
        }
    }
    return events;
}

bool StatusEffectRuntime::has_effect(const std::string& target_id, StatusEffectKind kind) const {
    return stack_count(target_id, kind) > 0;
}

int StatusEffectRuntime::stack_count(const std::string& target_id, StatusEffectKind kind) const {
    const auto it = targets_.find(target_id);
    if (it == targets_.end()) return 0;
    for (const auto& effect : it->second.effects) {
        if (effect.kind == kind) return std::max(1, effect.stacks);
    }
    return 0;
}

float StatusEffectRuntime::remaining_seconds(const std::string& target_id, StatusEffectKind kind) const {
    const auto it = targets_.find(target_id);
    if (it == targets_.end()) return 0.0f;
    for (const auto& effect : it->second.effects) {
        if (effect.kind == kind) return std::max(0.0f, effect.remaining_seconds);
    }
    return 0.0f;
}

float StatusEffectRuntime::duration_seconds(const std::string& target_id, StatusEffectKind kind) const {
    const auto it = targets_.find(target_id);
    if (it == targets_.end()) return 0.0f;
    for (const auto& effect : it->second.effects) {
        if (effect.kind == kind) return std::max(0.0f, effect.duration_seconds);
    }
    return 0.0f;
}

std::vector<StatusEffectKind> StatusEffectRuntime::active_kinds(const std::string& target_id) const {
    std::vector<StatusEffectKind> out;
    const auto it = targets_.find(target_id);
    if (it == targets_.end()) return out;
    for (const auto& effect : it->second.effects) out.push_back(effect.kind);
    return out;
}

std::string StatusEffectRuntime::status_line(const std::string& target_id) const {
    const auto it = targets_.find(target_id);
    if (it == targets_.end() || it->second.effects.empty()) return {};
    std::string line;
    for (std::size_t i = 0; i < it->second.effects.size(); ++i) {
        if (i > 0) line += " · ";
        const auto& effect = it->second.effects[i];
        if (effect.kind == StatusEffectKind::Poison) line += "Poison";
        else if (effect.kind == StatusEffectKind::Burn) line += "Burn";
        else if (effect.kind == StatusEffectKind::Slow) line += "Slow";
        else line += "Bleed";
        const int stacks = std::max(1, effect.stacks);
        if (stacks > 1) {
            line += " ×";
            line += std::to_string(stacks);
        }
    }
    return line;
}

std::vector<StatusHudChip> StatusEffectRuntime::hud_chips() const {
    std::vector<StatusHudChip> out;
    for (const auto& [_, target] : targets_) {
        for (const auto& effect : target.effects) {
            if (effect.remaining_seconds <= 0.0f) continue;
            StatusHudChip chip;
            chip.target_id = target.target_id;
            chip.kind = effect.kind;
            chip.stacks = std::max(1, effect.stacks);
            chip.remaining_seconds = std::max(0.0f, effect.remaining_seconds);
            chip.duration_seconds =
                effect.duration_seconds > 0.0f ? effect.duration_seconds : chip.remaining_seconds;
            chip.world_x = target.world_x;
            chip.world_y = target.world_y;
            chip.world_z = target.world_z;
            out.push_back(chip);
        }
    }
    return out;
}

} // namespace engine
