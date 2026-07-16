#include "engine/world/combat_volumes.h"

#include <algorithm>
#include <cmath>

namespace engine {

void CombatVolumeRegistry::bind(CollisionBody body, CombatVolumeBinding binding) {
    if (!body.valid()) return;
    bindings_[body.value] = std::move(binding);
}

void CombatVolumeRegistry::unbind(CollisionBody body) {
    if (!body.valid()) return;
    bindings_.erase(body.value);
}

void CombatVolumeRegistry::clear() { bindings_.clear(); }

std::optional<CombatVolumeBinding> CombatVolumeRegistry::find(CollisionBody body) const {
    if (!body.valid()) return std::nullopt;
    const auto found = bindings_.find(body.value);
    return found == bindings_.end() ? std::nullopt : std::optional<CombatVolumeBinding>(found->second);
}

bool CombatVolumeRegistry::is_combat_body(CollisionBody body) const {
    return body.valid() && bindings_.find(body.value) != bindings_.end();
}

bool CombatVolumeRegistry::is_hit_body(CollisionBody body) const {
    const auto binding = find(body);
    return binding && binding->role == CombatVolumeRole::Hit;
}

bool CombatVolumeRegistry::is_hurt_body(CollisionBody body) const {
    const auto binding = find(body);
    return binding && binding->role == CombatVolumeRole::Hurt;
}

std::vector<CollisionBody> CombatVolumeRegistry::bodies_for_role(CombatVolumeRole role) const {
    std::vector<CollisionBody> bodies;
    for (const auto& entry : bindings_) {
        if (entry.second.role != role) continue;
        bodies.push_back({entry.first});
    }
    return bodies;
}

std::vector<CombatContactEvent> query_combat_hits(const std::string& attacker_id, WorldPosition center, float radius,
    const CollisionWorld& world, const CombatVolumeRegistry& registry) {
    std::vector<CombatContactEvent> events;
    if (attacker_id.empty() || !(radius > 0)) return events;

    CollisionQueryFilter filter;
    filter.layer = CollisionLayer::Trigger;
    const auto overlaps = world.overlap_sphere(center, radius, filter);
    if (!overlaps) return events;

    for (const auto& hit : overlaps.value()) {
        const auto binding = registry.find(hit.body);
        if (!binding || binding->role != CombatVolumeRole::Hurt) continue;
        CombatContactEvent event;
        event.attacker_id = attacker_id;
        event.hurt_placement_entity_id = binding->placement_entity_id;
        event.hurt_combat_id = binding->combat_id;
        event.hurt_volume_index = binding->volume_index;
        event.contact_point = hit.contact_point;
        events.push_back(event);
    }
    return events;
}

std::vector<CombatContactEvent> query_combat_hits_from_body(const std::string& attacker_id, CollisionBody hit_body,
    const CollisionWorld& world, const CombatVolumeRegistry& registry) {
    if (!registry.is_hit_body(hit_body)) return {};

    WorldPosition center{};
    float radius = 0.5f;
    for (const auto& debug : world.debug_bodies()) {
        if (debug.body.value != hit_body.value) continue;
        center = debug.position;
        if (debug.shape == CollisionDebugShape::Sphere) {
            radius = debug.radius;
        } else {
            radius = std::max({debug.half_extent.x, debug.half_extent.y, debug.half_extent.z});
        }
        break;
    }
    return query_combat_hits(attacker_id, center, radius, world, registry);
}

} // namespace engine
