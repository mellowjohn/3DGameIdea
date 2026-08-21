#pragma once

#include "engine/physics/collision_world.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace engine {

enum class CombatVolumeRole : std::uint8_t { Hit, Hurt };

struct CombatVolumeBinding {
    std::string placement_entity_id;
    std::uint32_t volume_index = 0;
    CombatVolumeRole role = CombatVolumeRole::Hurt;
    std::string combat_id;
};

struct CombatContactEvent {
    std::string attacker_id;
    std::string hurt_placement_entity_id;
    std::string hurt_combat_id;
    std::uint32_t hurt_volume_index = 0;
    std::optional<WorldPosition> contact_point;
    /// Live feet of the hurt placement (scene transform). Prefer for combat text /
    /// impact anchors — sphere contact points sit on a radial surface and miss.
    std::optional<WorldPosition> hurt_entity_position;
    /// Melee combo step when known: 1=attack, 2=attack2, 3=attack3. 0 = full range (ranged / unknown).
    int attacker_combo_step = 0;
    /// True when the defender blocked/parried this melee contact (no HP).
    bool blocked = false;
    /// Lightning chain hop: 0 = primary bolt, 1+ = jump from a prior target.
    int chain_hop = 0;
};

class CombatVolumeRegistry final {
public:
    void bind(CollisionBody body, CombatVolumeBinding binding);
    void unbind(CollisionBody body);
    void clear();
    [[nodiscard]] std::optional<CombatVolumeBinding> find(CollisionBody body) const;
    [[nodiscard]] bool is_combat_body(CollisionBody body) const;
    [[nodiscard]] bool is_hit_body(CollisionBody body) const;
    [[nodiscard]] bool is_hurt_body(CollisionBody body) const;
    [[nodiscard]] std::vector<CollisionBody> bodies_for_role(CombatVolumeRole role) const;

private:
    std::map<std::uint32_t, CombatVolumeBinding> bindings_;
};

[[nodiscard]] std::vector<CombatContactEvent> query_combat_hits(const std::string& attacker_id, WorldPosition center,
    float radius, const CollisionWorld& world, const CombatVolumeRegistry& registry,
    std::string_view ignore_placement_entity_id = {});

/// Sphere probes sampled along `from`→`to` so fast projectiles still overlap hurt volumes.
/// Contacts are unique per `hurt_placement_entity_id`.
/// `ignore_placement_entity_id` skips the shooter's own hurt volume (arrows/bolts leave the nock inside it).
[[nodiscard]] std::vector<CombatContactEvent> query_combat_hits_along_segment(const std::string& attacker_id,
    WorldPosition from, WorldPosition to, float radius, const CollisionWorld& world,
    const CombatVolumeRegistry& registry, std::string_view ignore_placement_entity_id = {});

[[nodiscard]] std::vector<CombatContactEvent> query_combat_hits_from_body(const std::string& attacker_id,
    CollisionBody hit_body, const CollisionWorld& world, const CombatVolumeRegistry& registry,
    std::string_view ignore_placement_entity_id = {});

/// Play-test player animator cue from HUD health. Damage chips HP without HitReact stagger.
enum class PlayerHealthAnimCue : std::uint8_t { None, Die, Revive };

[[nodiscard]] inline PlayerHealthAnimCue player_health_anim_cue(bool dead, double last_health, double health) {
    if (!dead && health <= 0.0 && last_health > 0.0)
        return PlayerHealthAnimCue::Die;
    if (dead && health > 0.0)
        return PlayerHealthAnimCue::Revive;
    return PlayerHealthAnimCue::None;
}

} // namespace engine
