#pragma once

#include "engine/assets/character_asset.h"
#include "engine/world/entity_id.h"
#include "engine/world/world_partition.h"

#include <array>
#include <optional>
#include <string>

namespace engine {

struct IdComponent { EntityId id; };
struct NameComponent { std::string name; };
struct TransformComponent {
    std::array<float, 3> position{0.0f, 0.0f, 0.0f};
    std::array<float, 4> rotation{0.0f, 0.0f, 0.0f, 1.0f};
    std::array<float, 3> scale{1.0f, 1.0f, 1.0f};
};
struct HierarchyComponent { std::optional<EntityId> parent; };
struct WorldPlacementComponent {
    std::string prefab_asset;
    CellCoord cell;
    std::optional<std::string> character_asset;
    std::optional<CharacterAsset> character_settings;
};

} // namespace engine
