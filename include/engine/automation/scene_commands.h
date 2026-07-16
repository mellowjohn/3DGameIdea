#pragma once

#include "engine/world/scene.h"

#include <memory>
#include <string>
#include <vector>

namespace engine {

class SceneCommand {
public:
    virtual ~SceneCommand() = default;
    [[nodiscard]] virtual Result<void> apply(Scene& scene) = 0;
    [[nodiscard]] virtual Result<void> revert(Scene& scene) = 0;
    [[nodiscard]] virtual std::string label() const = 0;
    [[nodiscard]] virtual std::vector<std::string> changed_object_ids() const = 0;
};

class RenameEntityCommand final : public SceneCommand {
public:
    RenameEntityCommand(EntityId id, std::string new_name);
    Result<void> apply(Scene& scene) override;
    Result<void> revert(Scene& scene) override;
    std::string label() const override { return "rename-entity"; }
    std::vector<std::string> changed_object_ids() const override { return {id_.str()}; }
private:
    EntityId id_;
    std::string new_name_;
    std::optional<std::string> previous_name_;
};

class ReparentEntityCommand final : public SceneCommand {
public:
    ReparentEntityCommand(EntityId child, std::optional<EntityId> parent);
    Result<void> apply(Scene& scene) override;
    Result<void> revert(Scene& scene) override;
    std::string label() const override { return "reparent-entity"; }
    std::vector<std::string> changed_object_ids() const override { return {child_.str()}; }
private:
    EntityId child_;
    std::optional<EntityId> parent_;
    std::optional<EntityId> previous_parent_;
    bool previous_captured_ = false;
};

class PlaceWorldObjectCommand final : public SceneCommand {
public:
    PlaceWorldObjectCommand(std::string name, std::string prefab_asset, TransformComponent transform, std::optional<EntityId> requested_id = std::nullopt, std::optional<std::string> character_asset = std::nullopt, std::optional<PrefabAsset> seed_prefab = std::nullopt);
    Result<void> apply(Scene& scene) override;
    Result<void> revert(Scene& scene) override;
    std::string label() const override { return "place-world-object"; }
    std::vector<std::string> changed_object_ids() const override { return id_ ? std::vector<std::string>{id_->str()} : std::vector<std::string>{}; }
    [[nodiscard]] const std::optional<EntityId>& placed_id() const { return id_; }
private:
    std::string name_, prefab_asset_;
    TransformComponent transform_;
    std::optional<EntityId> id_;
    std::optional<std::string> character_asset_;
    std::optional<PrefabAsset> seed_prefab_;
};

class MoveWorldObjectCommand final : public SceneCommand {
public:
    MoveWorldObjectCommand(EntityId id, TransformComponent transform);
    Result<void> apply(Scene& scene) override;
    Result<void> revert(Scene& scene) override;
    std::string label() const override { return "move-world-object"; }
    std::vector<std::string> changed_object_ids() const override { return {id_.str()}; }
private:
    EntityId id_; TransformComponent transform_; std::optional<TransformComponent> previous_;
};

class RemoveWorldObjectCommand final : public SceneCommand {
public:
    explicit RemoveWorldObjectCommand(EntityId id);
    Result<void> apply(Scene& scene) override;
    Result<void> revert(Scene& scene) override;
    std::string label() const override { return "remove-world-object"; }
    std::vector<std::string> changed_object_ids() const override { return {id_.str()}; }
private:
    EntityId id_; std::optional<std::string> name_; std::optional<TransformComponent> transform_; std::optional<WorldPlacementComponent> placement_; std::optional<AuthoredComponentsComponent> components_; std::optional<EntityId> parent_; bool captured_=false;
};

class SetPlacementCharacterSettingsCommand final : public SceneCommand {
public:
    SetPlacementCharacterSettingsCommand(EntityId id, std::optional<CharacterAsset> character_settings);
    Result<void> apply(Scene& scene) override;
    Result<void> revert(Scene& scene) override;
    std::string label() const override { return "set-placement-character-settings"; }
    std::vector<std::string> changed_object_ids() const override { return {id_.str()}; }
private:
    EntityId id_;
    std::optional<CharacterAsset> character_settings_;
    std::optional<CharacterAsset> previous_settings_;
    bool previous_captured_ = false;
};

class AddEntityComponentCommand final : public SceneCommand {
public:
    AddEntityComponentCommand(EntityId id, AuthoredComponentEntry entry);
    Result<void> apply(Scene& scene) override;
    Result<void> revert(Scene& scene) override;
    std::string label() const override { return "add-entity-component"; }
    std::vector<std::string> changed_object_ids() const override { return {id_.str()}; }
private:
    EntityId id_;
    AuthoredComponentEntry entry_;
    bool applied_ = false;
};

class RemoveEntityComponentCommand final : public SceneCommand {
public:
    RemoveEntityComponentCommand(EntityId id, std::string component_id);
    Result<void> apply(Scene& scene) override;
    Result<void> revert(Scene& scene) override;
    std::string label() const override { return "remove-entity-component"; }
    std::vector<std::string> changed_object_ids() const override { return {id_.str()}; }
private:
    EntityId id_;
    std::string component_id_;
    std::optional<AuthoredComponentEntry> previous_;
};

class SetEntityComponentCommand final : public SceneCommand {
public:
    SetEntityComponentCommand(EntityId id, AuthoredComponentEntry entry);
    Result<void> apply(Scene& scene) override;
    Result<void> revert(Scene& scene) override;
    std::string label() const override { return "set-entity-component"; }
    std::vector<std::string> changed_object_ids() const override { return {id_.str()}; }
private:
    EntityId id_;
    AuthoredComponentEntry entry_;
    std::optional<AuthoredComponentEntry> previous_;
    bool had_previous_ = false;
    bool applied_ = false;
};

class CompositeSceneCommand final : public SceneCommand {
public:
    CompositeSceneCommand(std::string label, std::vector<std::unique_ptr<SceneCommand>> commands);
    Result<void> apply(Scene& scene) override;
    Result<void> revert(Scene& scene) override;
    std::string label() const override { return label_; }
    std::vector<std::string> changed_object_ids() const override;
private:
    std::string label_;
    std::vector<std::unique_ptr<SceneCommand>> commands_;
    std::size_t applied_count_ = 0;
};

class CommandHistory final {
public:
    [[nodiscard]] Result<void> execute(Scene& scene, std::unique_ptr<SceneCommand> command);
    [[nodiscard]] Result<void> undo(Scene& scene);
    [[nodiscard]] Result<void> redo(Scene& scene);
    [[nodiscard]] std::size_t undo_size() const noexcept { return undo_.size(); }
    [[nodiscard]] std::size_t redo_size() const noexcept { return redo_.size(); }
    [[nodiscard]] const std::string& last_summary() const noexcept { return last_summary_; }
    [[nodiscard]] const std::vector<std::string>& last_changed_object_ids() const noexcept { return last_changed_object_ids_; }
private:
    std::vector<std::unique_ptr<SceneCommand>> undo_;
    std::vector<std::unique_ptr<SceneCommand>> redo_;
    std::string last_summary_;
    std::vector<std::string> last_changed_object_ids_;
};

} // namespace engine
