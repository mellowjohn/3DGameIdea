#include "engine/world/prefab_collision.h"

#include "engine/world/authored_components.h"
#include "engine/world/transform_utils.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
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
    const std::vector<PrefabCollisionVolume>& volumes, const TransformComponent& placement, CellCoord cell,
    bool follow_motion) {
    std::vector<CollisionBody> bodies;
    CollisionBodySettings settings = CollisionBodySettings::make_static();
    if (follow_motion) {
        settings = CollisionBodySettings::make_kinematic();
        settings.use_gravity = false;
        settings.freeze_rotation = true;
    }
    for (const auto& volume : volumes) {
        if (!is_sensor_volume(volume)) continue;
        const auto world_transform = multiply_transforms(placement, volume.transform);
        const WorldPosition position{world_transform.position[0], world_transform.position[1], world_transform.position[2]};
        const CollisionLayer layer = CollisionLayer::Trigger;
        if (volume.shape == PrefabCollisionShape::Box) {
            const auto half = scale_half_extent(volume.half_extent, world_transform.scale);
            const auto body = world.add_box(position, half, layer, settings, cell, world_transform.rotation);
            if (!body) return Result<std::vector<CollisionBody>>::failure(body.error());
            bodies.push_back(body.value());
        } else if (volume.shape == PrefabCollisionShape::Capsule) {
            const auto radius = scale_radius(volume.radius, world_transform.scale);
            const float half_height = volume.capsule_half_height *
                std::max({std::abs(world_transform.scale[0]), std::abs(world_transform.scale[1]), std::abs(world_transform.scale[2])});
            const auto body = world.add_capsule(position, radius, half_height, layer, settings, cell, world_transform.rotation);
            if (!body) return Result<std::vector<CollisionBody>>::failure(body.error());
            bodies.push_back(body.value());
        } else {
            const auto radius = scale_radius(volume.radius, world_transform.scale);
            const auto body = world.add_sphere(position, radius, layer, settings, cell, world_transform.rotation);
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
    placement.follow_sensors.clear();
    placement.motion_body.reset();
    placement.physics_driven = false;
    placement.motion_local = {};
}

Result<void> PlacementCollisionTracker::sync(CollisionWorld& world, const Scene& scene,
    const std::map<std::string, PrefabAsset>& catalog, bool simulate_dynamics, std::size_t max_rebuilds,
    const EntityId* priority_entity) {
    const auto scene_revision = scene.edit_revision();
    // Play-test and idle frames usually leave the authored scene untouched. Physics write-back uses
    // set_transform(..., bump_edit_revision=false), so we can skip the full entity scan most frames.
    if (!sync_incomplete_ && scene_revision == last_scene_edit_revision_ &&
        simulate_dynamics == last_simulate_dynamics_)
        return Result<void>::success();

    sync_incomplete_ = false;
    std::size_t rebuilds_left = max_rebuilds;
    std::set<std::string> active;

    auto rebuild_one = [&](const EntityId& id) -> Result<bool> {
        // Returns true if a body rebuild was performed.
        const auto placement = scene.placement(id);
        const auto transform = scene.transform(id);
        if (!placement || !transform) return Result<bool>::success(false);
        const auto id_key = id.str();
        active.insert(id_key);
        auto tracked_it = tracked_.find(id_key);
        if (tracked_it != tracked_.end()) {
            auto& tracked = tracked_it->second;
            const auto authored = scene.authored_components(id);
            const std::uint64_t generation = authored ? authored->generation : 0;
            const bool transform_matches = tracked.transform.position == transform->position &&
                tracked.transform.rotation == transform->rotation && tracked.transform.scale == transform->scale;
            if (tracked.prefab_path == placement->prefab_asset && tracked.cell == placement->cell &&
                tracked.components_generation == generation && tracked.simulate_dynamics == simulate_dynamics) {
                if (tracked.physics_driven) {
                    // Authored gizmo/MCP/Inspector moves bump the scene while physics-driven
                    // sync used to ignore the new pose. Capsules/hurt sensors stayed at spawn
                    // and write-back snapped the mesh back (or a later save wrote a cell mismatch).
                    if (!transform_matches)
                        teleport_physics_driven(world, tracked, *transform);
                    tracked.entity_id = id;
                    return Result<bool>::success(false);
                }
                if (transform_matches) {
                    tracked.transform = *transform;
                    tracked.entity_id = id;
                    return Result<bool>::success(false);
                }
            }
        }
        if (rebuilds_left == 0) {
            sync_incomplete_ = true;
            return Result<bool>::success(false);
        }
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
            (physics_driven || transform_matches);
        if (unchanged) {
            tracked.entity_id = id;
            if (physics_driven && !transform_matches)
                teleport_physics_driven(world, tracked, *transform);
            else if (!physics_driven)
                tracked.transform = *transform;
            return Result<bool>::success(false);
        }
        --rebuilds_left;
        remove_bodies(world, tracked);
        tracked.entity_id = id;
        tracked.prefab_path = resolved;
        tracked.transform = *transform;
        tracked.cell = placement->cell;
        tracked.components_generation = generation;
        tracked.simulate_dynamics = simulate_dynamics;
        tracked.physics_driven = physics_driven;
        if (volumes.empty()) return Result<bool>::success(true);

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
                if (!motion) return Result<bool>::failure(motion.error());
                tracked.motion_body = motion.value();
                tracked.bodies.push_back(motion.value());
                tracked.motion_local = solid->transform;
                tracked.motion_local.scale = {1.0f, 1.0f, 1.0f};
            }
            const auto sensors = spawn_sensor_volumes(world, volumes, *transform, placement->cell, true);
            if (!sensors) return Result<bool>::failure(sensors.error());
            for (const auto& body : sensors.value()) tracked.bodies.push_back(body);
            tracked.follow_sensors.clear();
            std::uint32_t spawned_sensor = 0;
            for (const auto& volume : volumes) {
                if (!is_sensor_volume(volume)) continue;
                if (spawned_sensor >= sensors.value().size()) break;
                tracked.follow_sensors.push_back(
                    TrackedPlacement::FollowSensor{sensors.value()[spawned_sensor], volume.transform});
                ++spawned_sensor;
            }
        } else {
            const auto spawned = spawn_collision_volumes(world, volumes, *transform, placement->cell);
            if (!spawned) return Result<bool>::failure(spawned.error());
            tracked.bodies = spawned.value();
        }

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
        return Result<bool>::success(true);
    };

    if (priority_entity && !priority_entity->empty()) {
        const auto prioritized = rebuild_one(*priority_entity);
        if (!prioritized) return Result<void>::failure(prioritized.error());
    }

    for (const auto& id : scene.entity_ids()) {
        if (priority_entity && !priority_entity->empty() && id == *priority_entity) continue;
        const auto rebuilt = rebuild_one(id);
        if (!rebuilt) return Result<void>::failure(rebuilt.error());
    }

    if (!sync_incomplete_) {
        for (auto it = tracked_.begin(); it != tracked_.end();) {
            if (active.find(it->first) != active.end()) {
                ++it;
                continue;
            }
            remove_bodies(world, it->second);
            it = tracked_.erase(it);
        }
        last_scene_edit_revision_ = scene_revision;
        last_simulate_dynamics_ = simulate_dynamics;
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
        EntityId entity = tracked.entity_id;
        if (entity.empty() || !scene.contains(entity)) {
            auto parsed = EntityId::parse(id_key);
            if (!parsed || !scene.contains(parsed.value())) continue;
            entity = parsed.value();
            tracked.entity_id = entity;
        }
        if (auto current = scene.transform(entity)) entity_pose.scale = current->scale;
        else entity_pose.scale = tracked.transform.scale;
        tracked.transform = entity_pose;
        // Physics follow must not bump edit_revision or sync would rebuild every frame.
        (void)scene.set_transform(entity, entity_pose, false);
        follow_sensor_transforms(world, tracked);
    }
}

void PlacementCollisionTracker::teleport_physics_driven(
    CollisionWorld& world, TrackedPlacement& tracked, const TransformComponent& entity_pose) {
    tracked.transform = entity_pose;
    if (tracked.motion_body && tracked.motion_body->valid()) {
        const auto world_xf = multiply_transforms(entity_pose, tracked.motion_local);
        const WorldPosition position{world_xf.position[0], world_xf.position[1], world_xf.position[2]};
        (void)world.set_transform(*tracked.motion_body, position, world_xf.rotation);
        (void)world.set_linear_velocity(*tracked.motion_body, {0.0f, 0.0f, 0.0f});
    }
    follow_sensor_transforms(world, tracked);
}

void PlacementCollisionTracker::follow_sensor_transforms(CollisionWorld& world, const TrackedPlacement& tracked) {
    for (const auto& sensor : tracked.follow_sensors) {
        if (!sensor.body.valid()) continue;
        const auto world_xf = multiply_transforms(tracked.transform, sensor.local);
        const WorldPosition position{world_xf.position[0], world_xf.position[1], world_xf.position[2]};
        (void)world.set_transform(sensor.body, position, world_xf.rotation);
    }
}

void PlacementCollisionTracker::clear(CollisionWorld& world) {
    for (auto& entry : tracked_) remove_bodies(world, entry.second);
    tracked_.clear();
    interaction_registry_.clear();
    combat_registry_.clear();
    last_scene_edit_revision_ = std::numeric_limits<std::uint64_t>::max();
    last_simulate_dynamics_ = true;
    sync_incomplete_ = false;
}

std::optional<CollisionBody> PlacementCollisionTracker::motion_body_for(const EntityId& id) const {
    const auto found = tracked_.find(id.str());
    if (found == tracked_.end() || !found->second.motion_body) return std::nullopt;
    return found->second.motion_body;
}

} // namespace engine
