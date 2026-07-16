#include "engine/world/prefab_collision.h"

#include "engine/world/authored_components.h"
#include "engine/world/transform_utils.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>

namespace engine {
namespace {

LocalPosition scale_half_extent(const LocalPosition& half, const std::array<float, 3>& scale) {
    return {half.x * std::abs(scale[0]), half.y * std::abs(scale[1]), half.z * std::abs(scale[2])};
}

float scale_radius(float radius, const std::array<float, 3>& scale) {
    return radius * std::max({std::abs(scale[0]), std::abs(scale[1]), std::abs(scale[2])});
}

Result<std::vector<CollisionBody>> spawn_collision_volumes(CollisionWorld& world,
    const std::vector<PrefabCollisionVolume>& volumes, const TransformComponent& placement, CellCoord cell) {
    std::vector<CollisionBody> bodies;
    bodies.reserve(volumes.size());
    for (const auto& volume : volumes) {
        const auto world_transform = multiply_transforms(placement, volume.transform);
        const WorldPosition position{world_transform.position[0], world_transform.position[1], world_transform.position[2]};
        const CollisionLayer layer =
            (volume.trigger || volume.is_interaction() || volume.is_combat_sensor()) ? CollisionLayer::Trigger : volume.layer;
        if (volume.shape == PrefabCollisionShape::Box) {
            const auto half = scale_half_extent(volume.half_extent, world_transform.scale);
            const auto body = world.add_box(position, half, layer, false, cell, world_transform.rotation);
            if (!body) return Result<std::vector<CollisionBody>>::failure(body.error());
            bodies.push_back(body.value());
        } else {
            const auto radius = scale_radius(volume.radius, world_transform.scale);
            const auto body = world.add_sphere(position, radius, layer, false, cell, world_transform.rotation);
            if (!body) return Result<std::vector<CollisionBody>>::failure(body.error());
            bodies.push_back(body.value());
        }
    }
    return Result<std::vector<CollisionBody>>::success(std::move(bodies));
}

} // namespace

Result<std::vector<CollisionBody>> spawn_prefab_collision(CollisionWorld& world, const PrefabAsset& prefab,
    const TransformComponent& placement, CellCoord cell) {
    return spawn_collision_volumes(world, prefab.collision, placement, cell);
}

void PlacementCollisionTracker::remove_bodies(CollisionWorld& world, TrackedPlacement& placement) {
    for (const auto& body : placement.bodies) {
        interaction_registry_.unbind(body);
        combat_registry_.unbind(body);
        (void)world.remove(body);
    }
    placement.bodies.clear();
}

Result<void> PlacementCollisionTracker::sync(CollisionWorld& world, const Scene& scene,
    const std::map<std::string, PrefabAsset>& catalog) {
    std::set<std::string> active;
    for (const auto& id : scene.entity_ids()) {
        const auto placement = scene.placement(id);
        const auto transform = scene.transform(id);
        if (!placement || !transform) continue;
        const auto id_key = id.str();
        active.insert(id_key);
        const auto resolved = resolve_prefab_catalog_path(catalog, placement->prefab_asset);
        const auto found = catalog.find(resolved);
        const PrefabAsset* prefab = found == catalog.end() ? nullptr : &found->second;
        const auto authored = scene.authored_components(id);
        const auto volumes = effective_collision_volumes(authored ? &*authored : nullptr, prefab);
        auto& tracked = tracked_[id_key];
        const std::uint64_t generation = authored ? authored->generation : 0;
        const bool unchanged = tracked.prefab_path == resolved && tracked.cell == placement->cell &&
            tracked.components_generation == generation &&
            tracked.transform.position == transform->position && tracked.transform.rotation == transform->rotation &&
            tracked.transform.scale == transform->scale;
        if (unchanged) continue;
        remove_bodies(world, tracked);
        tracked.prefab_path = resolved;
        tracked.transform = *transform;
        tracked.cell = placement->cell;
        tracked.components_generation = generation;
        if (volumes.empty()) continue;
        const auto spawned = spawn_collision_volumes(world, volumes, *transform, placement->cell);
        if (!spawned) return Result<void>::failure(spawned.error());
        tracked.bodies = spawned.value();
        for (std::uint32_t index = 0; index < volumes.size(); ++index) {
            const auto& volume = volumes[index];
            if (!volume.is_interaction() || index >= tracked.bodies.size()) continue;
            interaction_registry_.bind(tracked.bodies[index],
                InteractionVolumeBinding{id_key, index, volume.interaction_id});
        }
        for (std::uint32_t index = 0; index < volumes.size(); ++index) {
            const auto& volume = volumes[index];
            if (!volume.is_combat_sensor() || index >= tracked.bodies.size()) continue;
            CombatVolumeBinding binding{id_key, index,
                volume.is_combat_hit() ? CombatVolumeRole::Hit : CombatVolumeRole::Hurt,
                volume.is_combat_hit() ? volume.combat_hit_id : volume.combat_hurt_id};
            combat_registry_.bind(tracked.bodies[index], std::move(binding));
        }
    }
    for (auto it = tracked_.begin(); it != tracked_.end();) {
        if (active.find(it->first) != active.end()) {
            ++it;
            continue;
        }
        remove_bodies(world, it->second);
        it = tracked_.erase(it);
    }
    return Result<void>::success();
}

void PlacementCollisionTracker::clear(CollisionWorld& world) {
    for (auto& entry : tracked_) remove_bodies(world, entry.second);
    tracked_.clear();
    interaction_registry_.clear();
    combat_registry_.clear();
}

} // namespace engine
