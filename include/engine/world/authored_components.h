#pragma once

#include "engine/assets/prefab_asset.h"
#include "engine/core/result.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace engine {

enum class AuthoredComponentType : std::uint8_t { Collider, ScriptBinding, Animator, Rigidbody };

struct ScriptBindingComponentData {
    std::string kind; // interaction | combatHit | combatHurt | handler
    std::string binding_id;
};

struct AnimatorComponentData {
    std::string controller;      // project-relative *.animator.json
    std::string default_state;   // optional override of layer default
};

struct RigidbodyComponentData {
    std::string motion_type = "dynamic"; // dynamic | kinematic
    float mass = 1.0f;
    float linear_damping = 0.0f;
    float angular_damping = 0.05f;
    bool use_gravity = true;
    bool freeze_rotation = false;
};

struct AuthoredComponentEntry {
    std::string id;
    AuthoredComponentType type = AuthoredComponentType::Collider;
    bool from_prefab = false;
    bool overridden = false;
    PrefabCollisionVolume collider{};
    ScriptBindingComponentData script{};
    AnimatorComponentData animator{};
    RigidbodyComponentData rigidbody{};
};

struct AuthoredComponentsComponent {
    std::vector<AuthoredComponentEntry> entries;
    std::uint64_t generation = 0;
};

[[nodiscard]] std::string authored_component_type_name(AuthoredComponentType type);
[[nodiscard]] std::optional<AuthoredComponentType> parse_authored_component_type(const std::string& value);

[[nodiscard]] AuthoredComponentsComponent seed_authored_components_from_prefab(const PrefabAsset& prefab);
[[nodiscard]] std::vector<PrefabCollisionVolume> effective_collision_volumes(
    const AuthoredComponentsComponent* entity_components, const PrefabAsset* prefab);
/** First Rigidbody on the entity/prefab, if any (DEC-0038 — one motion body per entity). */
[[nodiscard]] std::optional<RigidbodyComponentData> effective_rigidbody(
    const AuthoredComponentsComponent* entity_components, const PrefabAsset* prefab);

[[nodiscard]] Result<void> validate_authored_component_entry(const AuthoredComponentEntry& entry);

// JSON helpers (nlohmann objects as strings for Scene / MCP).
[[nodiscard]] std::string authored_components_to_json(const AuthoredComponentsComponent& components);
[[nodiscard]] Result<AuthoredComponentsComponent> authored_components_from_json(const std::string& json);
[[nodiscard]] std::string authored_component_entry_to_json(const AuthoredComponentEntry& entry);
[[nodiscard]] Result<AuthoredComponentEntry> authored_component_entry_from_json(const std::string& json);

[[nodiscard]] Result<PrefabCollisionVolume> collision_volume_from_json_object(const std::string& json);
[[nodiscard]] std::string collision_volume_to_json_object(const PrefabCollisionVolume& volume);

[[nodiscard]] std::size_t propagate_prefab_components_into_entries(
    AuthoredComponentsComponent& entity_components, const PrefabAsset& prefab);

} // namespace engine
