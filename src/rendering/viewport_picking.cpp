#include "engine/rendering/viewport_picking.h"

#include "engine/world/transform_utils.h"

#include <DirectXMath.h>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <limits>

namespace engine {
namespace {

void expand_world_bounds(WorldBounds& bounds, float x, float y, float z) {
    bounds.min_x = std::min(bounds.min_x, x);
    bounds.min_y = std::min(bounds.min_y, y);
    bounds.min_z = std::min(bounds.min_z, z);
    bounds.max_x = std::max(bounds.max_x, x);
    bounds.max_y = std::max(bounds.max_y, y);
    bounds.max_z = std::max(bounds.max_z, z);
}

std::string normalize_asset_path(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    std::transform(path.begin(), path.end(), path.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return path;
}

} // namespace

std::optional<ViewportRay> build_viewport_ray(float image_min_x, float image_min_y, float width, float height,
    const std::array<float, 16>& view, const std::array<float, 16>& projection, float mouse_x, float mouse_y) {
    using namespace DirectX;
    const auto projection_matrix = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(projection.data()));
    const auto view_matrix = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(view.data()));
    const auto identity = XMMatrixIdentity();
    const auto near_point = XMVector3Unproject(
        XMVectorSet(mouse_x - image_min_x, mouse_y - image_min_y, 0.0f, 1.0f), 0.0f, 0.0f, width, height, 0.0f, 1.0f,
        projection_matrix, view_matrix, identity);
    const auto far_point = XMVector3Unproject(
        XMVectorSet(mouse_x - image_min_x, mouse_y - image_min_y, 1.0f, 1.0f), 0.0f, 0.0f, width, height, 0.0f, 1.0f,
        projection_matrix, view_matrix, identity);
    const auto direction = XMVectorSubtract(far_point, near_point);
    const float length = XMVectorGetX(XMVector3Length(direction));
    if (!(length > 0.0f)) return std::nullopt;
    const auto normalized = XMVectorScale(direction, 1.0f / length);
    ViewportRay ray;
    ray.origin = {XMVectorGetX(near_point), XMVectorGetY(near_point), XMVectorGetZ(near_point)};
    ray.direction = {XMVectorGetX(normalized), XMVectorGetY(normalized), XMVectorGetZ(normalized)};
    return ray;
}

WorldBounds transform_mesh_bounds(const MeshBounds& local, const TransformComponent& transform) {
    const float corners[8][3] = {
        {local.min_x, local.min_y, local.min_z}, {local.max_x, local.min_y, local.min_z},
        {local.max_x, local.max_y, local.min_z}, {local.min_x, local.max_y, local.min_z},
        {local.min_x, local.min_y, local.max_z}, {local.max_x, local.min_y, local.max_z},
        {local.max_x, local.max_y, local.max_z}, {local.min_x, local.max_y, local.max_z},
    };
    WorldBounds world{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest(),
                      std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
    using namespace DirectX;
    const auto matrix =
        XMMatrixScaling(transform.scale[0], transform.scale[1], transform.scale[2]) *
        XMMatrixRotationQuaternion(XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(transform.rotation.data()))) *
        XMMatrixTranslation(transform.position[0], transform.position[1], transform.position[2]);
    for (const auto& corner : corners) {
        const auto world_corner = XMVector3TransformCoord(XMVectorSet(corner[0], corner[1], corner[2], 1.0f), matrix);
        expand_world_bounds(world, XMVectorGetX(world_corner), XMVectorGetY(world_corner), XMVectorGetZ(world_corner));
    }
    return world;
}

std::optional<float> ray_aabb_intersection(const ViewportRay& ray, const WorldBounds& bounds) {
    float t_min = 0.0f;
    float t_max = std::numeric_limits<float>::max();
    const float origin[3] = {static_cast<float>(ray.origin.x), static_cast<float>(ray.origin.y),
                             static_cast<float>(ray.origin.z)};
    const float direction[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
    const float mins[3] = {bounds.min_x, bounds.min_y, bounds.min_z};
    const float maxs[3] = {bounds.max_x, bounds.max_y, bounds.max_z};
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(direction[axis]) < 1.0e-8f) {
            if (origin[axis] < mins[axis] || origin[axis] > maxs[axis]) return std::nullopt;
            continue;
        }
        const float inv = 1.0f / direction[axis];
        float t0 = (mins[axis] - origin[axis]) * inv;
        float t1 = (maxs[axis] - origin[axis]) * inv;
        if (t0 > t1) std::swap(t0, t1);
        t_min = std::max(t_min, t0);
        t_max = std::min(t_max, t1);
        if (t_min > t_max) return std::nullopt;
    }
    if (t_max < 0.0f) return std::nullopt;
    return t_min >= 0.0f ? t_min : t_max;
}

Frustum frustum_from_view_projection(const std::array<float, 16>& view_projection) {
    // Row-vector convention (v' = v * M): clip.x/y/z/w = v . column(M). Frustum planes are the classic
    // Gribb-Hartmann combinations of those columns; D3D near/far use depth range [0, 1].
    const float* m = view_projection.data();
    const auto column = [&](int index) {
        return std::array<float, 4>{m[index], m[4 + index], m[8 + index], m[12 + index]};
    };
    const auto add = [](const std::array<float, 4>& x, const std::array<float, 4>& y) {
        return std::array<float, 4>{x[0] + y[0], x[1] + y[1], x[2] + y[2], x[3] + y[3]};
    };
    const auto subtract = [](const std::array<float, 4>& x, const std::array<float, 4>& y) {
        return std::array<float, 4>{x[0] - y[0], x[1] - y[1], x[2] - y[2], x[3] - y[3]};
    };
    const auto to_plane = [](const std::array<float, 4>& v) { return FrustumPlane{v[0], v[1], v[2], v[3]}; };
    const auto c0 = column(0), c1 = column(1), c2 = column(2), c3 = column(3);
    Frustum frustum;
    frustum.planes[0] = to_plane(add(c3, c0));      // left
    frustum.planes[1] = to_plane(subtract(c3, c0)); // right
    frustum.planes[2] = to_plane(add(c3, c1));      // bottom
    frustum.planes[3] = to_plane(subtract(c3, c1)); // top
    frustum.planes[4] = to_plane(c2);               // near
    frustum.planes[5] = to_plane(subtract(c3, c2)); // far
    return frustum;
}

bool frustum_intersects_aabb(const Frustum& frustum, const WorldBounds& bounds) {
    for (const auto& plane : frustum.planes) {
        // Test the AABB corner most aligned with the plane normal; if even that corner is outside, the whole
        // box is outside (safe/conservative: never rejects a box that is still partially visible).
        const float px = plane.a >= 0.0f ? bounds.max_x : bounds.min_x;
        const float py = plane.b >= 0.0f ? bounds.max_y : bounds.min_y;
        const float pz = plane.c >= 0.0f ? bounds.max_z : bounds.min_z;
        if (plane.a * px + plane.b * py + plane.c * pz + plane.d < 0.0f) return false;
    }
    return true;
}

std::vector<WorldBounds> placement_mesh_bounds(const PrefabAsset& prefab, const TransformComponent& placement,
    const std::map<std::string, MeshBounds>& mesh_bounds, const PrefabAsset::MaterialLookup& lookup_material) {
    std::vector<WorldBounds> bounds;
    if (!prefab.is_compositional()) {
        if (prefab.mesh.empty()) return bounds;
        const auto found = mesh_bounds.find(normalize_asset_path(prefab.mesh));
        if (found == mesh_bounds.end()) return bounds;
        bounds.push_back(transform_mesh_bounds(found->second, placement));
        return bounds;
    }
    bounds.reserve(prefab.parts.size());
    for (const auto& part : prefab.parts) {
        const auto key = prefab.mesh_key_for_part(part, lookup_material);
        const auto found = mesh_bounds.find(key);
        if (found == mesh_bounds.end()) continue;
        bounds.push_back(transform_mesh_bounds(found->second, multiply_transforms(placement, part.transform)));
    }
    return bounds;
}

std::optional<EntityId> pick_placement_mesh(const PlacementPickContext& context, const ViewportRay& ray) {
    if (!context.scene || !context.prefab_catalog || !context.mesh_bounds) return std::nullopt;
    const auto lookup_material = make_material_lookup(context.material_cache);
    std::optional<EntityId> best_id;
    float best_fraction = std::numeric_limits<float>::max();
    for (const auto& id : context.scene->entity_ids()) {
        const auto placement = context.scene->placement(id);
        const auto transform = context.scene->transform(id);
        if (!placement || !transform) continue;
        const auto normalized = normalize_asset_path(placement->prefab_asset);
        const auto prefab = context.prefab_catalog->find(normalized);
        if (prefab == context.prefab_catalog->end()) continue;
        const auto part_bounds = placement_mesh_bounds(prefab->second, *transform, *context.mesh_bounds, lookup_material);
        if (part_bounds.empty()) continue;
        float closest = std::numeric_limits<float>::max();
        for (const auto& bounds : part_bounds) {
            if (const auto hit = ray_aabb_intersection(ray, bounds)) closest = std::min(closest, *hit);
        }
        if (closest < best_fraction) {
            best_fraction = closest;
            best_id = id;
        }
    }
    return best_id;
}

} // namespace engine
