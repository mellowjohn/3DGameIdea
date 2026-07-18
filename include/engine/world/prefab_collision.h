#pragma once

#include "engine/assets/prefab_asset.h"
#include "engine/physics/collision_world.h"
#include "engine/world/components.h"
#include "engine/world/interaction_volumes.h"
#include "engine/world/combat_volumes.h"
#include "engine/world/entity_id.h"
#include "engine/world/scene.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace engine {

[[nodiscard]] Result<std::vector<CollisionBody>> spawn_prefab_collision(CollisionWorld& world, const PrefabAsset& prefab,
    const TransformComponent& placement, CellCoord cell);

class PlacementCollisionTracker final {
public:
    /**
     * @param simulate_dynamics When false (editor idle), authored dynamic Rigidbodies spawn as kinematic so crates
     * do not fall during edit. When true (play/test running or runtime), use authored motion type.
     */
    [[nodiscard]] Result<void> sync(CollisionWorld& world, const Scene& scene,
        const std::map<std::string, PrefabAsset>& catalog, bool simulate_dynamics = true);
    /** Copy motion-body poses into scene transforms for physics-driven placements. */
    void write_back_transforms(Scene& scene, CollisionWorld& world);
    void clear(CollisionWorld& world);
    [[nodiscard]] std::optional<CollisionBody> motion_body_for(const EntityId& id) const;
    [[nodiscard]] const InteractionVolumeRegistry& interaction_registry() const { return interaction_registry_; }
    [[nodiscard]] const CombatVolumeRegistry& combat_registry() const { return combat_registry_; }

private:
    struct TrackedPlacement {
        std::string prefab_path;
        TransformComponent transform;
        CellCoord cell{};
        std::uint64_t components_generation = 0;
        bool simulate_dynamics = true;
        bool physics_driven = false;
        TransformComponent motion_local{}; // solid collider local TR used for write-back (scale baked into shape)
        std::optional<CollisionBody> motion_body;
        std::vector<CollisionBody> bodies; // sensors + motion (motion also in motion_body)
    };

    std::map<std::string, TrackedPlacement> tracked_;
    InteractionVolumeRegistry interaction_registry_;
    CombatVolumeRegistry combat_registry_;
    void remove_bodies(CollisionWorld& world, TrackedPlacement& placement);
};

} // namespace engine
