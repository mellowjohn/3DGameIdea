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
#include <string>
#include <vector>

namespace engine {

[[nodiscard]] Result<std::vector<CollisionBody>> spawn_prefab_collision(CollisionWorld& world, const PrefabAsset& prefab,
    const TransformComponent& placement, CellCoord cell);

class PlacementCollisionTracker final {
public:
    [[nodiscard]] Result<void> sync(CollisionWorld& world, const Scene& scene,
        const std::map<std::string, PrefabAsset>& catalog);
    void clear(CollisionWorld& world);
    [[nodiscard]] const InteractionVolumeRegistry& interaction_registry() const { return interaction_registry_; }
    [[nodiscard]] const CombatVolumeRegistry& combat_registry() const { return combat_registry_; }

private:
    struct TrackedPlacement {
        std::string prefab_path;
        TransformComponent transform;
        CellCoord cell{};
        std::uint64_t components_generation = 0;
        std::vector<CollisionBody> bodies;
    };

    std::map<std::string, TrackedPlacement> tracked_;
    InteractionVolumeRegistry interaction_registry_;
    CombatVolumeRegistry combat_registry_;
    void remove_bodies(CollisionWorld& world, TrackedPlacement& placement);
};

} // namespace engine
