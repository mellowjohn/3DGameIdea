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

bool is_sensor_volume(const PrefabCollisionVolume& volume) {
    return volume.trigger || volume.is_interaction() || volume.is_combat_sensor();
}

CollisionBodySettings settings_from_rigidbody(const RigidbodyComponentData& rigidbody, bool simulate_dynamics) {
    CollisionBodySettings settings;
    const auto motion = rigidbody.motion_type;
    const bool want_dynamic = motion == "dynamic";
    if (!simulate_dynamics && want_dynamic) settings = CollisionBodySettings::make_kinematic();
    else if (want_dynamic) settings = CollisionBodySettings::make_dynamic();
    else if (motion == "kinematic") settings = CollisionBodySettings::make_kinematic();
    else settings = CollisionBodySettings::make_static();
    settings.mass = rigidbody.mass;
    settings.linear_damping = rigidbody.linear_damping;
    settings.angular_damping = rigidbody.angular_damping;
    settings.use_gravity = rigidbody.use_gravity;
    settings.freeze_rotation = rigidbody.freeze_rotation;
    return settings;
}

Result<std::vector<CollisionBody>> spawn_sensor_volumes(CollisionWorld& world,
    const std::vector<PrefabCollisionVolume>& volumes, const TransformComponent& placement, CellCoord cell) {
    std::vector<CollisionBody> bodies;
    for (const auto& volume : volumes) {
        if (!is_sensor_volume(volume)) continue;
        const auto world_transform = multiply_transforms(placement, volume.transform);
        const WorldPosition position{world_transform.position[0], world_transform.position[1], world_transform.position[2]};
        const CollisionLayer layer = CollisionLayer::Trigger;
        if (volume.shape == PrefabCollisionShape::Box) {
            const auto half = scale_half_extent(volume.half_extent, world_transform.scale);
            const auto body = world.add_box(position, half, layer, false, cell, world_transform.rotation);
            if (!body) return Result<std::vector<CollisionBody>>::failure(body.error());
            bodies.push_back(body.value());
        } else if (volume.shape == PrefabCollisionShape::Capsule) {
            const auto radius = scale_radius(volume.radius, world_transform.scale);
            const float half_height = volume.capsule_half_height *
                std::max({std::abs(world_transform.scale[0]), std::abs(world_transform.scale[1]), std::abs(world_transform.scale[2])});
            const auto body = world.add_capsule(position, radius, half_height, layer, false, cell, world_transform.rotation);
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

Result<std::vector<CollisionBody>> spawn_static_solid_volumes(CollisionWorld& world,
    const std::vector<PrefabCollisionVolume>& volumes, const TransformComponent& placement, CellCoord cell) {
    std::vector<CollisionBody> bodies;
    for (const auto& volume : volumes) {
        if (is_sensor_volume(volume)) continue;
        const auto world_transform = multiply_transforms(placement, volume.transform);
        const WorldPosition position{world_transform.position[0], world_transform.position[1], world_transform.position[2]};
        const CollisionLayer layer = volume.layer;
        if (volume.shape == PrefabCollisionShape::Box) {
            const auto half = scale_half_extent(volume.half_extent, world_transform.scale);
            const auto body = world.add_box(position, half, layer, false, cell, world_transform.rotation);
            if (!body) return Result<std::vector<CollisionBody>>::failure(body.error());
            bodies.push_back(body.value());
        } else if (volume.shape == PrefabCollisionShape::Capsule) {
            const auto radius = scale_radius(volume.radius, world_transform.scale);
            const float half_height = volume.capsule_half_height *
                std::max({std::abs(world_transform.scale[0]), std::abs(world_transform.scale[1]), std::abs(world_transform.scale[2])});
            const auto body = world.add_capsule(position, radius, half_height, layer, false, cell, world_transform.rotation);
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

/** One motion body from the first solid collider (multi-solid compound deferred). */
Result<CollisionBody> spawn_motion_body(CollisionWorld& world, const PrefabCollisionVolume& volume,
    const TransformComponent& placement, CellCoord cell, const CollisionBodySettings& settings) {
    const auto world_transform = multiply_transforms(placement, volume.transform);
    const WorldPosition position{world_transform.position[0], world_transform.position[1], world_transform.position[2]};
    const CollisionLayer layer = CollisionLayer::Dynamic;
    if (volume.shape == PrefabCollisionShape::Box) {
        const auto half = scale_half_extent(volume.half_extent, world_transform.scale);
        return world.add_box(position, half, layer, settings, cell, world_transform.rotation);
    }
    if (volume.shape == PrefabCollisionShape::Capsule) {
        const auto radius = scale_radius(volume.radius, world_transform.scale);
        const float half_height = volume.capsule_half_height *
            std::max({std::abs(world_transform.scale[0]), std::abs(world_transform.scale[1]), std::abs(world_transform.scale[2])});
        return world.add_capsule(position, radius, half_height, layer, settings, cell, world_transform.rotation);
    }
    const auto radius = scale_radius(volume.radius, world_transform.scale);
    return world.add_sphere(position, radius, layer, settings, cell, world_transform.rotation);
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
        } else if (volume.shape == PrefabCollisionShape::Capsule) {
            const auto radius = scale_radius(volume.radius, world_transform.scale);
            const float half_height = volume.capsule_half_height *
                std::max({std::abs(world_transform.scale[0]), std::abs(world_transform.scale[1]), std::abs(world_transform.scale[2])});
            const auto body = world.add_capsule(position, radius, half_height, layer, false, cell, world_transform.rotation);
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
    placement.motion_body.reset();
    placement.physics_driven = false;
    placement.motion_local = {};
}

Result<void> PlacementCollisionTracker::sync(CollisionWorld& world, const Scene& scene,
    const std::map<std::string, PrefabAsset>& catalog, bool simulate_dynamics) {
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
        const auto rigidbody = effective_rigidbody(authored ? &*authored : nullptr, prefab);
        auto& tracked = tracked_[id_key];
        const std::uint64_t generation = authored ? authored->generation : 0;
        const bool physics_driven = rigidbody.has_value();
        const bool transform_matches = tracked.transform.position == transform->position &&
            tracked.transform.rotation == transform->rotation && tracked.transform.scale == transform->scale;
        const bool unchanged = tracked.prefab_path == resolved && tracked.cell == placement->cell &&
            tracked.components_generation == generation && tracked.simulate_dynamics == simulate_dynamics &&
            tracked.physics_driven == physics_driven &&
            (physics_driven ? true : transform_matches);
        if (unchanged) {
            if (!physics_driven) tracked.transform = *transform;
            continue;
        }
        remove_bodies(world, tracked);
        tracked.prefab_path = resolved;
        tracked.transform = *transform;
        tracked.cell = placement->cell;
        tracked.components_generation = generation;
        tracked.simulate_dynamics = simulate_dynamics;
        tracked.physics_driven = physics_driven;
        if (volumes.empty()) continue;

        if (physics_driven) {
            const PrefabCollisionVolume* solid = nullptr;
            for (const auto& volume : volumes) {
                if (!is_sensor_volume(volume)) {
                    solid = &volume;
                    break;
                }
            }
            if (solid) {
                const auto settings = settings_from_rigidbody(*rigidbody, simulate_dynamics);
                const auto motion = spawn_motion_body(world, *solid, *transform, placement->cell, settings);
                if (!motion) return Result<void>::failure(motion.error());
                tracked.motion_body = motion.value();
                tracked.bodies.push_back(motion.value());
                tracked.motion_local = solid->transform;
                tracked.motion_local.scale = {1.0f, 1.0f, 1.0f};
            }
            const auto sensors = spawn_sensor_volumes(world, volumes, *transform, placement->cell);
            if (!sensors) return Result<void>::failure(sensors.error());
            for (const auto& body : sensors.value()) tracked.bodies.push_back(body);
        } else {
            const auto spawned = spawn_collision_volumes(world, volumes, *transform, placement->cell);
            if (!spawned) return Result<void>::failure(spawned.error());
            tracked.bodies = spawned.value();
        }

        // Bind interaction/combat to sensor bodies — index by volume order among sensors for physics-driven,
        // or by full volume list for static path.
        if (physics_driven) {
            std::uint32_t sensor_index = 0;
            const std::uint32_t motion_count = tracked.motion_body ? 1u : 0u;
            for (std::uint32_t index = 0; index < volumes.size(); ++index) {
                const auto& volume = volumes[index];
                if (!is_sensor_volume(volume)) continue;
                const auto body_index = motion_count + sensor_index;
                ++sensor_index;
                if (body_index >= tracked.bodies.size()) continue;
                if (volume.is_interaction())
                    interaction_registry_.bind(tracked.bodies[body_index],
                        InteractionVolumeBinding{id_key, index, volume.interaction_id});
                if (volume.is_combat_sensor()) {
                    CombatVolumeBinding binding{id_key, index,
                        volume.is_combat_hit() ? CombatVolumeRole::Hit : CombatVolumeRole::Hurt,
                        volume.is_combat_hit() ? volume.combat_hit_id : volume.combat_hurt_id};
                    combat_registry_.bind(tracked.bodies[body_index], std::move(binding));
                }
            }
        } else {
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

void PlacementCollisionTracker::write_back_transforms(Scene& scene, CollisionWorld& world) {
    for (auto& [id_key, tracked] : tracked_) {
        if (!tracked.physics_driven || !tracked.motion_body) continue;
        const auto position = world.position(*tracked.motion_body);
        const auto rotation = world.rotation(*tracked.motion_body);
        if (!position || !rotation) continue;
        TransformComponent body_pose;
        body_pose.position = {static_cast<float>(position.value().x), static_cast<float>(position.value().y),
            static_cast<float>(position.value().z)};
        body_pose.rotation = rotation.value();
        body_pose.scale = {1.0f, 1.0f, 1.0f};
        TransformComponent entity_pose = multiply_transforms(body_pose, inverse_transform(tracked.motion_local));
        for (const auto& entity : scene.entity_ids()) {
            if (entity.str() != id_key) continue;
            if (auto current = scene.transform(entity)) entity_pose.scale = current->scale;
            else entity_pose.scale = tracked.transform.scale;
            tracked.transform = entity_pose;
            (void)scene.set_transform(entity, entity_pose);
            break;
        }
    }
}

void PlacementCollisionTracker::clear(CollisionWorld& world) {
    for (auto& entry : tracked_) remove_bodies(world, entry.second);
    tracked_.clear();
    interaction_registry_.clear();
    combat_registry_.clear();
}

std::optional<CollisionBody> PlacementCollisionTracker::motion_body_for(const EntityId& id) const {
    const auto found = tracked_.find(id.str());
    if (found == tracked_.end() || !found->second.motion_body) return std::nullopt;
    return found->second.motion_body;
}

} // namespace engine
