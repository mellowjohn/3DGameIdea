#include "engine/world/authored_components.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace engine {
namespace {

EngineError authored_error(std::string code, std::string message) {
    return {std::move(code), Severity::Error, ErrorCategory::Validation, "authored-components", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, "Correct the authored component payload.", make_correlation_id()};
}

std::string normalize_key(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

nlohmann::json write_transform(const TransformComponent& transform) {
    return {{"position", transform.position}, {"rotation", transform.rotation}, {"scale", transform.scale}};
}

TransformComponent read_transform(const nlohmann::json& value) {
    TransformComponent transform;
    if (value.contains("position")) transform.position = value.at("position").get<std::array<float, 3>>();
    if (value.contains("rotation")) transform.rotation = value.at("rotation").get<std::array<float, 4>>();
    if (value.contains("scale")) transform.scale = value.at("scale").get<std::array<float, 3>>();
    return transform;
}

std::string layer_name(CollisionLayer layer) {
    switch (layer) {
    case CollisionLayer::StaticWorld: return "staticWorld";
    case CollisionLayer::Dynamic: return "dynamic";
    case CollisionLayer::Character: return "character";
    case CollisionLayer::Trigger: return "trigger";
    }
    return "staticWorld";
}

Result<CollisionLayer> read_layer(const std::string& value) {
    const auto key = normalize_key(value);
    if (key == "staticworld") return Result<CollisionLayer>::success(CollisionLayer::StaticWorld);
    if (key == "dynamic") return Result<CollisionLayer>::success(CollisionLayer::Dynamic);
    if (key == "character") return Result<CollisionLayer>::success(CollisionLayer::Character);
    if (key == "trigger") return Result<CollisionLayer>::success(CollisionLayer::Trigger);
    return Result<CollisionLayer>::failure(authored_error("COMPONENT-LAYER-INVALID", "Unsupported collision layer: " + value));
}

Result<PrefabCollisionVolume> parse_collision_object(const nlohmann::json& value) {
    if (!value.is_object())
        return Result<PrefabCollisionVolume>::failure(authored_error("COMPONENT-COLLIDER-INVALID", "Collider data must be an object"));
    PrefabCollisionVolume volume;
    volume.id = value.value("id", std::string{});
    const auto shape = normalize_key(value.value("shape", std::string{"box"}));
    if (shape == "box") volume.shape = PrefabCollisionShape::Box;
    else if (shape == "sphere") volume.shape = PrefabCollisionShape::Sphere;
    else
        return Result<PrefabCollisionVolume>::failure(authored_error("COMPONENT-COLLIDER-SHAPE", "Unsupported shape: " + shape));
    if (value.contains("layer")) {
        const auto layer = read_layer(value["layer"].get<std::string>());
        if (!layer) return Result<PrefabCollisionVolume>::failure(layer.error());
        volume.layer = layer.value();
    }
    if (value.contains("trigger")) volume.trigger = value["trigger"].get<bool>();
    if (value.contains("interaction")) volume.interaction_id = value["interaction"].get<std::string>();
    if (value.contains("combatHit")) volume.combat_hit_id = value["combatHit"].get<std::string>();
    if (value.contains("combatHurt")) volume.combat_hurt_id = value["combatHurt"].get<std::string>();
    if (value.contains("transform")) volume.transform = read_transform(value["transform"]);
    if (value.contains("halfExtent")) {
        const auto extent = value["halfExtent"].get<std::array<float, 3>>();
        volume.half_extent = {extent[0], extent[1], extent[2]};
    }
    if (value.contains("radius")) volume.radius = value["radius"].get<float>();
    if (volume.trigger) volume.layer = CollisionLayer::Trigger;
    if (!volume.interaction_id.empty() || volume.is_combat_sensor()) {
        volume.trigger = true;
        volume.layer = CollisionLayer::Trigger;
    }
    if (!volume.combat_hit_id.empty() && !volume.combat_hurt_id.empty())
        return Result<PrefabCollisionVolume>::failure(
            authored_error("COMPONENT-COLLIDER-INVALID", "Cannot set both combatHit and combatHurt"));
    if (volume.shape == PrefabCollisionShape::Box) {
        if (volume.half_extent.x <= 0 || volume.half_extent.y <= 0 || volume.half_extent.z <= 0)
            return Result<PrefabCollisionVolume>::failure(
                authored_error("COMPONENT-COLLIDER-INVALID", "Box halfExtent must be positive"));
    } else if (!(volume.radius > 0)) {
        return Result<PrefabCollisionVolume>::failure(
            authored_error("COMPONENT-COLLIDER-INVALID", "Sphere radius must be positive"));
    }
    return Result<PrefabCollisionVolume>::success(std::move(volume));
}

nlohmann::json write_collision_object(const PrefabCollisionVolume& volume) {
    nlohmann::json entry;
    if (!volume.id.empty()) entry["id"] = volume.id;
    entry["shape"] = volume.shape == PrefabCollisionShape::Box ? "box" : "sphere";
    entry["layer"] = layer_name(volume.layer);
    entry["trigger"] = volume.trigger;
    if (!volume.interaction_id.empty()) entry["interaction"] = volume.interaction_id;
    if (!volume.combat_hit_id.empty()) entry["combatHit"] = volume.combat_hit_id;
    if (!volume.combat_hurt_id.empty()) entry["combatHurt"] = volume.combat_hurt_id;
    entry["transform"] = write_transform(volume.transform);
    if (volume.shape == PrefabCollisionShape::Box)
        entry["halfExtent"] = nlohmann::json::array({volume.half_extent.x, volume.half_extent.y, volume.half_extent.z});
    else
        entry["radius"] = volume.radius;
    return entry;
}

const PrefabCollisionVolume* find_prefab_collider(const PrefabAsset& prefab, const std::string& id) {
    for (const auto& volume : prefab.collision) {
        if (volume.id == id) return &volume;
    }
    return nullptr;
}

const PrefabScriptBinding* find_prefab_script(const PrefabAsset& prefab, const std::string& id) {
    for (const auto& binding : prefab.script_bindings) {
        if (binding.id == id) return &binding;
    }
    return nullptr;
}

const PrefabAnimator* find_prefab_animator(const PrefabAsset& prefab, const std::string& id) {
    for (const auto& animator : prefab.animators) {
        if (animator.id == id) return &animator;
    }
    return nullptr;
}

} // namespace

std::string authored_component_type_name(AuthoredComponentType type) {
    switch (type) {
    case AuthoredComponentType::Collider: return "collider";
    case AuthoredComponentType::ScriptBinding: return "scriptBinding";
    case AuthoredComponentType::Animator: return "animator";
    }
    return "collider";
}

std::optional<AuthoredComponentType> parse_authored_component_type(const std::string& value) {
    const auto key = normalize_key(value);
    if (key == "collider") return AuthoredComponentType::Collider;
    if (key == "scriptbinding" || key == "script") return AuthoredComponentType::ScriptBinding;
    if (key == "animator") return AuthoredComponentType::Animator;
    return std::nullopt;
}

AuthoredComponentsComponent seed_authored_components_from_prefab(const PrefabAsset& prefab) {
    AuthoredComponentsComponent components;
    components.entries.reserve(
        prefab.collision.size() + prefab.script_bindings.size() + prefab.animators.size());
    for (std::size_t index = 0; index < prefab.collision.size(); ++index) {
        AuthoredComponentEntry entry;
        entry.type = AuthoredComponentType::Collider;
        entry.from_prefab = true;
        entry.overridden = false;
        entry.collider = prefab.collision[index];
        entry.id = entry.collider.id.empty() ? ("collision-" + std::to_string(index)) : entry.collider.id;
        entry.collider.id = entry.id;
        components.entries.push_back(std::move(entry));
    }
    for (std::size_t index = 0; index < prefab.script_bindings.size(); ++index) {
        AuthoredComponentEntry entry;
        entry.type = AuthoredComponentType::ScriptBinding;
        entry.from_prefab = true;
        entry.overridden = false;
        entry.script.kind = prefab.script_bindings[index].kind;
        entry.script.binding_id = prefab.script_bindings[index].binding_id;
        entry.id = prefab.script_bindings[index].id.empty() ? ("script-" + std::to_string(index))
                                                              : prefab.script_bindings[index].id;
        components.entries.push_back(std::move(entry));
    }
    for (std::size_t index = 0; index < prefab.animators.size(); ++index) {
        AuthoredComponentEntry entry;
        entry.type = AuthoredComponentType::Animator;
        entry.from_prefab = true;
        entry.overridden = false;
        entry.animator.controller = prefab.animators[index].controller;
        entry.animator.default_state = prefab.animators[index].default_state;
        entry.id = prefab.animators[index].id.empty() ? ("animator-" + std::to_string(index))
                                                        : prefab.animators[index].id;
        components.entries.push_back(std::move(entry));
    }
    components.generation = 1;
    return components;
}

std::vector<PrefabCollisionVolume> effective_collision_volumes(
    const AuthoredComponentsComponent* entity_components, const PrefabAsset* prefab) {
    if (entity_components && !entity_components->entries.empty()) {
        std::vector<PrefabCollisionVolume> volumes;
        for (const auto& entry : entity_components->entries) {
            if (entry.type != AuthoredComponentType::Collider) continue;
            PrefabCollisionVolume volume = entry.collider;
            if (entry.from_prefab && !entry.overridden && prefab) {
                if (const auto* source = find_prefab_collider(*prefab, entry.id)) volume = *source;
            }
            volume.id = entry.id;
            volumes.push_back(std::move(volume));
        }
        return volumes;
    }
    if (prefab) return prefab->collision;
    return {};
}

Result<void> validate_authored_component_entry(const AuthoredComponentEntry& entry) {
    if (entry.id.empty())
        return Result<void>::failure(authored_error("COMPONENT-ID-REQUIRED", "Component id is required"));
    if (entry.type == AuthoredComponentType::Collider) {
        const auto parsed = parse_collision_object(write_collision_object(entry.collider));
        if (!parsed) return Result<void>::failure(parsed.error());
        return Result<void>::success();
    }
    if (entry.type == AuthoredComponentType::Animator) {
        if (entry.animator.controller.empty())
            return Result<void>::failure(
                authored_error("COMPONENT-ANIMATOR-INVALID", "animator requires controller path"));
        return Result<void>::success();
    }
    if (entry.script.kind.empty() || entry.script.binding_id.empty())
        return Result<void>::failure(
            authored_error("COMPONENT-SCRIPT-INVALID", "scriptBinding requires kind and bindingId"));
    const auto kind = normalize_key(entry.script.kind);
    if (kind != "interaction" && kind != "combathit" && kind != "combathurt" && kind != "handler")
        return Result<void>::failure(authored_error("COMPONENT-SCRIPT-KIND", "Unsupported scriptBinding kind"));
    return Result<void>::success();
}

std::string collision_volume_to_json_object(const PrefabCollisionVolume& volume) {
    return write_collision_object(volume).dump();
}

Result<PrefabCollisionVolume> collision_volume_from_json_object(const std::string& json) {
    try {
        return parse_collision_object(nlohmann::json::parse(json));
    } catch (const std::exception& exception) {
        EngineError error = authored_error("COMPONENT-JSON-INVALID", "Could not parse collider JSON");
        error.causes.push_back(exception.what());
        return Result<PrefabCollisionVolume>::failure(std::move(error));
    }
}

std::string authored_component_entry_to_json(const AuthoredComponentEntry& entry) {
    nlohmann::json root{{"id", entry.id}, {"type", authored_component_type_name(entry.type)},
        {"source", entry.from_prefab ? "prefab" : "instance"}, {"overridden", entry.overridden}};
    if (entry.type == AuthoredComponentType::Collider) root["data"] = write_collision_object(entry.collider);
    else if (entry.type == AuthoredComponentType::Animator) {
        nlohmann::json data{{"controller", entry.animator.controller}};
        if (!entry.animator.default_state.empty()) data["defaultState"] = entry.animator.default_state;
        root["data"] = std::move(data);
    } else
        root["data"] = {{"kind", entry.script.kind}, {"bindingId", entry.script.binding_id}};
    return root.dump();
}

Result<AuthoredComponentEntry> authored_component_entry_from_json(const std::string& json) {
    try {
        const auto root = nlohmann::json::parse(json);
        AuthoredComponentEntry entry;
        entry.id = root.value("id", std::string{});
        const auto type = parse_authored_component_type(root.value("type", std::string{"collider"}));
        if (!type)
            return Result<AuthoredComponentEntry>::failure(
                authored_error("COMPONENT-TYPE-INVALID", "Unsupported component type"));
        entry.type = *type;
        const auto source = normalize_key(root.value("source", std::string{"instance"}));
        entry.from_prefab = source == "prefab";
        entry.overridden = root.value("overridden", false);
        const auto& data = root.contains("data") ? root.at("data") : root;
        if (entry.type == AuthoredComponentType::Collider) {
            const auto volume = parse_collision_object(data);
            if (!volume) return Result<AuthoredComponentEntry>::failure(volume.error());
            entry.collider = volume.value();
            if (entry.collider.id.empty()) entry.collider.id = entry.id;
            if (entry.id.empty()) entry.id = entry.collider.id;
        } else if (entry.type == AuthoredComponentType::Animator) {
            entry.animator.controller = data.value("controller", std::string{});
            entry.animator.default_state =
                data.value("defaultState", data.value("default_state", std::string{}));
        } else {
            entry.script.kind = data.value("kind", std::string{});
            entry.script.binding_id = data.value("bindingId", data.value("binding_id", std::string{}));
        }
        const auto valid = validate_authored_component_entry(entry);
        if (!valid) return Result<AuthoredComponentEntry>::failure(valid.error());
        return Result<AuthoredComponentEntry>::success(std::move(entry));
    } catch (const std::exception& exception) {
        EngineError error = authored_error("COMPONENT-JSON-INVALID", "Could not parse component JSON");
        error.causes.push_back(exception.what());
        return Result<AuthoredComponentEntry>::failure(std::move(error));
    }
}

std::string authored_components_to_json(const AuthoredComponentsComponent& components) {
    nlohmann::json array = nlohmann::json::array();
    for (const auto& entry : components.entries)
        array.push_back(nlohmann::json::parse(authored_component_entry_to_json(entry)));
    return array.dump();
}

Result<AuthoredComponentsComponent> authored_components_from_json(const std::string& json) {
    try {
        const auto root = nlohmann::json::parse(json);
        if (!root.is_array())
            return Result<AuthoredComponentsComponent>::failure(
                authored_error("COMPONENT-ARRAY-REQUIRED", "components must be a JSON array"));
        AuthoredComponentsComponent components;
        for (const auto& item : root) {
            const auto parsed = authored_component_entry_from_json(item.dump());
            if (!parsed) return Result<AuthoredComponentsComponent>::failure(parsed.error());
            components.entries.push_back(parsed.value());
        }
        components.generation = 1;
        return Result<AuthoredComponentsComponent>::success(std::move(components));
    } catch (const std::exception& exception) {
        EngineError error = authored_error("COMPONENT-JSON-INVALID", "Could not parse components JSON");
        error.causes.push_back(exception.what());
        return Result<AuthoredComponentsComponent>::failure(std::move(error));
    }
}

std::size_t propagate_prefab_components_into_entries(
    AuthoredComponentsComponent& entity_components, const PrefabAsset& prefab) {
    std::size_t updated = 0;
    const auto seeded = seed_authored_components_from_prefab(prefab);
    std::vector<AuthoredComponentEntry> next = entity_components.entries;

    // Update non-overridden prefab-linked entries from prefab.
    for (auto& entry : next) {
        if (!entry.from_prefab || entry.overridden) continue;
        if (entry.type == AuthoredComponentType::Collider) {
            if (const auto* source = find_prefab_collider(prefab, entry.id)) {
                entry.collider = *source;
                entry.collider.id = entry.id;
                ++updated;
            }
        } else if (entry.type == AuthoredComponentType::Animator) {
            if (const auto* source = find_prefab_animator(prefab, entry.id)) {
                entry.animator.controller = source->controller;
                entry.animator.default_state = source->default_state;
                ++updated;
            }
        } else if (const auto* source = find_prefab_script(prefab, entry.id)) {
            entry.script.kind = source->kind;
            entry.script.binding_id = source->binding_id;
            ++updated;
        }
    }

    // Add new prefab components that instances do not yet have.
    for (const auto& seed : seeded.entries) {
        const bool exists = std::any_of(next.begin(), next.end(),
            [&](const AuthoredComponentEntry& entry) { return entry.id == seed.id; });
        if (exists) continue;
        next.push_back(seed);
        ++updated;
    }

    // Remove prefab-sourced non-overridden entries deleted from the prefab.
    next.erase(std::remove_if(next.begin(), next.end(),
                   [&](const AuthoredComponentEntry& entry) {
                       if (!entry.from_prefab || entry.overridden) return false;
                       if (entry.type == AuthoredComponentType::Collider)
                           return find_prefab_collider(prefab, entry.id) == nullptr;
                       if (entry.type == AuthoredComponentType::Animator)
                           return find_prefab_animator(prefab, entry.id) == nullptr;
                       return find_prefab_script(prefab, entry.id) == nullptr;
                   }),
        next.end());

    if (updated > 0 || next.size() != entity_components.entries.size()) {
        entity_components.entries = std::move(next);
        ++entity_components.generation;
    }
    return updated;
}

} // namespace engine
