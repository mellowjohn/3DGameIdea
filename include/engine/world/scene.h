#pragma once

#include "engine/world/authored_components.h"
#include "engine/world/components.h"
#include "engine/world/world_partition.h"

#include "engine/assets/prefab_asset.h"

#include <entt/entity/registry.hpp>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace engine {

/// How a `.world.json` is presented at runtime (thin cinematic instances vs open world).
enum class WorldPresentation : std::uint8_t {
    OpenWorld = 0,
    Menu,
    CinematicInstance,
};

/// Optional terrain stores owned by a self-contained world presentation.
/// Paths are project-relative and intentionally keep height, paint, and foliage
/// independently diffable.
struct WorldTerrainDataPaths {
    std::string edits;
    std::string paint;
    std::string foliage;
};

class Scene final {
public:
    Scene();
    Scene(Scene&& other) noexcept;
    Scene& operator=(Scene&& other) noexcept;
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    [[nodiscard]] Result<EntityId> create_entity(std::string name, std::optional<EntityId> requested_id = std::nullopt);
    [[nodiscard]] Result<void> destroy_entity(const EntityId& id);
    [[nodiscard]] Result<void> rename_entity(const EntityId& id, std::string name);
    [[nodiscard]] Result<void> set_parent(const EntityId& child, std::optional<EntityId> parent);
    /**
     * @param bump_edit_revision When false, updates the pose without invalidating placement-collision sync
     * (physics write-back, play-test player visual follow). Editor/gizmo/MCP transforms keep the default so
     * collision bodies stay in sync. World placements also retag `placement.cell` from the new position so
     * save/load cannot persist WORLD-PLACEMENT-CELL-MISMATCH after physics follow or End Test restore.
     */
    [[nodiscard]] Result<void> set_transform(const EntityId& id, const TransformComponent& transform,
        bool bump_edit_revision = true);
    [[nodiscard]] Result<EntityId> place_world_object(std::string name, std::string prefab_asset,
        const TransformComponent& transform, std::optional<EntityId> requested_id = std::nullopt,
        std::optional<std::string> character_asset = std::nullopt, const PrefabAsset* seed_prefab = nullptr);
    [[nodiscard]] Result<void> set_placement_character_asset(const EntityId& id, std::optional<std::string> character_asset);
    [[nodiscard]] Result<void> set_placement_character_settings(const EntityId& id, std::optional<CharacterAsset> character_settings);
    [[nodiscard]] Result<void> move_world_object(const EntityId& id, const TransformComponent& transform);
    [[nodiscard]] Result<void> remap_asset_path_prefix(const std::string& old_prefix, const std::string& new_prefix);
    [[nodiscard]] Result<void> repair_prefab_paths(const std::map<std::string, PrefabAsset>& catalog);
    [[nodiscard]] Result<std::vector<EntityId>> instantiate_prefab(const Scene& prefab, std::optional<EntityId> parent = std::nullopt);

    [[nodiscard]] Result<void> set_authored_components(const EntityId& id, AuthoredComponentsComponent components);
    /// Seeds prefab collision/scriptBinding entries when the entity has none. Returns true if newly seeded.
    [[nodiscard]] Result<bool> ensure_authored_components_seeded(const EntityId& id, const PrefabAsset& prefab);
    /// Seeds missing authored components for every placed object from the prefab catalog. Returns count seeded.
    [[nodiscard]] std::size_t seed_missing_authored_components(const std::map<std::string, PrefabAsset>& catalog);
    [[nodiscard]] Result<void> add_authored_component(const EntityId& id, AuthoredComponentEntry entry, bool mark_override = true);
    [[nodiscard]] Result<void> remove_authored_component(const EntityId& id, const std::string& component_id);
    [[nodiscard]] Result<void> set_authored_component(const EntityId& id, AuthoredComponentEntry entry, bool mark_override = true);
    [[nodiscard]] std::size_t propagate_prefab_components(const std::string& prefab_asset, const PrefabAsset& prefab);

    [[nodiscard]] bool contains(const EntityId& id) const;
    [[nodiscard]] std::optional<std::string> name(const EntityId& id) const;
    [[nodiscard]] std::optional<EntityId> parent(const EntityId& id) const;
    [[nodiscard]] std::optional<TransformComponent> transform(const EntityId& id) const;
    [[nodiscard]] std::optional<WorldPlacementComponent> placement(const EntityId& id) const;
    [[nodiscard]] std::optional<AuthoredComponentsComponent> authored_components(const EntityId& id) const;
    [[nodiscard]] bool has_children(const EntityId& id) const;
    [[nodiscard]] std::size_t size() const noexcept { return entities_.size(); }
    /** Monotonic counter bumped by authoring edits that can change collision bodies. */
    [[nodiscard]] std::uint64_t edit_revision() const noexcept { return edit_revision_; }
    [[nodiscard]] std::vector<EntityId> entity_ids() const;
    [[nodiscard]] std::vector<EngineError> validate() const;
    [[nodiscard]] std::string to_json() const;
    /** Partition bounds from authored `worldSizeMeters` / `cellSizeMeters` (DEC-0054 defaults when unset). */
    [[nodiscard]] PartitionConfig partition_config() const;
    /** Optional `presentation` on the world document; defaults to OpenWorld. */
    [[nodiscard]] WorldPresentation presentation() const noexcept { return presentation_; }
    void set_presentation(WorldPresentation presentation) noexcept { presentation_ = presentation; }
    [[nodiscard]] const std::optional<WorldTerrainDataPaths>& terrain_data_paths() const noexcept { return terrain_data_paths_; }
    void set_terrain_data_paths(std::optional<WorldTerrainDataPaths> paths) { terrain_data_paths_ = std::move(paths); }

    [[nodiscard]] static Result<Scene> from_json(const std::string& json);
    [[nodiscard]] static Result<Scene> load(const std::filesystem::path& path);
    [[nodiscard]] Result<void> save_atomic(const std::filesystem::path& path) const;

private:
    [[nodiscard]] std::optional<entt::entity> handle(const EntityId& id) const;
    void bump_edit_revision() noexcept { ++edit_revision_; }
    std::unique_ptr<entt::registry> registry_;
    std::map<std::string, entt::entity> entities_;
    std::uint64_t edit_revision_ = 0;
    std::optional<std::string> world_id_;
    std::optional<std::string> document_name_;
    std::optional<std::array<double, 2>> world_size_meters_;
    std::optional<double> cell_size_meters_;
    WorldPresentation presentation_ = WorldPresentation::OpenWorld;
    std::optional<WorldTerrainDataPaths> terrain_data_paths_;
};

} // namespace engine
