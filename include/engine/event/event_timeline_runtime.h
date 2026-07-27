#pragma once

#include "engine/assets/world_forge_events_asset.h"
#include "engine/core/result.h"

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace engine {

struct EventTimelineEmitEvent {
    std::string sequence_id;
    std::string name;
    std::string payload_json = "{}";
};

/** Active look_at directive for the host camera (TICKET-0222).
 *  Host blends orbit pivot to `target` (subject focus) using `alpha`; optional distance/pitch frame the shot. */
struct EventTimelineCameraDirective {
    bool active = false;
    std::array<float, 3> target{0.0f, 0.0f, 0.0f};
    float distance = 0.0f; // <=0 keep current
    float pitch = 0.0f;
    bool has_pitch = false;
    float alpha = 0.0f; // 0..1 blend through the look_at step
};

/// Headless event timeline sequencer (TICKET-0221 / DEC-0045).
/// Hosts may inject a dialogue starter so start_dialogue steps reach DialogueRuntime.
class EventTimelineRuntime {
public:
    using DialogueStarter = std::function<Result<void>(const std::string& dialogue_id)>;

    [[nodiscard]] Result<void> bind(const WorldForgeEventsAsset* asset);
    void set_dialogue_starter(DialogueStarter starter);
    [[nodiscard]] Result<void> start(const std::string& sequence_id);
    void cancel() noexcept;
    void tick(float dt_seconds);
    [[nodiscard]] bool is_active() const noexcept { return active_; }
    [[nodiscard]] bool is_complete() const noexcept { return complete_; }
    [[nodiscard]] bool control_locked() const noexcept { return control_locked_; }
    [[nodiscard]] EventTimelineCameraDirective camera_directive() const noexcept { return camera_; }
    [[nodiscard]] const std::string& sequence_id() const noexcept { return sequence_id_; }
    [[nodiscard]] std::size_t step_index() const noexcept { return step_index_; }
    [[nodiscard]] std::vector<EventTimelineEmitEvent> take_emitted_events();
    [[nodiscard]] const std::vector<EngineError>& recent_errors() const noexcept { return recent_errors_; }
    void clear_recent_errors() { recent_errors_.clear(); }
    void reset() noexcept;

private:
    void advance_instant_steps();
    void begin_look_at(const EventTimelineStep& step);
    void push_error(EngineError error);

    const WorldForgeEventsAsset* asset_ = nullptr;
    DialogueStarter dialogue_starter_;
    std::string sequence_id_;
    std::size_t step_index_ = 0;
    float wait_remaining_ = 0.0f;
    float wait_duration_ = 0.0f;
    bool waiting_ = false;
    bool active_ = false;
    bool complete_ = false;
    bool control_locked_ = false;
    EventTimelineCameraDirective camera_{};
    std::vector<EventTimelineEmitEvent> emitted_;
    std::vector<EngineError> recent_errors_;
};

} // namespace engine
