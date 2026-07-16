#pragma once

#include "engine/physics/collision_world.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
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
    float radius, const CollisionWorld& world, const CombatVolumeRegistry& registry);

[[nodiscard]] std::vector<CombatContactEvent> query_combat_hits_from_body(const std::string& attacker_id,
    CollisionBody hit_body, const CollisionWorld& world, const CombatVolumeRegistry& registry);

} // namespace engine
