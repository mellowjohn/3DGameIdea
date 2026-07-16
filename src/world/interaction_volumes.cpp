#include "engine/world/interaction_volumes.h"

namespace engine {

void InteractionVolumeRegistry::bind(CollisionBody body, InteractionVolumeBinding binding) {
    if (!body.valid()) return;
    bindings_[body.value] = std::move(binding);
}

void InteractionVolumeRegistry::unbind(CollisionBody body) {
    if (!body.valid()) return;
    bindings_.erase(body.value);
}

void InteractionVolumeRegistry::clear() { bindings_.clear(); }

std::optional<InteractionVolumeBinding> InteractionVolumeRegistry::find(CollisionBody body) const {
    if (!body.valid()) return std::nullopt;
    const auto found = bindings_.find(body.value);
    return found == bindings_.end() ? std::nullopt : std::optional<InteractionVolumeBinding>(found->second);
}

bool InteractionVolumeRegistry::is_interaction_body(CollisionBody body) const {
    return body.valid() && bindings_.find(body.value) != bindings_.end();
}

std::vector<InteractionEvent> InteractionOverlapTracker::update(const std::string& interactor_id, WorldPosition center,
    float radius, const CollisionWorld& world, const InteractionVolumeRegistry& registry) {
    std::vector<InteractionEvent> events;
    if (interactor_id.empty() || !(radius > 0)) return events;

    CollisionQueryFilter filter;
    filter.layer = CollisionLayer::Trigger;
    const auto overlaps = world.overlap_sphere(center, radius, filter);
    if (!overlaps) return events;

    std::set<std::uint32_t> current;
    for (const auto& hit : overlaps.value()) {
        if (!registry.is_interaction_body(hit.body)) continue;
        current.insert(hit.body.value);
        const auto binding = registry.find(hit.body);
        if (!binding) continue;
        auto& active = active_by_interactor_[interactor_id];
        if (active.find(hit.body.value) != active.end()) continue;
        InteractionEvent event;
        event.type = InteractionEventType::Enter;
        event.placement_entity_id = binding->placement_entity_id;
        event.interaction_id = binding->interaction_id;
        event.volume_index = binding->volume_index;
        event.interactor_id = interactor_id;
        event.contact_point = hit.contact_point;
        events.push_back(event);
        active.insert(hit.body.value);
    }

    auto& active = active_by_interactor_[interactor_id];
    std::vector<std::uint32_t> to_remove;
    for (const auto token : active) {
        if (current.find(token) != current.end()) continue;
        const auto binding = registry.find({token});
        if (!binding) {
            to_remove.push_back(token);
            continue;
        }
        InteractionEvent event;
        event.type = InteractionEventType::Exit;
        event.placement_entity_id = binding->placement_entity_id;
        event.interaction_id = binding->interaction_id;
        event.volume_index = binding->volume_index;
        event.interactor_id = interactor_id;
        events.push_back(event);
        to_remove.push_back(token);
    }
    for (const auto token : to_remove) active.erase(token);
    return events;
}

void InteractionOverlapTracker::reset(const std::string& interactor_id) { active_by_interactor_.erase(interactor_id); }

} // namespace engine
