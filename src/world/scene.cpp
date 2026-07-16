#include "engine/world/scene.h"

#include "engine/assets/character_asset.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <windows.h>

namespace engine {
namespace {
using Json = nlohmann::ordered_json;

EngineError world_error(std::string code, std::string message, std::string remedy = "Inspect the scene data and referenced entity IDs.") {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Validation, "world", std::move(message),
                       ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

bool finite(const TransformComponent& value) {
    for (float item : value.position) if (!std::isfinite(item)) return false;
    for (float item : value.rotation) if (!std::isfinite(item)) return false;
    for (float item : value.scale) if (!std::isfinite(item)) return false;
    return true;
}
}

std::optional<entt::entity> Scene::handle(const EntityId& id) const {
    const auto found = entities_.find(id.str());
    return found == entities_.end() ? std::nullopt : std::optional<entt::entity>(found->second);
}

bool Scene::contains(const EntityId& id) const { return handle(id).has_value(); }

Scene::Scene() : registry_(std::make_unique<entt::registry>()) {}
Scene::Scene(Scene&& other) noexcept : registry_(std::move(other.registry_)), entities_(std::move(other.entities_)), world_id_(std::move(other.world_id_)), document_name_(std::move(other.document_name_)), world_size_meters_(other.world_size_meters_), cell_size_meters_(other.cell_size_meters_) {
    if (!registry_) registry_ = std::make_unique<entt::registry>();
}
Scene& Scene::operator=(Scene&& other) noexcept {
    if (this != &other) {
        registry_ = std::move(other.registry_);
        entities_ = std::move(other.entities_);
        world_id_=std::move(other.world_id_);document_name_=std::move(other.document_name_);world_size_meters_=other.world_size_meters_;cell_size_meters_=other.cell_size_meters_;
        if (!registry_) registry_ = std::make_unique<entt::registry>();
    }
    return *this;
}

Result<EntityId> Scene::create_entity(std::string name, std::optional<EntityId> requested_id) {
    EntityId id = requested_id.value_or(EntityId::generate());
    if (id.empty()) return Result<EntityId>::failure(world_error("WORLD-EMPTY-UUID", "Entity UUID cannot be empty"));
    if (contains(id)) return Result<EntityId>::failure(world_error("WORLD-DUPLICATE-UUID", "Duplicate entity UUID: " + id.str()));
    const auto entity = registry_->create();
    registry_->emplace<IdComponent>(entity, id);
    registry_->emplace<NameComponent>(entity, std::move(name));
    registry_->emplace<TransformComponent>(entity);
    registry_->emplace<HierarchyComponent>(entity);
    entities_.emplace(id.str(), entity);
    return Result<EntityId>::success(id);
}

Result<void> Scene::destroy_entity(const EntityId& id) {
    const auto value = handle(id);
    if (!value) return Result<void>::failure(world_error("WORLD-ENTITY-NOT-FOUND", "Cannot destroy missing entity: " + id.str()));
    for (const auto& entry : entities_) {
        auto& hierarchy = registry_->get<HierarchyComponent>(entry.second);
        if (hierarchy.parent && *hierarchy.parent == id) hierarchy.parent.reset();
    }
    registry_->destroy(*value);
    entities_.erase(id.str());
    return Result<void>::success();
}

Result<void> Scene::rename_entity(const EntityId& id, std::string name_value) {
    const auto value = handle(id);
    if (!value) return Result<void>::failure(world_error("WORLD-ENTITY-NOT-FOUND", "Cannot rename missing entity: " + id.str()));
    registry_->get<NameComponent>(*value).name = std::move(name_value);
    return Result<void>::success();
}

Result<void> Scene::set_parent(const EntityId& child, std::optional<EntityId> parent_value) {
    const auto child_handle = handle(child);
    if (!child_handle) return Result<void>::failure(world_error("WORLD-ENTITY-NOT-FOUND", "Child entity does not exist: " + child.str()));
    if (parent_value && *parent_value == child) return Result<void>::failure(world_error("WORLD-HIERARCHY-SELF", "Entity cannot parent itself: " + child.str()));
    if (parent_value && !contains(*parent_value)) return Result<void>::failure(world_error("WORLD-PARENT-NOT-FOUND", "Parent entity does not exist: " + parent_value->str()));
    std::set<std::string> visited;
    auto cursor = parent_value;
    while (cursor) {
        if (*cursor == child) return Result<void>::failure(world_error("WORLD-HIERARCHY-CYCLE", "Parenting would create a hierarchy cycle"));
        if (!visited.insert(cursor->str()).second) return Result<void>::failure(world_error("WORLD-HIERARCHY-CORRUPT", "Existing hierarchy contains a cycle"));
        cursor = parent(*cursor);
    }
    registry_->get<HierarchyComponent>(*child_handle).parent = std::move(parent_value);
    return Result<void>::success();
}

Result<void> Scene::set_transform(const EntityId& id, const TransformComponent& transform) {
    const auto value = handle(id);
    if (!value) return Result<void>::failure(world_error("WORLD-ENTITY-NOT-FOUND", "Cannot transform missing entity: " + id.str()));
    if (!finite(transform)) return Result<void>::failure(world_error("WORLD-TRANSFORM-NONFINITE", "Transform contains NaN or infinity"));
    for (const float scale : transform.scale)
        if (std::abs(scale) < 0.000001f) return Result<void>::failure(world_error("WORLD-TRANSFORM-ZERO-SCALE", "Transform scale cannot be zero"));
    registry_->replace<TransformComponent>(*value, transform);
    return Result<void>::success();
}

Result<EntityId> Scene::place_world_object(std::string name_value, std::string prefab_asset, const TransformComponent& transform_value, std::optional<EntityId> requested_id, std::optional<std::string> character_asset, const PrefabAsset* seed_prefab) {
    if (prefab_asset.empty() || prefab_asset.rfind("assets/", 0) != 0)
        return Result<EntityId>::failure(world_error("WORLD-PREFAB-PATH-INVALID", "Placed object requires a project-relative assets/ prefab path"));
    WorldPartition partition;
    auto cell = partition.cell_for({transform_value.position[0], transform_value.position[1], transform_value.position[2]});
    if (!cell) return Result<EntityId>::failure(cell.error());
    auto created = create_entity(std::move(name_value), requested_id);
    if (!created) return created;
    auto transformed = set_transform(created.value(), transform_value);
    if (!transformed) { (void)destroy_entity(created.value()); return Result<EntityId>::failure(transformed.error()); }
    registry_->emplace<WorldPlacementComponent>(*handle(created.value()),
        WorldPlacementComponent{std::move(prefab_asset), cell.value(), std::move(character_asset), std::nullopt});
    if (seed_prefab) {
        registry_->emplace_or_replace<AuthoredComponentsComponent>(*handle(created.value()),
            seed_authored_components_from_prefab(*seed_prefab));
    }
    return created;
}

Result<void> Scene::set_authored_components(const EntityId& id, AuthoredComponentsComponent components) {
    const auto value = handle(id);
    if (!value) return Result<void>::failure(world_error("WORLD-ENTITY-NOT-FOUND", "Cannot set components on missing entity: " + id.str()));
    for (const auto& entry : components.entries) {
        const auto valid = validate_authored_component_entry(entry);
        if (!valid) return Result<void>::failure(valid.error());
    }
    ++components.generation;
    registry_->emplace_or_replace<AuthoredComponentsComponent>(*value, std::move(components));
    return Result<void>::success();
}

Result<bool> Scene::ensure_authored_components_seeded(const EntityId& id, const PrefabAsset& prefab) {
    const auto value = handle(id);
    if (!value) return Result<bool>::failure(world_error("WORLD-ENTITY-NOT-FOUND", "Cannot seed components on missing entity: " + id.str()));
    if (registry_->all_of<AuthoredComponentsComponent>(*value) &&
        !registry_->get<AuthoredComponentsComponent>(*value).entries.empty())
        return Result<bool>::success(false);
    registry_->emplace_or_replace<AuthoredComponentsComponent>(*value, seed_authored_components_from_prefab(prefab));
    return Result<bool>::success(true);
}

std::size_t Scene::seed_missing_authored_components(const std::map<std::string, PrefabAsset>& catalog) {
    std::size_t seeded = 0;
    for (const auto& id : entity_ids()) {
        const auto placement = this->placement(id);
        if (!placement) continue;
        const auto* prefab = find_prefab_in_catalog(catalog, placement->prefab_asset);
        if (!prefab) continue;
        if (prefab->collision.empty() && prefab->script_bindings.empty()) continue;
        const auto result = ensure_authored_components_seeded(id, *prefab);
        if (result && result.value()) ++seeded;
    }
    return seeded;
}

Result<void> Scene::add_authored_component(const EntityId& id, AuthoredComponentEntry entry, bool mark_override) {
    const auto valid = validate_authored_component_entry(entry);
    if (!valid) return Result<void>::failure(valid.error());
    const auto value = handle(id);
    if (!value) return Result<void>::failure(world_error("WORLD-ENTITY-NOT-FOUND", "Cannot add component to missing entity: " + id.str()));
    if (!registry_->all_of<AuthoredComponentsComponent>(*value))
        registry_->emplace<AuthoredComponentsComponent>(*value);
    auto& components = registry_->get<AuthoredComponentsComponent>(*value);
    for (const auto& existing : components.entries) {
        if (existing.id == entry.id)
            return Result<void>::failure(world_error("WORLD-COMPONENT-DUPLICATE", "Component id already exists: " + entry.id));
    }
    if (mark_override) {
        entry.from_prefab = false;
        entry.overridden = true;
    }
    components.entries.push_back(std::move(entry));
    ++components.generation;
    return Result<void>::success();
}

Result<void> Scene::remove_authored_component(const EntityId& id, const std::string& component_id) {
    const auto value = handle(id);
    if (!value || !registry_->all_of<AuthoredComponentsComponent>(*value))
        return Result<void>::failure(world_error("WORLD-COMPONENT-NOT-FOUND", "Component target missing: " + component_id));
    auto& components = registry_->get<AuthoredComponentsComponent>(*value);
    const auto before = components.entries.size();
    components.entries.erase(std::remove_if(components.entries.begin(), components.entries.end(),
                                 [&](const AuthoredComponentEntry& entry) { return entry.id == component_id; }),
        components.entries.end());
    if (components.entries.size() == before)
        return Result<void>::failure(world_error("WORLD-COMPONENT-NOT-FOUND", "Component id not found: " + component_id));
    ++components.generation;
    return Result<void>::success();
}

Result<void> Scene::set_authored_component(const EntityId& id, AuthoredComponentEntry entry, bool mark_override) {
    const auto valid = validate_authored_component_entry(entry);
    if (!valid) return Result<void>::failure(valid.error());
    const auto value = handle(id);
    if (!value) return Result<void>::failure(world_error("WORLD-ENTITY-NOT-FOUND", "Cannot set component on missing entity: " + id.str()));
    if (!registry_->all_of<AuthoredComponentsComponent>(*value))
        registry_->emplace<AuthoredComponentsComponent>(*value);
    auto& components = registry_->get<AuthoredComponentsComponent>(*value);
    for (auto& existing : components.entries) {
        if (existing.id != entry.id) continue;
        if (mark_override) {
            entry.from_prefab = existing.from_prefab;
            entry.overridden = true;
        }
        existing = std::move(entry);
        ++components.generation;
        return Result<void>::success();
    }
    if (mark_override) {
        entry.from_prefab = false;
        entry.overridden = true;
    }
    components.entries.push_back(std::move(entry));
    ++components.generation;
    return Result<void>::success();
}

std::size_t Scene::propagate_prefab_components(const std::string& prefab_asset, const PrefabAsset& prefab) {
    std::size_t total = 0;
    auto normalize = [](std::string path) {
        std::replace(path.begin(), path.end(), '\\', '/');
        std::transform(path.begin(), path.end(), path.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return path;
    };
    const auto target = normalize(prefab_asset);
    const auto target_name = std::filesystem::path(target).filename().string();
    for (const auto& entry : entities_) {
        if (!registry_->all_of<WorldPlacementComponent>(entry.second)) continue;
        const auto& placement = registry_->get<WorldPlacementComponent>(entry.second);
        const auto placed = normalize(placement.prefab_asset);
        if (placed != target && std::filesystem::path(placed).filename().string() != target_name) continue;
        if (!registry_->all_of<AuthoredComponentsComponent>(entry.second) ||
            registry_->get<AuthoredComponentsComponent>(entry.second).entries.empty()) {
            registry_->emplace_or_replace<AuthoredComponentsComponent>(entry.second, seed_authored_components_from_prefab(prefab));
            ++total;
            continue;
        }
        total += propagate_prefab_components_into_entries(registry_->get<AuthoredComponentsComponent>(entry.second), prefab);
    }
    return total;
}

std::optional<AuthoredComponentsComponent> Scene::authored_components(const EntityId& id) const {
    const auto value = handle(id);
    if (!value || !registry_->all_of<AuthoredComponentsComponent>(*value)) return std::nullopt;
    return registry_->get<AuthoredComponentsComponent>(*value);
}

Result<void> Scene::set_placement_character_asset(const EntityId& id, std::optional<std::string> character_asset) {
    const auto value = handle(id);
    if (!value || !registry_->all_of<WorldPlacementComponent>(*value))
        return Result<void>::failure(world_error("WORLD-PLACEMENT-NOT-FOUND", "Character asset target is not a placed world object"));
    registry_->get<WorldPlacementComponent>(*value).character_asset = std::move(character_asset);
    return Result<void>::success();
}

Result<void> Scene::set_placement_character_settings(const EntityId& id, std::optional<CharacterAsset> character_settings) {
    const auto value = handle(id);
    if (!value || !registry_->all_of<WorldPlacementComponent>(*value))
        return Result<void>::failure(world_error("WORLD-PLACEMENT-NOT-FOUND", "Character settings target is not a placed world object"));
    registry_->get<WorldPlacementComponent>(*value).character_settings = std::move(character_settings);
    return Result<void>::success();
}

namespace {
std::string normalize_asset_path_for_remap(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    std::transform(path.begin(), path.end(), path.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return path;
}

std::string remap_asset_path_prefix_value(std::string path, const std::string& old_prefix, const std::string& new_prefix) {
    const auto normalized = normalize_asset_path_for_remap(path);
    const auto old_norm = normalize_asset_path_for_remap(old_prefix);
    const auto new_norm = normalize_asset_path_for_remap(new_prefix);
    if (normalized == old_norm) return new_norm;
    const std::string old_slash = old_norm + "/";
    if (normalized.rfind(old_slash, 0) == 0) return new_norm + normalized.substr(old_norm.size());
    return path;
}
} // namespace

Result<void> Scene::remap_asset_path_prefix(const std::string& old_prefix, const std::string& new_prefix) {
    if (old_prefix.empty() || new_prefix.empty())
        return Result<void>::failure(world_error("WORLD-ASSET-PREFIX-INVALID", "Asset path remap requires non-empty prefixes"));
    for (const auto& entry : entities_) {
        if (!registry_->all_of<WorldPlacementComponent>(entry.second)) continue;
        auto& placement = registry_->get<WorldPlacementComponent>(entry.second);
        placement.prefab_asset = remap_asset_path_prefix_value(placement.prefab_asset, old_prefix, new_prefix);
        if (placement.character_asset)
            *placement.character_asset = remap_asset_path_prefix_value(*placement.character_asset, old_prefix, new_prefix);
    }
    return Result<void>::success();
}

Result<void> Scene::repair_prefab_paths(const std::map<std::string, PrefabAsset>& catalog) {
    for (const auto& entry : entities_) {
        if (!registry_->all_of<WorldPlacementComponent>(entry.second)) continue;
        auto& placement = registry_->get<WorldPlacementComponent>(entry.second);
        const auto resolved = resolve_prefab_catalog_path(catalog, placement.prefab_asset);
        if (catalog.find(resolved) != catalog.end()) placement.prefab_asset = resolved;
    }
    return Result<void>::success();
}

Result<void> Scene::move_world_object(const EntityId& id, const TransformComponent& transform_value) {
    const auto value = handle(id);
    if (!value || !registry_->all_of<WorldPlacementComponent>(*value))
        return Result<void>::failure(world_error("WORLD-PLACEMENT-NOT-FOUND", "Move target is not a placed world object"));
    WorldPartition partition;
    auto cell = partition.cell_for({transform_value.position[0], transform_value.position[1], transform_value.position[2]});
    if (!cell) return Result<void>::failure(cell.error());
    auto transformed = set_transform(id, transform_value);
    if (!transformed) return transformed;
    registry_->get<WorldPlacementComponent>(*value).cell = cell.value();
    return Result<void>::success();
}

Result<std::vector<EntityId>> Scene::instantiate_prefab(const Scene& prefab, std::optional<EntityId> external_parent) {
    if (external_parent && !contains(*external_parent))
        return Result<std::vector<EntityId>>::failure(world_error("PREFAB-PARENT-NOT-FOUND", "Prefab parent does not exist"));
    const auto prefab_errors = prefab.validate();
    if (!prefab_errors.empty()) return Result<std::vector<EntityId>>::failure(prefab_errors.front());
    std::map<std::string, EntityId> remap;
    std::vector<EntityId> created;
    auto rollback = [&]() { for (auto it = created.rbegin(); it != created.rend(); ++it) (void)destroy_entity(*it); };
    for (const auto& entry : prefab.entities_) {
        const auto source = entry.second;
        auto result = create_entity(prefab.registry_->get<NameComponent>(source).name);
        if (!result) { rollback(); return Result<std::vector<EntityId>>::failure(result.error()); }
        remap.emplace(entry.first, result.value());
        created.push_back(result.value());
        auto transformed = set_transform(result.value(), prefab.registry_->get<TransformComponent>(source));
        if (!transformed) { rollback(); return Result<std::vector<EntityId>>::failure(transformed.error()); }
    }
    for (const auto& entry : prefab.entities_) {
        const auto& old_hierarchy = prefab.registry_->get<HierarchyComponent>(entry.second);
        std::optional<EntityId> new_parent = external_parent;
        if (old_hierarchy.parent) {
            const auto found = remap.find(old_hierarchy.parent->str());
            if (found == remap.end()) { rollback(); return Result<std::vector<EntityId>>::failure(world_error("PREFAB-PARENT-OUTSIDE", "Prefab references a parent outside itself")); }
            new_parent = found->second;
        }
        auto parented = set_parent(remap.at(entry.first), new_parent);
        if (!parented) { rollback(); return Result<std::vector<EntityId>>::failure(parented.error()); }
    }
    return Result<std::vector<EntityId>>::success(std::move(created));
}

std::optional<std::string> Scene::name(const EntityId& id) const {
    const auto value = handle(id);
    return value ? std::optional<std::string>(registry_->get<NameComponent>(*value).name) : std::nullopt;
}

std::optional<EntityId> Scene::parent(const EntityId& id) const {
    const auto value = handle(id);
    return value ? registry_->get<HierarchyComponent>(*value).parent : std::nullopt;
}

std::optional<TransformComponent> Scene::transform(const EntityId& id) const {
    const auto value = handle(id); return value ? std::optional<TransformComponent>(registry_->get<TransformComponent>(*value)) : std::nullopt;
}

std::optional<WorldPlacementComponent> Scene::placement(const EntityId& id) const {
    const auto value = handle(id); if (!value || !registry_->all_of<WorldPlacementComponent>(*value)) return std::nullopt; return registry_->get<WorldPlacementComponent>(*value);
}
bool Scene::has_children(const EntityId& id) const { for(const auto& entry:entities_){const auto& h=registry_->get<HierarchyComponent>(entry.second);if(h.parent&&*h.parent==id)return true;}return false; }

std::vector<EntityId> Scene::entity_ids() const {
    std::vector<EntityId> ids;
    ids.reserve(entities_.size());
    for (const auto& entry : entities_) {
        auto parsed = EntityId::parse(entry.first);
        if (parsed) ids.push_back(parsed.value());
    }
    return ids;
}

std::vector<EngineError> Scene::validate() const {
    std::vector<EngineError> errors;
    for (const auto& entry : entities_) {
        const auto entity = entry.second;
        const auto& id = registry_->get<IdComponent>(entity).id;
        if (id.str() != entry.first) errors.push_back(world_error("WORLD-ID-INDEX-MISMATCH", "Entity index differs from ID component"));
        const auto& transform = registry_->get<TransformComponent>(entity);
        if (!finite(transform)) errors.push_back(world_error("WORLD-TRANSFORM-NONFINITE", "Entity transform is not finite: " + id.str()));
        for (const float scale : transform.scale)
            if (std::abs(scale) < 0.000001f) { errors.push_back(world_error("WORLD-TRANSFORM-ZERO-SCALE", "Entity transform has zero scale: " + id.str())); break; }
        const auto parent_value = registry_->get<HierarchyComponent>(entity).parent;
        if (registry_->all_of<WorldPlacementComponent>(entity)) {
            const auto& placement = registry_->get<WorldPlacementComponent>(entity);
            WorldPartition partition; auto expected = partition.cell_for({transform.position[0], transform.position[1], transform.position[2]});
            if (placement.prefab_asset.empty() || placement.prefab_asset.rfind("assets/", 0) != 0) errors.push_back(world_error("WORLD-PREFAB-PATH-INVALID", "Placement has an invalid prefab path: " + id.str()));
            if (!expected || expected.value() != placement.cell) errors.push_back(world_error("WORLD-PLACEMENT-CELL-MISMATCH", "Placement cell does not match transform: " + id.str()));
        }
        if (parent_value && !contains(*parent_value)) errors.push_back(world_error("WORLD-PARENT-NOT-FOUND", "Entity references missing parent: " + id.str()));
        std::set<std::string> visited{id.str()};
        auto cursor = parent_value;
        while (cursor) {
            if (!visited.insert(cursor->str()).second) { errors.push_back(world_error("WORLD-HIERARCHY-CYCLE", "Hierarchy cycle includes: " + id.str())); break; }
            cursor = parent(*cursor);
        }
    }
    return errors;
}

std::string Scene::to_json() const {
    Json root{{"schemaVersion", 1}, {"entities", Json::array()}};
    if(world_id_)root["worldId"]=*world_id_;if(document_name_)root["name"]=*document_name_;if(world_size_meters_&&cell_size_meters_)root["partition"]={{"worldSizeMeters",*world_size_meters_},{"cellSizeMeters",*cell_size_meters_}};
    for (const auto& entry : entities_) {
        const auto entity = entry.second;
        const auto& transform = registry_->get<TransformComponent>(entity);
        const auto& hierarchy = registry_->get<HierarchyComponent>(entity);
        Json item{{"id", entry.first}, {"name", registry_->get<NameComponent>(entity).name},
                  {"transform", {{"position", transform.position}, {"rotation", transform.rotation}, {"scale", transform.scale}}}};
        item["parent"] = hierarchy.parent ? Json(hierarchy.parent->str()) : Json(nullptr);
        if (registry_->all_of<WorldPlacementComponent>(entity)) {
            const auto& placement = registry_->get<WorldPlacementComponent>(entity);
            Json placement_json{{"prefab", placement.prefab_asset}, {"cell", {placement.cell.x, placement.cell.z}}};
            if (placement.character_asset) placement_json["characterAsset"] = *placement.character_asset;
            if (placement.character_settings)
                placement_json["characterSettings"] = nlohmann::json::parse(placement.character_settings->to_json());
            item["placement"] = std::move(placement_json);
        }
        if (registry_->all_of<AuthoredComponentsComponent>(entity)) {
            const auto& components = registry_->get<AuthoredComponentsComponent>(entity);
            if (!components.entries.empty())
                item["components"] = nlohmann::json::parse(authored_components_to_json(components));
        }
        root["entities"].push_back(std::move(item));
    }
    return root.dump(2) + "\n";
}

Result<Scene> Scene::from_json(const std::string& text) {
    try {
        const auto root = Json::parse(text);
        if (root.value("schemaVersion", 0) != 1 || !root.contains("entities") || !root["entities"].is_array())
            return Result<Scene>::failure(world_error("WORLD-SCHEMA-INVALID", "Scene requires schemaVersion 1 and an entities array"));
        Scene scene;
        if(root.contains("worldId"))scene.world_id_=root.at("worldId").get<std::string>();if(root.contains("name"))scene.document_name_=root.at("name").get<std::string>();if(root.contains("partition")){const auto& partition=root.at("partition");scene.world_size_meters_=partition.at("worldSizeMeters").get<std::array<double,2>>();scene.cell_size_meters_=partition.at("cellSizeMeters").get<double>();}
        struct PendingParent { EntityId child; std::optional<EntityId> parent; };
        std::vector<PendingParent> parents;
        for (const auto& item : root["entities"]) {
            auto id = EntityId::parse(item.at("id").get<std::string>());
            if (!id) return Result<Scene>::failure(id.error());
            auto created = scene.create_entity(item.value("name", "Entity"), id.value());
            if (!created) return Result<Scene>::failure(created.error());
            const auto& value = item.at("transform");
            TransformComponent transform{value.at("position").get<std::array<float, 3>>(),
                                         value.at("rotation").get<std::array<float, 4>>(),
                                         value.at("scale").get<std::array<float, 3>>()};
            auto transformed = scene.set_transform(id.value(), transform);
            if (!transformed) return Result<Scene>::failure(transformed.error());
            if (item.contains("placement")) {
                const auto& placement = item.at("placement");
                const auto cell = placement.at("cell").get<std::array<std::int32_t, 2>>();
                std::optional<std::string> character_asset;
                if (placement.contains("characterAsset") && placement["characterAsset"].is_string())
                    character_asset = placement["characterAsset"].get<std::string>();
                std::optional<CharacterAsset> character_settings;
                if (placement.contains("characterSettings")) {
                    const auto parsed = CharacterAsset::from_json(placement["characterSettings"].dump());
                    if (!parsed) return Result<Scene>::failure(parsed.error());
                    character_settings = parsed.value();
                }
                scene.registry_->emplace<WorldPlacementComponent>(
                    *scene.handle(id.value()),
                    WorldPlacementComponent{placement.at("prefab").get<std::string>(), {cell[0], cell[1]},
                        std::move(character_asset), std::move(character_settings)});
            }
            if (item.contains("components")) {
                const auto parsed = authored_components_from_json(item.at("components").dump());
                if (!parsed) return Result<Scene>::failure(parsed.error());
                scene.registry_->emplace_or_replace<AuthoredComponentsComponent>(*scene.handle(id.value()), parsed.value());
            }
            std::optional<EntityId> parent_id;
            if (item.contains("parent") && !item["parent"].is_null()) {
                auto parsed_parent = EntityId::parse(item["parent"].get<std::string>());
                if (!parsed_parent) return Result<Scene>::failure(parsed_parent.error());
                parent_id = parsed_parent.value();
            }
            parents.push_back({id.value(), parent_id});
        }
        for (const auto& value : parents) {
            auto parented = scene.set_parent(value.child, value.parent);
            if (!parented) return Result<Scene>::failure(parented.error());
        }
        const auto errors = scene.validate();
        if (!errors.empty()) return Result<Scene>::failure(errors.front());
        return Result<Scene>::success(std::move(scene));
    } catch (const std::exception& exception) {
        EngineError error = world_error("WORLD-JSON-INVALID", "Could not parse scene JSON", "Correct malformed or missing scene fields.");
        error.causes.push_back(exception.what());
        return Result<Scene>::failure(std::move(error));
    }
}

Result<Scene> Scene::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return Result<Scene>::failure(world_error("WORLD-FILE-READ", "Could not open scene: " + path.generic_string()));
    return from_json(std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()));
}

Result<void> Scene::save_atomic(const std::filesystem::path& path) const {
    try {
        if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
        const auto temporary = path.string() + ".tmp";
        { std::ofstream output(temporary, std::ios::binary | std::ios::trunc); output << to_json(); if (!output) throw std::runtime_error("write failed"); }
        const std::filesystem::path temporary_path(temporary);
        if (std::filesystem::exists(path)) {
            const std::filesystem::path backup(path.string() + ".bak");
            if (!ReplaceFileW(path.c_str(), temporary_path.c_str(), backup.c_str(), REPLACEFILE_WRITE_THROUGH, nullptr, nullptr))
                throw std::runtime_error("ReplaceFileW failed with " + std::to_string(GetLastError()));
        } else if (!MoveFileExW(temporary_path.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH)) {
            throw std::runtime_error("MoveFileExW failed with " + std::to_string(GetLastError()));
        }
        return Result<void>::success();
    } catch (const std::exception& exception) {
        EngineError error = world_error("WORLD-FILE-WRITE", "Could not atomically save scene", "Check permissions and free space; the previous file is preserved as .bak when available.");
        error.causes.push_back(exception.what());
        return Result<void>::failure(std::move(error));
    }
}

} // namespace engine
