#include "engine/automation/scene_commands.h"

#include <algorithm>

namespace engine {
namespace {
EngineError history_error(std::string code, std::string message) {
    return EngineError{std::move(code), Severity::Warning, ErrorCategory::Validation, "automation", std::move(message),
                       ENGINE_SOURCE_CONTEXT, {}, "Check command-history availability before invoking this operation.", make_correlation_id()};
}
}

RenameEntityCommand::RenameEntityCommand(EntityId id, std::string new_name) : id_(std::move(id)), new_name_(std::move(new_name)) {}
Result<void> RenameEntityCommand::apply(Scene& scene) {
    if (!previous_name_) {
        previous_name_ = scene.name(id_);
        if (!previous_name_) return Result<void>::failure(history_error("COMMAND-ENTITY-NOT-FOUND", "Rename target does not exist"));
    }
    return scene.rename_entity(id_, new_name_);
}
Result<void> RenameEntityCommand::revert(Scene& scene) {
    return previous_name_ ? scene.rename_entity(id_, *previous_name_)
                          : Result<void>::failure(history_error("COMMAND-NOT-APPLIED", "Rename command was never applied"));
}

ReparentEntityCommand::ReparentEntityCommand(EntityId child, std::optional<EntityId> parent)
    : child_(std::move(child)), parent_(std::move(parent)) {}
Result<void> ReparentEntityCommand::apply(Scene& scene) {
    if (!previous_captured_) { previous_parent_ = scene.parent(child_); previous_captured_ = true; }
    return scene.set_parent(child_, parent_);
}
Result<void> ReparentEntityCommand::revert(Scene& scene) {
    return previous_captured_ ? scene.set_parent(child_, previous_parent_)
                              : Result<void>::failure(history_error("COMMAND-NOT-APPLIED", "Reparent command was never applied"));
}

PlaceWorldObjectCommand::PlaceWorldObjectCommand(std::string name, std::string prefab_asset, TransformComponent transform,
    std::optional<EntityId> requested_id, std::optional<std::string> character_asset, std::optional<PrefabAsset> seed_prefab)
    : name_(std::move(name)), prefab_asset_(std::move(prefab_asset)), transform_(transform), id_(std::move(requested_id)),
      character_asset_(std::move(character_asset)), seed_prefab_(std::move(seed_prefab)) {}
Result<void> PlaceWorldObjectCommand::apply(Scene& scene) {
    auto placed = scene.place_world_object(name_, prefab_asset_, transform_, id_, character_asset_,
        seed_prefab_ ? &*seed_prefab_ : nullptr);
    if (!placed) return Result<void>::failure(placed.error());
    id_ = placed.value();
    return Result<void>::success();
}
Result<void> PlaceWorldObjectCommand::revert(Scene& scene){return id_?scene.destroy_entity(*id_):Result<void>::failure(history_error("COMMAND-NOT-APPLIED","Place command was never applied"));}

MoveWorldObjectCommand::MoveWorldObjectCommand(EntityId id,TransformComponent transform):id_(std::move(id)),transform_(transform){}
Result<void> MoveWorldObjectCommand::apply(Scene& scene){if(!previous_){previous_=scene.transform(id_);if(!previous_)return Result<void>::failure(history_error("COMMAND-ENTITY-NOT-FOUND","Move target does not exist"));}return scene.move_world_object(id_,transform_);}
Result<void> MoveWorldObjectCommand::revert(Scene& scene){return previous_?scene.move_world_object(id_,*previous_):Result<void>::failure(history_error("COMMAND-NOT-APPLIED","Move command was never applied"));}

RemoveWorldObjectCommand::RemoveWorldObjectCommand(EntityId id):id_(std::move(id)){}
Result<void> RemoveWorldObjectCommand::apply(Scene& scene){if(scene.has_children(id_))return Result<void>::failure(history_error("COMMAND-REMOVE-HAS-CHILDREN","Cannot remove a placed object with children"));if(!captured_){name_=scene.name(id_);transform_=scene.transform(id_);placement_=scene.placement(id_);components_=scene.authored_components(id_);parent_=scene.parent(id_);captured_=true;}if(!name_||!transform_||!placement_)return Result<void>::failure(history_error("COMMAND-PLACEMENT-NOT-FOUND","Remove target is not a placed world object"));return scene.destroy_entity(id_);}
Result<void> RemoveWorldObjectCommand::revert(Scene& scene){if(!captured_||!name_||!transform_||!placement_)return Result<void>::failure(history_error("COMMAND-NOT-APPLIED","Remove command was never applied"));auto restored=scene.place_world_object(*name_,placement_->prefab_asset,*transform_,id_,placement_->character_asset);if(!restored)return Result<void>::failure(restored.error());if(placement_->character_settings){const auto settings=scene.set_placement_character_settings(id_,placement_->character_settings);if(!settings){(void)scene.destroy_entity(id_);return settings;}}if(components_){const auto set=scene.set_authored_components(id_,*components_);if(!set){(void)scene.destroy_entity(id_);return set;}}if(parent_){auto parented=scene.set_parent(id_,parent_);if(!parented){(void)scene.destroy_entity(id_);return parented;}}return Result<void>::success();}

SetPlacementCharacterSettingsCommand::SetPlacementCharacterSettingsCommand(EntityId id,
    std::optional<CharacterAsset> character_settings)
    : id_(std::move(id)), character_settings_(std::move(character_settings)) {}
Result<void> SetPlacementCharacterSettingsCommand::apply(Scene& scene) {
    if (!previous_captured_) {
        const auto placement = scene.placement(id_);
        if (!placement) return Result<void>::failure(history_error("COMMAND-PLACEMENT-NOT-FOUND", "Character settings target is not a placed world object"));
        previous_settings_ = placement->character_settings;
        previous_captured_ = true;
    }
    return scene.set_placement_character_settings(id_, character_settings_);
}
Result<void> SetPlacementCharacterSettingsCommand::revert(Scene& scene) {
    return previous_captured_ ? scene.set_placement_character_settings(id_, previous_settings_)
                                : Result<void>::failure(history_error("COMMAND-NOT-APPLIED", "Character settings command was never applied"));
}

AddEntityComponentCommand::AddEntityComponentCommand(EntityId id, AuthoredComponentEntry entry)
    : id_(std::move(id)), entry_(std::move(entry)) {}
Result<void> AddEntityComponentCommand::apply(Scene& scene) {
    const auto result = scene.add_authored_component(id_, entry_, true);
    if (!result) return result;
    applied_ = true;
    return Result<void>::success();
}
Result<void> AddEntityComponentCommand::revert(Scene& scene) {
    return applied_ ? scene.remove_authored_component(id_, entry_.id)
                    : Result<void>::failure(history_error("COMMAND-NOT-APPLIED", "Add component command was never applied"));
}

RemoveEntityComponentCommand::RemoveEntityComponentCommand(EntityId id, std::string component_id)
    : id_(std::move(id)), component_id_(std::move(component_id)) {}
Result<void> RemoveEntityComponentCommand::apply(Scene& scene) {
    if (!previous_) {
        const auto components = scene.authored_components(id_);
        if (!components) return Result<void>::failure(history_error("COMMAND-COMPONENT-NOT-FOUND", "Entity has no components"));
        for (const auto& entry : components->entries) {
            if (entry.id == component_id_) {
                previous_ = entry;
                break;
            }
        }
        if (!previous_) return Result<void>::failure(history_error("COMMAND-COMPONENT-NOT-FOUND", "Component id not found"));
    }
    return scene.remove_authored_component(id_, component_id_);
}
Result<void> RemoveEntityComponentCommand::revert(Scene& scene) {
    return previous_ ? scene.add_authored_component(id_, *previous_, false)
                     : Result<void>::failure(history_error("COMMAND-NOT-APPLIED", "Remove component command was never applied"));
}

SetEntityComponentCommand::SetEntityComponentCommand(EntityId id, AuthoredComponentEntry entry)
    : id_(std::move(id)), entry_(std::move(entry)) {}
Result<void> SetEntityComponentCommand::apply(Scene& scene) {
    if (!applied_) {
        const auto components = scene.authored_components(id_);
        if (components) {
            for (const auto& existing : components->entries) {
                if (existing.id == entry_.id) {
                    previous_ = existing;
                    had_previous_ = true;
                    break;
                }
            }
        }
        applied_ = true;
    }
    return scene.set_authored_component(id_, entry_, true);
}
Result<void> SetEntityComponentCommand::revert(Scene& scene) {
    if (!applied_) return Result<void>::failure(history_error("COMMAND-NOT-APPLIED", "Set component command was never applied"));
    if (had_previous_ && previous_) return scene.set_authored_component(id_, *previous_, false);
    return scene.remove_authored_component(id_, entry_.id);
}

CompositeSceneCommand::CompositeSceneCommand(std::string label, std::vector<std::unique_ptr<SceneCommand>> commands)
    : label_(label.empty() ? "batch-scene-edit" : std::move(label)), commands_(std::move(commands)) {}

Result<void> CompositeSceneCommand::apply(Scene& scene) {
    applied_count_ = 0;
    for (const auto& command : commands_) {
        const auto applied = command->apply(scene);
        if (!applied) {
            for (std::size_t index = applied_count_; index > 0; --index) {
                const auto reverted = commands_[index - 1]->revert(scene);
                if (!reverted) return reverted;
            }
            applied_count_ = 0;
            return applied;
        }
        ++applied_count_;
    }
    return Result<void>::success();
}

Result<void> CompositeSceneCommand::revert(Scene& scene) {
    if (applied_count_ == 0 && !commands_.empty()) applied_count_ = commands_.size();
    for (std::size_t index = applied_count_; index > 0; --index) {
        const auto reverted = commands_[index - 1]->revert(scene);
        if (!reverted) return reverted;
    }
    applied_count_ = 0;
    return Result<void>::success();
}

std::vector<std::string> CompositeSceneCommand::changed_object_ids() const {
    std::vector<std::string> changed;
    for (const auto& command : commands_) {
        for (const auto& id : command->changed_object_ids()) {
            if (std::find(changed.begin(), changed.end(), id) == changed.end()) changed.push_back(id);
        }
    }
    return changed;
}

Result<void> CommandHistory::execute(Scene& scene, std::unique_ptr<SceneCommand> command) {
    if (!command) return Result<void>::failure(history_error("COMMAND-NULL", "Cannot execute a null command"));
    auto applied = command->apply(scene);
    if (!applied) return applied;
    last_summary_ = command->label(); last_changed_object_ids_ = command->changed_object_ids();
    undo_.push_back(std::move(command));
    redo_.clear();
    return Result<void>::success();
}
Result<void> CommandHistory::undo(Scene& scene) {
    if (undo_.empty()) return Result<void>::failure(history_error("COMMAND-NOTHING-TO-UNDO", "Undo history is empty"));
    auto command = std::move(undo_.back()); undo_.pop_back();
    auto reverted = command->revert(scene);
    if (!reverted) { undo_.push_back(std::move(command)); return reverted; }
    last_summary_ = "undo:" + command->label(); last_changed_object_ids_ = command->changed_object_ids(); redo_.push_back(std::move(command));
    return Result<void>::success();
}
Result<void> CommandHistory::redo(Scene& scene) {
    if (redo_.empty()) return Result<void>::failure(history_error("COMMAND-NOTHING-TO-REDO", "Redo history is empty"));
    auto command = std::move(redo_.back()); redo_.pop_back();
    auto applied = command->apply(scene);
    if (!applied) { redo_.push_back(std::move(command)); return applied; }
    last_summary_ = "redo:" + command->label(); last_changed_object_ids_ = command->changed_object_ids(); undo_.push_back(std::move(command));
    return Result<void>::success();
}

} // namespace engine
