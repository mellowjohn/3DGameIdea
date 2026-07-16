#pragma once

#include "engine/assets/material_asset.h"
#include "engine/assets/prefab_asset.h"
#include "engine/assets/mesh_asset.h"
#include "engine/world/components.h"
#include "engine/world/entity_id.h"
#include "engine/world/scene.h"
#include "engine/world/world_partition.h"

#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace engine {

struct ViewportRay {
    WorldPosition origin;
    LocalPosition direction;
};

struct WorldBounds {
    float min_x = 0.0f;
    float min_y = 0.0f;
    float min_z = 0.0f;
    float max_x = 0.0f;
    float max_y = 0.0f;
    float max_z = 0.0f;
};

[[nodiscard]] std::optional<ViewportRay> build_viewport_ray(float image_min_x, float image_min_y, float width,
    float height, const std::array<float, 16>& view, const std::array<float, 16>& projection, float mouse_x,
    float mouse_y);

[[nodiscard]] WorldBounds transform_mesh_bounds(const MeshBounds& local, const TransformComponent& transform);

[[nodiscard]] std::optional<float> ray_aabb_intersection(const ViewportRay& ray, const WorldBounds& bounds);

[[nodiscard]] std::vector<WorldBounds> placement_mesh_bounds(const PrefabAsset& prefab,
    const TransformComponent& placement, const std::map<std::string, MeshBounds>& mesh_bounds,
    const PrefabAsset::MaterialLookup& lookup_material = {});

struct PlacementPickContext {
    const Scene* scene = nullptr;
    const std::map<std::string, PrefabAsset>* prefab_catalog = nullptr;
    const std::map<std::string, MeshBounds>* mesh_bounds = nullptr;
    const std::map<std::string, MaterialAsset>* material_cache = nullptr;
};

[[nodiscard]] std::optional<EntityId> pick_placement_mesh(const PlacementPickContext& context, const ViewportRay& ray);

} // namespace engine
