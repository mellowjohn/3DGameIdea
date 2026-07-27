#include "engine/event/event_timeline_runtime.h"

#include <algorithm>

namespace engine {
namespace {

EngineError rt_error(std::string code, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::InternalInvariant, "event_timeline",
        std::move(message), ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

} // namespace

Result<void> EventTimelineRuntime::bind(const WorldForgeEventsAsset* asset) {
    if (!asset) {
        return Result<void>::failure(rt_error("EVENT-RT-BIND", "EventTimelineRuntime requires an events asset",
            "Call bind with a loaded WorldForgeEventsAsset."));
    }
    reset();
    asset_ = asset;
    return Result<void>::success();
}

void EventTimelineRuntime::set_dialogue_starter(DialogueStarter starter) {
    dialogue_starter_ = std::move(starter);
}

Result<void> EventTimelineRuntime::start(const std::string& sequence_id) {
    if (!asset_) {
        return Result<void>::failure(
            rt_error("EVENT-RT-STATE", "EventTimelineRuntime is not bound", "Call bind before start."));
    }
    if (!asset_->find_sequence(sequence_id)) {
        return Result<void>::failure(rt_error("EVENT-RT-START", "Unknown event sequence id: " + sequence_id,
            "Pass a sequence id from events.worldforge.json."));
    }
    sequence_id_ = sequence_id;
    step_index_ = 0;
    wait_remaining_ = 0.0f;
    wait_duration_ = 0.0f;
    waiting_ = false;
    active_ = true;
    complete_ = false;
    control_locked_ = false;
    camera_ = {};
    emitted_.clear();
    recent_errors_.clear();
    advance_instant_steps();
    return Result<void>::success();
}

void EventTimelineRuntime::cancel() noexcept {
    active_ = false;
    complete_ = false;
    waiting_ = false;
    wait_remaining_ = 0.0f;
    wait_duration_ = 0.0f;
    step_index_ = 0;
    sequence_id_.clear();
    control_locked_ = false;
    camera_ = {};
}

void EventTimelineRuntime::tick(float dt_seconds) {
    if (!active_ || !asset_) return;
    if (dt_seconds < 0.0f) dt_seconds = 0.0f;

    if (waiting_) {
        wait_remaining_ -= dt_seconds;
        if (wait_duration_ > 0.0f && camera_.active) {
            const float elapsed = wait_duration_ - wait_remaining_;
            camera_.alpha = std::clamp(elapsed / wait_duration_, 0.0f, 1.0f);
        }
        if (wait_remaining_ > 0.0f) return;
        waiting_ = false;
        wait_remaining_ = 0.0f;
        wait_duration_ = 0.0f;
        camera_ = {};
        ++step_index_;
    }
    advance_instant_steps();
}

std::vector<EventTimelineEmitEvent> EventTimelineRuntime::take_emitted_events() {
    std::vector<EventTimelineEmitEvent> out;
    out.swap(emitted_);
    return out;
}

void EventTimelineRuntime::reset() noexcept {
    cancel();
    asset_ = nullptr;
    dialogue_starter_ = nullptr;
    emitted_.clear();
    recent_errors_.clear();
}

void EventTimelineRuntime::push_error(EngineError error) {
    recent_errors_.push_back(std::move(error));
}

void EventTimelineRuntime::begin_look_at(const EventTimelineStep& step) {
    camera_.active = true;
    camera_.target = step.look_at_target;
    camera_.distance = step.look_at_distance;
    camera_.pitch = step.look_at_pitch;
    camera_.has_pitch = step.look_at_has_pitch;
    camera_.alpha = 0.0f;
    if (step.seconds <= 0.0f) {
        camera_.alpha = 1.0f;
        waiting_ = false;
        wait_remaining_ = 0.0f;
        wait_duration_ = 0.0f;
        ++step_index_;
        return;
    }
    waiting_ = true;
    wait_duration_ = step.seconds;
    wait_remaining_ = step.seconds;
}

void EventTimelineRuntime::advance_instant_steps() {
    const auto* sequence = asset_ ? asset_->find_sequence(sequence_id_) : nullptr;
    if (!sequence) {
        active_ = false;
        return;
    }

    while (active_ && step_index_ < sequence->steps.size()) {
        const auto& step = sequence->steps[step_index_];
        switch (step.kind) {
        case EventTimelineStepKind::Wait:
            if (step.seconds <= 0.0f) {
                ++step_index_;
                continue;
            }
            camera_ = {};
            waiting_ = true;
            wait_duration_ = step.seconds;
            wait_remaining_ = step.seconds;
            return;
        case EventTimelineStepKind::LookAt:
            begin_look_at(step);
            if (waiting_) return;
            break;
        case EventTimelineStepKind::LockControl:
            control_locked_ = true;
            ++step_index_;
            break;
        case EventTimelineStepKind::UnlockControl:
            control_locked_ = false;
            ++step_index_;
            break;
        case EventTimelineStepKind::Emit:
            emitted_.push_back(EventTimelineEmitEvent{sequence_id_, step.emit_name, step.payload_json});
            ++step_index_;
            break;
        case EventTimelineStepKind::StartDialogue: {
            if (!dialogue_starter_) {
                push_error(rt_error("EVENT-RT-DIALOGUE",
                    "start_dialogue step requires a dialogue starter on sequence '" + sequence_id_ + "'",
                    "Inject EventTimelineRuntime::set_dialogue_starter that starts DialogueRuntime."));
                active_ = false;
                complete_ = false;
                return;
            }
            const auto started = dialogue_starter_(step.dialogue_id);
            if (!started) {
                push_error(started.error());
                active_ = false;
                complete_ = false;
                return;
            }
            ++step_index_;
            break;
        }
        }
    }

    if (active_ && step_index_ >= sequence->steps.size()) {
        active_ = false;
        complete_ = true;
        waiting_ = false;
        wait_remaining_ = 0.0f;
        wait_duration_ = 0.0f;
        camera_ = {};
    }
}

} // namespace engine
