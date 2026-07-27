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

struct FrustumPlane { float a = 0.0f, b = 0.0f, c = 0.0f, d = 0.0f; };

/** Six world-space frustum planes (left, right, bottom, top, near, far), inward-facing (plane . point >= 0 = inside). */
struct Frustum { std::array<FrustumPlane, 6> planes; };

/**
 * Extract world-space frustum planes from a combined view*projection matrix (DirectXMath row-vector convention,
 * D3D depth range [0, 1]). Used to cull draw calls whose bounds fall entirely outside the current camera view.
 */
[[nodiscard]] Frustum frustum_from_view_projection(const std::array<float, 16>& view_projection);

/**
 * Conservative AABB-vs-frustum test. Returns false only when `bounds` is fully outside at least one plane, so it
 * never culls something that is actually (even partially) visible — safe to skip the draw call when false.
 */
[[nodiscard]] bool frustum_intersects_aabb(const Frustum& frustum, const WorldBounds& bounds);

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
