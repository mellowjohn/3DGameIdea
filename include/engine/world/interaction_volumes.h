#pragma once

#include "engine/physics/collision_world.h"

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace engine {

struct InteractionVolumeBinding {
    std::string placement_entity_id;
    std::uint32_t volume_index = 0;
    std::string interaction_id;
};

enum class InteractionEventType : std::uint8_t { Enter, Exit };

struct InteractionEvent {
    InteractionEventType type = InteractionEventType::Enter;
    std::string placement_entity_id;
    std::string interaction_id;
    std::uint32_t volume_index = 0;
    std::string interactor_id;
    std::optional<WorldPosition> contact_point;
};

class InteractionVolumeRegistry final {
public:
    void bind(CollisionBody body, InteractionVolumeBinding binding);
    void unbind(CollisionBody body);
    void clear();
    [[nodiscard]] std::optional<InteractionVolumeBinding> find(CollisionBody body) const;
    [[nodiscard]] bool is_interaction_body(CollisionBody body) const;

private:
    std::map<std::uint32_t, InteractionVolumeBinding> bindings_;
};

class InteractionOverlapTracker final {
public:
    [[nodiscard]] std::vector<InteractionEvent> update(const std::string& interactor_id, WorldPosition center,
        float radius, const CollisionWorld& world, const InteractionVolumeRegistry& registry);
    void reset(const std::string& interactor_id);

private:
    std::map<std::string, std::set<std::uint32_t>> active_by_interactor_;
};

} // namespace engine
