#include "engine/assets/prefab_asset.h"

#include "engine/assets/material_asset.h"
#include "engine/assets/mesh_asset.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>

namespace engine {
namespace {

EngineError prefab_error(std::string code, std::string message) {
    return {std::move(code), Severity::Error, ErrorCategory::Serialization, "prefab-asset", std::move(message),
            ENGINE_SOURCE_CONTEXT, {}, "Use compositional parts or a project-relative mesh path.", make_correlation_id()};
}

std::array<float, 3> read_vec3(const nlohmann::json& value) {
    return {value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>()};
}

TransformComponent read_transform(const nlohmann::json& value) {
    TransformComponent transform;
    if (value.contains("position")) transform.position = read_vec3(value["position"]);
    if (value.contains("rotation")) transform.rotation = {value["rotation"].at(0).get<float>(),
                                                          value["rotation"].at(1).get<float>(),
                                                          value["rotation"].at(2).get<float>(),
                                                          value["rotation"].at(3).get<float>()};
    if (value.contains("scale")) transform.scale = read_vec3(value["scale"]);
    return transform;
}

nlohmann::json write_vec3(const std::array<float, 3>& value) {
    return nlohmann::json::array({value[0], value[1], value[2]});
}

nlohmann::json write_transform(const TransformComponent& transform) {
    return {{"position", write_vec3(transform.position)},
            {"rotation", nlohmann::json::array(
                             {transform.rotation[0], transform.rotation[1], transform.rotation[2], transform.rotation[3]})},
            {"scale", write_vec3(transform.scale)}};
}

std::string normalize_primitive_name(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

bool is_supported_primitive(const std::string& primitive) {
    return primitive == "cube" || primitive == "pyramid" || primitive == "cylinder" || primitive == "sphere" ||
           primitive == "capsule";
}

Result<PrefabMeshSource> read_mesh_source(const nlohmann::json& value) {
    PrefabMeshSource source;
    if (value.contains("primitive")) {
        source.primitive = normalize_primitive_name(value["primitive"].get<std::string>());
        if (!is_supported_primitive(*source.primitive))
            return Result<PrefabMeshSource>::failure(
                prefab_error("PREFAB-PRIMITIVE-INVALID", "Unsupported primitive: " + *source.primitive));
    }
    if (value.contains("asset")) source.asset = value["asset"].get<std::string>();
    if (value.contains("material") && value["material"].is_string())
        source.material = value["material"].get<std::string>();
    if (value.contains("color")) source.color = read_vec3(value["color"]);
    if (!source.primitive && !source.asset)
        return Result<PrefabMeshSource>::failure(
            prefab_error("PREFAB-MESH-INVALID", "Mesh block requires primitive or asset"));
    if (source.primitive && source.asset)
        return Result<PrefabMeshSource>::failure(
            prefab_error("PREFAB-MESH-INVALID", "Mesh block cannot contain both primitive and asset"));
    for (float channel : source.color) {
        if (!std::isfinite(channel) || channel < 0.0f)
            return Result<PrefabMeshSource>::failure(
                prefab_error("PREFAB-MESH-INVALID", "Mesh color channels must be finite and non-negative"));
    }
    return Result<PrefabMeshSource>::success(std::move(source));
}

Result<PrefabPointLight> read_light(const nlohmann::json& light) {
    if (!light.is_object()) return Result<PrefabPointLight>::failure(prefab_error("PREFAB-LIGHT-INVALID", "Prefab light must be an object"));
    PrefabPointLight point{};
    if (light.contains("color")) point.color = read_vec3(light["color"]);
    if (light.contains("radius")) point.radius = light["radius"].get<float>();
    if (light.contains("strength")) point.strength = light["strength"].get<float>();
    if (light.contains("offset")) point.offset = read_vec3(light["offset"]);
    if (point.radius <= 0.0f || point.strength <= 0.0f || !std::isfinite(point.radius) || !std::isfinite(point.strength))
        return Result<PrefabPointLight>::failure(
            prefab_error("PREFAB-LIGHT-INVALID", "Prefab light radius and strength must be finite and positive"));
    for (float channel : point.color) {
        if (!std::isfinite(channel) || channel < 0.0f)
            return Result<PrefabPointLight>::failure(
                prefab_error("PREFAB-LIGHT-INVALID", "Prefab light color channels must be finite and non-negative"));
    }
    return Result<PrefabPointLight>::success(point);
}

Result<CollisionLayer> read_collision_layer(const std::string& value) {
    const auto normalized = normalize_primitive_name(value);
    if (normalized == "staticworld") return Result<CollisionLayer>::success(CollisionLayer::StaticWorld);
    if (normalized == "dynamic") return Result<CollisionLayer>::success(CollisionLayer::Dynamic);
    if (normalized == "character") return Result<CollisionLayer>::success(CollisionLayer::Character);
    if (normalized == "trigger") return Result<CollisionLayer>::success(CollisionLayer::Trigger);
    return Result<CollisionLayer>::failure(prefab_error("PREFAB-COLLISION-LAYER-INVALID", "Unsupported collision layer: " + value));
}

Result<PrefabCollisionVolume> read_collision_volume(const nlohmann::json& value) {
    if (!value.is_object())
        return Result<PrefabCollisionVolume>::failure(
            prefab_error("PREFAB-COLLISION-INVALID", "Collision volume must be an object"));
    PrefabCollisionVolume volume;
    volume.id = value.value("id", std::string{});
    const auto shape = normalize_primitive_name(value.value("shape", std::string{"box"}));
    if (shape == "box") volume.shape = PrefabCollisionShape::Box;
    else if (shape == "sphere") volume.shape = PrefabCollisionShape::Sphere;
    else if (shape == "capsule") volume.shape = PrefabCollisionShape::Capsule;
    else
        return Result<PrefabCollisionVolume>::failure(
            prefab_error("PREFAB-COLLISION-SHAPE-INVALID", "Unsupported collision shape: " + shape));
    if (value.contains("layer")) {
        const auto layer = read_collision_layer(value["layer"].get<std::string>());
        if (!layer) return Result<PrefabCollisionVolume>::failure(layer.error());
        volume.layer = layer.value();
    }
    if (value.contains("trigger")) volume.trigger = value["trigger"].get<bool>();
    if (value.contains("interaction")) volume.interaction_id = value["interaction"].get<std::string>();
    if (value.contains("combatHit")) volume.combat_hit_id = value["combatHit"].get<std::string>();
    if (value.contains("combatHurt")) volume.combat_hurt_id = value["combatHurt"].get<std::string>();
    if (value.contains("transform")) volume.transform = read_transform(value["transform"]);
    if (value.contains("halfExtent")) {
        const auto extent = read_vec3(value["halfExtent"]);
        volume.half_extent = {extent[0], extent[1], extent[2]};
    }
    if (value.contains("radius")) volume.radius = value["radius"].get<float>();
    if (value.contains("halfHeight")) volume.capsule_half_height = value["halfHeight"].get<float>();
    if (volume.trigger) volume.layer = CollisionLayer::Trigger;
    if (!volume.interaction_id.empty()) {
        volume.trigger = true;
        volume.layer = CollisionLayer::Trigger;
    }
    if (!volume.combat_hit_id.empty() && !volume.combat_hurt_id.empty()) {
        return Result<PrefabCollisionVolume>::failure(
            prefab_error("PREFAB-COLLISION-INVALID", "Collision volume cannot set both combatHit and combatHurt"));
    }
    if (volume.is_combat_sensor()) {
        volume.trigger = true;
        volume.layer = CollisionLayer::Trigger;
    }
    if (volume.shape == PrefabCollisionShape::Box) {
        if (volume.half_extent.x <= 0 || volume.half_extent.y <= 0 || volume.half_extent.z <= 0)
            return Result<PrefabCollisionVolume>::failure(
                prefab_error("PREFAB-COLLISION-INVALID", "Box halfExtent must be positive on every axis"));
    } else if (volume.shape == PrefabCollisionShape::Capsule) {
        if (!(volume.radius > 0) || !(volume.capsule_half_height > 0))
            return Result<PrefabCollisionVolume>::failure(
                prefab_error("PREFAB-COLLISION-INVALID", "Capsule radius and halfHeight must be positive"));
    } else if (!(volume.radius > 0)) {
        return Result<PrefabCollisionVolume>::failure(
            prefab_error("PREFAB-COLLISION-INVALID", "Sphere radius must be positive"));
    }
    for (float channel : volume.transform.scale) {
        if (!std::isfinite(channel) || channel <= 0.0f)
            return Result<PrefabCollisionVolume>::failure(
                prefab_error("PREFAB-COLLISION-INVALID", "Collision transform scale must be finite and positive"));
    }
    return Result<PrefabCollisionVolume>::success(std::move(volume));
}

Result<PrefabScriptBinding> read_script_binding_component(const nlohmann::json& value) {
    if (!value.is_object())
        return Result<PrefabScriptBinding>::failure(
            prefab_error("PREFAB-COMPONENT-INVALID", "Component entry must be an object"));
    PrefabScriptBinding binding;
    binding.id = value.value("id", std::string{});
    const auto type = normalize_primitive_name(value.value("type", std::string{}));
    if (type != "scriptbinding" && type != "script")
        return Result<PrefabScriptBinding>::failure(
            prefab_error("PREFAB-COMPONENT-TYPE", "Unsupported prefab component type (expected scriptBinding)"));
    const auto& data = value.contains("data") ? value.at("data") : value;
    binding.kind = data.value("kind", std::string{});
    binding.binding_id = data.value("bindingId", data.value("binding_id", std::string{}));
    if (binding.kind.empty() || binding.binding_id.empty())
        return Result<PrefabScriptBinding>::failure(
            prefab_error("PREFAB-COMPONENT-INVALID", "scriptBinding requires kind and bindingId"));
    return Result<PrefabScriptBinding>::success(std::move(binding));
}

Result<PrefabAnimator> read_animator_component(const nlohmann::json& value) {
    if (!value.is_object())
        return Result<PrefabAnimator>::failure(
            prefab_error("PREFAB-COMPONENT-INVALID", "Component entry must be an object"));
    PrefabAnimator animator;
    animator.id = value.value("id", std::string{});
    const auto type = normalize_primitive_name(value.value("type", std::string{}));
    if (type != "animator")
        return Result<PrefabAnimator>::failure(
            prefab_error("PREFAB-COMPONENT-TYPE", "Unsupported prefab component type (expected animator)"));
    const auto& data = value.contains("data") ? value.at("data") : value;
    animator.controller = data.value("controller", std::string{});
    animator.default_state = data.value("defaultState", data.value("default_state", std::string{}));
    if (animator.controller.empty())
        return Result<PrefabAnimator>::failure(
            prefab_error("PREFAB-COMPONENT-INVALID", "animator requires controller path"));
    return Result<PrefabAnimator>::success(std::move(animator));
}

Result<PrefabRigidbody> read_rigidbody_component(const nlohmann::json& value) {
    if (!value.is_object())
        return Result<PrefabRigidbody>::failure(
            prefab_error("PREFAB-COMPONENT-INVALID", "Component entry must be an object"));
    PrefabRigidbody rigidbody;
    rigidbody.id = value.value("id", std::string{});
    const auto type = normalize_primitive_name(value.value("type", std::string{}));
    if (type != "rigidbody")
        return Result<PrefabRigidbody>::failure(
            prefab_error("PREFAB-COMPONENT-TYPE", "Unsupported prefab component type (expected rigidbody)"));
    const auto& data = value.contains("data") ? value.at("data") : value;
    rigidbody.motion_type = data.value("motionType", data.value("motion_type", std::string{"dynamic"}));
    rigidbody.mass = data.value("mass", 1.0f);
    rigidbody.linear_damping = data.value("linearDamping", data.value("linear_damping", 0.0f));
    rigidbody.angular_damping = data.value("angularDamping", data.value("angular_damping", 0.05f));
    rigidbody.use_gravity = data.value("useGravity", data.value("use_gravity", true));
    rigidbody.freeze_rotation = data.value("freezeRotation", data.value("freeze_rotation", false));
    const auto motion = normalize_primitive_name(rigidbody.motion_type);
    if (motion != "dynamic" && motion != "kinematic")
        return Result<PrefabRigidbody>::failure(
            prefab_error("PREFAB-RIGIDBODY-MOTION", "rigidbody motionType must be dynamic or kinematic"));
    if (!(rigidbody.mass > 0.0f) || !std::isfinite(rigidbody.mass))
        return Result<PrefabRigidbody>::failure(
            prefab_error("PREFAB-RIGIDBODY-MASS", "rigidbody mass must be finite and positive"));
    if (!(rigidbody.linear_damping >= 0.0f) || !std::isfinite(rigidbody.linear_damping) ||
        !(rigidbody.angular_damping >= 0.0f) || !std::isfinite(rigidbody.angular_damping))
        return Result<PrefabRigidbody>::failure(
            prefab_error("PREFAB-RIGIDBODY-DAMPING", "rigidbody damping must be finite and non-negative"));
    rigidbody.motion_type = motion;
    return Result<PrefabRigidbody>::success(std::move(rigidbody));
}

void expand_bounds_component(MeshBounds& bounds, float x, float y, float z) {
    if (x < bounds.min_x) bounds.min_x = x;
    if (y < bounds.min_y) bounds.min_y = y;
    if (z < bounds.min_z) bounds.min_z = z;
    if (x > bounds.max_x) bounds.max_x = x;
    if (y > bounds.max_y) bounds.max_y = y;
    if (z > bounds.max_z) bounds.max_z = z;
}

MeshBounds transform_bounds(const MeshBounds& local, const TransformComponent& transform) {
    const float corners[8][3] = {
        {local.min_x, local.min_y, local.min_z}, {local.max_x, local.min_y, local.min_z},
        {local.max_x, local.max_y, local.min_z}, {local.min_x, local.max_y, local.min_z},
        {local.min_x, local.min_y, local.max_z}, {local.max_x, local.min_y, local.max_z},
        {local.max_x, local.max_y, local.max_z}, {local.min_x, local.max_y, local.max_z},
    };
    MeshBounds world{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest(),
                     std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
    const auto qx = transform.rotation[0];
    const auto qy = transform.rotation[1];
    const auto qz = transform.rotation[2];
    const auto qw = transform.rotation[3];
    const float xx = qx * qx;
    const float yy = qy * qy;
    const float zz = qz * qz;
    const float xy = qx * qy;
    const float xz = qx * qz;
    const float yz = qy * qz;
    const float wx = qw * qx;
    const float wy = qw * qy;
    const float wz = qw * qz;
    const float m00 = 1.0f - 2.0f * (yy + zz);
    const float m01 = 2.0f * (xy - wz);
    const float m02 = 2.0f * (xz + wy);
    const float m10 = 2.0f * (xy + wz);
    const float m11 = 1.0f - 2.0f * (xx + zz);
    const float m12 = 2.0f * (yz - wx);
    const float m20 = 2.0f * (xz - wy);
    const float m21 = 2.0f * (yz + wx);
    const float m22 = 1.0f - 2.0f * (xx + yy);
    for (const auto& corner : corners) {
        const float sx = corner[0] * transform.scale[0];
        const float sy = corner[1] * transform.scale[1];
        const float sz = corner[2] * transform.scale[2];
        const float rx = m00 * sx + m01 * sy + m02 * sz;
        const float ry = m10 * sx + m11 * sy + m12 * sz;
        const float rz = m20 * sx + m21 * sy + m22 * sz;
        expand_bounds_component(world, rx + transform.position[0], ry + transform.position[1], rz + transform.position[2]);
    }
    return world;
}

std::string normalize_asset_path(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    std::transform(path.begin(), path.end(), path.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return path;
}

} // namespace

std::string primitive_mesh_cache_key(const std::string& primitive, const std::array<float, 3>& color) {
    char buffer[128];
    std::snprintf(buffer, sizeof(buffer), "__primitive/%s/%.3f_%.3f_%.3f", primitive.c_str(), color[0], color[1],
                  color[2]);
    return normalize_asset_path(buffer);
}

std::array<float, 3> resolved_prefab_mesh_color(const PrefabMeshSource& mesh,
    const PrefabAsset::MaterialLookup& lookup_material) {
    if (mesh.material && lookup_material) {
        if (const MaterialAsset* material = lookup_material(*mesh.material))
            return {material->base_color[0], material->base_color[1], material->base_color[2]};
    }
    return mesh.color;
}

PrefabAsset::MaterialLookup make_material_lookup(const std::map<std::string, MaterialAsset>* materials) {
    return [materials](const std::string& normalized_path) -> const MaterialAsset* {
        if (!materials) return nullptr;
        const auto found = materials->find(normalize_asset_path(normalized_path));
        return found == materials->end() ? nullptr : &found->second;
    };
}

std::string PrefabAsset::mesh_key_for_part(const PrefabPart& part, const MaterialLookup& lookup_material) const {
    if (part.mesh.asset) return normalize_asset_path(*part.mesh.asset);
    if (part.mesh.primitive)
        return primitive_mesh_cache_key(*part.mesh.primitive, resolved_prefab_mesh_color(part.mesh, lookup_material));
    return {};
}

std::vector<std::string> PrefabAsset::required_mesh_keys(const MaterialLookup& lookup_material) const {
    std::vector<std::string> keys;
    if (!is_compositional()) {
        if (!mesh.empty()) keys.push_back(normalize_asset_path(mesh));
        return keys;
    }
    for (const auto& part : parts) {
        const auto key = mesh_key_for_part(part, lookup_material);
        if (!key.empty()) keys.push_back(key);
    }
    return keys;
}

MeshBounds PrefabAsset::bounds(const std::map<std::string, MeshBounds>& mesh_bounds,
    const MaterialLookup& lookup_material) const {
    if (!is_compositional()) {
        const auto found = mesh_bounds.find(normalize_asset_path(mesh));
        if (found != mesh_bounds.end()) return found->second;
        return {-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f};
    }
    MeshBounds combined{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                        std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest(),
                        std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
    bool any = false;
    for (const auto& part : parts) {
        const auto key = mesh_key_for_part(part, lookup_material);
        const auto found = mesh_bounds.find(key);
        if (found == mesh_bounds.end()) continue;
        const auto part_bounds = transform_bounds(found->second, part.transform);
        if (!any) {
            combined = part_bounds;
            any = true;
        } else {
            expand_bounds_component(combined, part_bounds.min_x, part_bounds.min_y, part_bounds.min_z);
            expand_bounds_component(combined, part_bounds.max_x, part_bounds.max_y, part_bounds.max_z);
        }
    }
    if (!any) {
        return {-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f};
    }
    return combined;
}

Result<PrefabAsset> PrefabAsset::load(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) return Result<PrefabAsset>::failure(prefab_error("PREFAB-FILE-READ", "Could not read prefab: " + path.generic_string()));
    nlohmann::json document;
    try {
        input >> document;
    } catch (const std::exception& exception) {
        auto error = prefab_error("PREFAB-JSON-PARSE", "Could not parse prefab JSON: " + path.generic_string());
        error.causes.push_back(exception.what());
        return Result<PrefabAsset>::failure(std::move(error));
    }

    PrefabAsset asset;
    asset.schema_version = document.value("schemaVersion", 1);
    if (document.contains("mesh") && document["mesh"].is_string()) asset.mesh = document["mesh"].get<std::string>();
    if (document.contains("light")) {
        const auto light = read_light(document["light"]);
        if (!light) return Result<PrefabAsset>::failure(light.error());
        asset.light = light.value();
    }
    if (document.contains("characterAsset") && document["characterAsset"].is_string())
        asset.character_asset = document["characterAsset"].get<std::string>();
    if (document.contains("entities") && document["entities"].is_array()) {
        for (const auto& entity : document["entities"]) {
            if (!entity.contains("mesh")) continue;
            PrefabPart part;
            part.name = entity.value("name", std::string{"Part"});
            if (entity.contains("transform")) part.transform = read_transform(entity["transform"]);
            const auto mesh = read_mesh_source(entity["mesh"]);
            if (!mesh) return Result<PrefabAsset>::failure(mesh.error());
            part.mesh = mesh.value();
            if (part.mesh.material) part.mesh.material = normalize_asset_path(*part.mesh.material);
            asset.parts.push_back(std::move(part));
        }
    }
    if (document.contains("collision") && document["collision"].is_array()) {
        for (const auto& entry : document["collision"]) {
            const auto volume = read_collision_volume(entry);
            if (!volume) return Result<PrefabAsset>::failure(volume.error());
            asset.collision.push_back(volume.value());
        }
        for (std::size_t index = 0; index < asset.collision.size(); ++index) {
            if (asset.collision[index].id.empty())
                asset.collision[index].id = "collision-" + std::to_string(index);
        }
    }
    if (document.contains("components") && document["components"].is_array()) {
        for (const auto& entry : document["components"]) {
            const auto type = normalize_primitive_name(entry.value("type", std::string{}));
            if (type == "collider") {
                const auto& data = entry.contains("data") ? entry.at("data") : entry;
                auto volume_json = data;
                if (entry.contains("id") && (!volume_json.contains("id") || volume_json["id"].get<std::string>().empty()))
                    volume_json["id"] = entry["id"];
                const auto volume = read_collision_volume(volume_json);
                if (!volume) return Result<PrefabAsset>::failure(volume.error());
                PrefabCollisionVolume collider = volume.value();
                if (collider.id.empty()) collider.id = "collision-" + std::to_string(asset.collision.size());
                asset.collision.push_back(std::move(collider));
                continue;
            }
            if (type == "animator") {
                const auto animator = read_animator_component(entry);
                if (!animator) return Result<PrefabAsset>::failure(animator.error());
                PrefabAnimator component = animator.value();
                if (component.id.empty()) component.id = "animator-" + std::to_string(asset.animators.size());
                asset.animators.push_back(std::move(component));
                continue;
            }
            if (type == "rigidbody") {
                const auto rigidbody = read_rigidbody_component(entry);
                if (!rigidbody) return Result<PrefabAsset>::failure(rigidbody.error());
                PrefabRigidbody component = rigidbody.value();
                if (component.id.empty()) component.id = "rigidbody-" + std::to_string(asset.rigidbodies.size());
                asset.rigidbodies.push_back(std::move(component));
                continue;
            }
            const auto binding = read_script_binding_component(entry);
            if (!binding) return Result<PrefabAsset>::failure(binding.error());
            PrefabScriptBinding script = binding.value();
            if (script.id.empty()) script.id = "script-" + std::to_string(asset.script_bindings.size());
            asset.script_bindings.push_back(std::move(script));
        }
    }
    if (asset.schema_version >= 2) {
        if (asset.parts.empty() && asset.collision.empty() && asset.script_bindings.empty() && asset.animators.empty() &&
            asset.rigidbodies.empty())
            return Result<PrefabAsset>::failure(
                prefab_error("PREFAB-PARTS-MISSING", "schemaVersion 2 prefabs require mesh parts, collision, or components"));
    } else if (asset.mesh.empty() && asset.parts.empty()) {
        return Result<PrefabAsset>::failure(prefab_error("PREFAB-MESH-MISSING", "Prefab requires a mesh path or parts"));
    }
    if (!asset.mesh.empty()) asset.mesh = normalize_asset_path(asset.mesh);
    return Result<PrefabAsset>::success(std::move(asset));
}

Result<void> PrefabAsset::save(const std::filesystem::path& path) const {
    nlohmann::json document;
    document["schemaVersion"] = schema_version;
    nlohmann::json entities = nlohmann::json::array();
    for (const auto& part : parts) {
        nlohmann::json entity;
        entity["name"] = part.name;
        entity["transform"] = write_transform(part.transform);
        entity["parent"] = nullptr;
        nlohmann::json mesh_block;
        if (part.mesh.primitive) mesh_block["primitive"] = *part.mesh.primitive;
        if (part.mesh.asset) mesh_block["asset"] = *part.mesh.asset;
        if (part.mesh.material) mesh_block["material"] = *part.mesh.material;
        mesh_block["color"] = write_vec3(part.mesh.color);
        entity["mesh"] = std::move(mesh_block);
        entities.push_back(std::move(entity));
    }
    document["entities"] = std::move(entities);
    if (!collision.empty()) {
        nlohmann::json collision_array = nlohmann::json::array();
        for (const auto& volume : collision) {
            nlohmann::json entry;
            if (!volume.id.empty()) entry["id"] = volume.id;
            entry["shape"] = volume.shape == PrefabCollisionShape::Box       ? "box"
                             : volume.shape == PrefabCollisionShape::Sphere ? "sphere"
                                                                            : "capsule";
            switch (volume.layer) {
            case CollisionLayer::StaticWorld: entry["layer"] = "staticWorld"; break;
            case CollisionLayer::Dynamic: entry["layer"] = "dynamic"; break;
            case CollisionLayer::Character: entry["layer"] = "character"; break;
            case CollisionLayer::Trigger: entry["layer"] = "trigger"; break;
            }
            entry["trigger"] = volume.trigger;
            if (!volume.interaction_id.empty()) entry["interaction"] = volume.interaction_id;
            if (!volume.combat_hit_id.empty()) entry["combatHit"] = volume.combat_hit_id;
            if (!volume.combat_hurt_id.empty()) entry["combatHurt"] = volume.combat_hurt_id;
            entry["transform"] = write_transform(volume.transform);
            if (volume.shape == PrefabCollisionShape::Box)
                entry["halfExtent"] = write_vec3({volume.half_extent.x, volume.half_extent.y, volume.half_extent.z});
            else if (volume.shape == PrefabCollisionShape::Capsule) {
                entry["radius"] = volume.radius;
                entry["halfHeight"] = volume.capsule_half_height;
            } else
                entry["radius"] = volume.radius;
            collision_array.push_back(std::move(entry));
        }
        document["collision"] = std::move(collision_array);
    }
    if (!script_bindings.empty() || !animators.empty() || !rigidbodies.empty()) {
        nlohmann::json components = nlohmann::json::array();
        for (const auto& binding : script_bindings) {
            components.push_back({{"id", binding.id}, {"type", "scriptBinding"},
                {"data", {{"kind", binding.kind}, {"bindingId", binding.binding_id}}}});
        }
        for (const auto& animator : animators) {
            nlohmann::json data{{"controller", animator.controller}};
            if (!animator.default_state.empty()) data["defaultState"] = animator.default_state;
            components.push_back({{"id", animator.id}, {"type", "animator"}, {"data", std::move(data)}});
        }
        for (const auto& rigidbody : rigidbodies) {
            components.push_back({{"id", rigidbody.id}, {"type", "rigidbody"},
                {"data", {{"motionType", rigidbody.motion_type}, {"mass", rigidbody.mass},
                    {"linearDamping", rigidbody.linear_damping}, {"angularDamping", rigidbody.angular_damping},
                    {"useGravity", rigidbody.use_gravity}, {"freezeRotation", rigidbody.freeze_rotation}}}});
        }
        document["components"] = std::move(components);
    }
    if (light) {
        document["light"] = {{"color", write_vec3(light->color)},
                             {"radius", light->radius},
                             {"strength", light->strength},
                             {"offset", write_vec3(light->offset)}};
    }
    if (character_asset) document["characterAsset"] = *character_asset;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    const auto temporary = path.string() + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) return Result<void>::failure(prefab_error("PREFAB-FILE-WRITE", "Could not write prefab: " + path.generic_string()));
    output << document.dump(2) << '\n';
    output.close();
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::filesystem::rename(temporary, path, ignored);
    if (ignored)
        return Result<void>::failure(prefab_error("PREFAB-FILE-WRITE", "Could not replace prefab: " + path.generic_string()));
    return Result<void>::success();
}

std::string resolve_prefab_catalog_path(const std::map<std::string, PrefabAsset>& catalog,
    const std::string& prefab_asset) {
    auto normalize_path = [](std::string path) {
        std::replace(path.begin(), path.end(), '\\', '/');
        std::transform(path.begin(), path.end(), path.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        return path;
    };
    const auto normalized = normalize_path(prefab_asset);
    if (catalog.find(normalized) != catalog.end()) return normalized;
    const auto filename = std::filesystem::path(normalized).filename().string();
    std::string match;
    int count = 0;
    for (const auto& entry : catalog) {
        const auto entry_path = normalize_path(entry.first);
        if (std::filesystem::path(entry_path).filename().string() != filename) continue;
        match = entry_path;
        ++count;
    }
    return count == 1 ? match : normalized;
}

const PrefabAsset* find_prefab_in_catalog(const std::map<std::string, PrefabAsset>& catalog,
    const std::string& prefab_asset) {
    const auto resolved = resolve_prefab_catalog_path(catalog, prefab_asset);
    const auto found = catalog.find(resolved);
    return found == catalog.end() ? nullptr : &found->second;
}

void ensure_prefab_catalog_meshes(const std::filesystem::path& project_root,
    const std::map<std::string, PrefabAsset>& prefab_catalog, const PrefabAsset::MaterialLookup& lookup_material,
    std::map<std::string, MeshBounds>& mesh_bounds,
    std::vector<std::pair<std::string, ImportedMesh>>& imported_meshes,
    std::set<std::string>* reload_mesh_keys) {
    std::set<std::string> existing;
    for (const auto& mesh : imported_meshes) existing.insert(normalize_asset_path(mesh.first));
    if (reload_mesh_keys && !reload_mesh_keys->empty()) {
        std::set<std::string> normalized_reloads;
        for (const auto& key : *reload_mesh_keys) normalized_reloads.insert(normalize_asset_path(key));
        imported_meshes.erase(std::remove_if(imported_meshes.begin(), imported_meshes.end(),
                                   [&](const std::pair<std::string, ImportedMesh>& entry) {
                                       return normalized_reloads.count(normalize_asset_path(entry.first)) > 0;
                                   }),
            imported_meshes.end());
        for (const auto& key : normalized_reloads) {
            mesh_bounds.erase(key);
            existing.erase(key);
            const auto extension = std::filesystem::path(key).extension().string();
            if (extension != ".gltf" && extension != ".glb") continue;
            auto imported = import_project_mesh(project_root / key);
            if (!imported) continue;
            mesh_bounds[key] = imported.value().aabb;
            imported_meshes.emplace_back(key, std::move(imported.value()));
            existing.insert(key);
        }
        reload_mesh_keys->clear();
    }
    for (const auto& entry : prefab_catalog) {
        for (const auto& key : entry.second.required_mesh_keys(lookup_material)) {
            const auto normalized = normalize_asset_path(key);
            if (existing.find(normalized) != existing.end()) continue;
            if (normalized.rfind("__primitive/", 0) == 0) {
                const auto rest = normalized.substr(12);
                const auto slash = rest.find('/');
                if (slash == std::string::npos) continue;
                const auto primitive = rest.substr(0, slash);
                const auto color_token = rest.substr(slash + 1);
                std::array<float, 3> color{};
                if (std::sscanf(color_token.c_str(), "%f_%f_%f", &color[0], &color[1], &color[2]) != 3) continue;
                auto generated = generate_primitive_mesh(primitive, color);
                if (!generated) continue;
                mesh_bounds[normalized] = generated.value().aabb;
                imported_meshes.emplace_back(normalized, std::move(generated.value()));
                existing.insert(normalized);
                continue;
            }
            const auto extension = std::filesystem::path(normalized).extension().string();
            if (extension != ".gltf" && extension != ".glb") continue;
            auto imported = import_project_mesh(project_root / normalized);
            if (!imported) continue;
            mesh_bounds[normalized] = imported.value().aabb;
            imported_meshes.emplace_back(normalized, std::move(imported.value()));
            existing.insert(normalized);
        }
    }
}

} // namespace engine
