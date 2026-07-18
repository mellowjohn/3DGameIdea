#pragma once

#include "engine/assets/material_asset.h"
#include "engine/assets/mesh_asset.h"
#include "engine/core/result.h"
#include "engine/physics/collision_world.h"
#include "engine/world/components.h"

#include <array>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace engine {

struct PrefabPointLight {
    std::array<float, 3> color{1.0f, 0.62f, 0.28f};
    float radius = 20.0f;
    float strength = 1.35f;
    std::array<float, 3> offset{0.0f, 0.35f, 0.0f};
};

struct PrefabMeshSource {
    std::optional<std::string> primitive;
    std::optional<std::string> asset;
    std::optional<std::string> material;
    std::array<float, 3> color{0.45f, 0.32f, 0.18f};
};

struct PrefabPart {
    std::string name;
    TransformComponent transform;
    PrefabMeshSource mesh;
};

enum class PrefabCollisionShape : std::uint8_t { Box, Sphere, Capsule };

struct PrefabCollisionVolume {
    std::string id;
    PrefabCollisionShape shape = PrefabCollisionShape::Box;
    CollisionLayer layer = CollisionLayer::StaticWorld;
    bool trigger = false;
    std::string interaction_id;
    std::string combat_hit_id;
    std::string combat_hurt_id;
    TransformComponent transform;
    LocalPosition half_extent{0.5f, 0.5f, 0.5f};
    float radius = 0.5f;
    /** Cylinder half-height for Capsule (Jolt CapsuleShape); total height = 2*(halfHeight+radius). */
    float capsule_half_height = 0.85f;

    [[nodiscard]] bool is_interaction() const { return !interaction_id.empty(); }
    [[nodiscard]] bool is_combat_hit() const { return !combat_hit_id.empty(); }
    [[nodiscard]] bool is_combat_hurt() const { return !combat_hurt_id.empty(); }
    [[nodiscard]] bool is_combat_sensor() const { return is_combat_hit() || is_combat_hurt(); }
};

struct PrefabScriptBinding {
    std::string id;
    std::string kind; // interaction | combatHit | combatHurt | handler
    std::string binding_id;
};

struct PrefabAnimator {
    std::string id;
    std::string controller;    // project-relative *.animator.json
    std::string default_state; // optional
};

struct PrefabRigidbody {
    std::string id;
    std::string motion_type = "dynamic"; // dynamic | kinematic
    float mass = 1.0f;
    float linear_damping = 0.0f;
    float angular_damping = 0.05f;
    bool use_gravity = true;
    bool freeze_rotation = false;
};

struct PrefabAsset {
    int schema_version = 1;
    std::string mesh;
    std::vector<PrefabPart> parts;
    std::vector<PrefabCollisionVolume> collision;
    std::vector<PrefabScriptBinding> script_bindings;
    std::vector<PrefabAnimator> animators;
    std::vector<PrefabRigidbody> rigidbodies;
    std::optional<PrefabPointLight> light;
    std::optional<std::string> character_asset;

    [[nodiscard]] bool is_compositional() const { return schema_version >= 2 && !parts.empty(); }

    [[nodiscard]] static Result<PrefabAsset> load(const std::filesystem::path& path);
    [[nodiscard]] Result<void> save(const std::filesystem::path& path) const;
    using MaterialLookup = std::function<const MaterialAsset*(const std::string& normalized_path)>;

    [[nodiscard]] std::string mesh_key_for_part(const PrefabPart& part, const MaterialLookup& lookup_material = {}) const;
    [[nodiscard]] std::vector<std::string> required_mesh_keys(const MaterialLookup& lookup_material = {}) const;
    [[nodiscard]] MeshBounds bounds(const std::map<std::string, MeshBounds>& mesh_bounds,
        const MaterialLookup& lookup_material = {}) const;
};

[[nodiscard]] std::string primitive_mesh_cache_key(const std::string& primitive, const std::array<float, 3>& color);
[[nodiscard]] std::array<float, 3> resolved_prefab_mesh_color(const PrefabMeshSource& mesh,
    const PrefabAsset::MaterialLookup& lookup_material);
[[nodiscard]] PrefabAsset::MaterialLookup make_material_lookup(const std::map<std::string, MaterialAsset>* materials);

[[nodiscard]] std::string resolve_prefab_catalog_path(const std::map<std::string, PrefabAsset>& catalog,
    const std::string& prefab_asset);
[[nodiscard]] const PrefabAsset* find_prefab_in_catalog(const std::map<std::string, PrefabAsset>& catalog,
    const std::string& prefab_asset);

} // namespace engine
