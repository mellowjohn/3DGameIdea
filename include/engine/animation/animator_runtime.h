#pragma once

#include "engine/assets/animation_clip_asset.h"
#include "engine/assets/animator_controller_asset.h"
#include "engine/core/result.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace engine {

struct AnimatorClipWeight {
    std::string clip_source;
    std::string clip;
    float weight = 0.0f;
    float time_seconds = 0.0f;
    bool loop = true;
};

struct AnimatorLayerStatus {
    std::string layer;
    std::string state;
    float state_time_seconds = 0.0f;
    bool in_transition = false;
    std::string next_state;
    float transition_alpha = 0.0f;
};

struct AnimatorInstanceStatus {
    std::string entity_id;
    std::string controller_path;
    bool bound = false;
    bool apply_root_motion = false;
    std::vector<AnimatorLayerStatus> layers;
    std::vector<AnimatorClipWeight> active_clips;
};

struct AnimatorRootMotionDelta {
    std::array<float, 3> translation{0.0f, 0.0f, 0.0f}; // clip-space meters for last tick
    bool include_y = false;
    bool applied = false;
};

struct AnimatorFiredEvent {
    std::string entity_id;
    std::string name;
    std::string state;
    std::string layer;
    float time = 0.0f;
    std::string payload_json = "{}";
};

/// C++ animator backend (TICKET-0103 / DEC-0022). Evaluates controller graphs; Lua only drives params/states.
class AnimatorRuntime {
public:
    void set_project_root(std::filesystem::path project_root);
    void set_clip_library(AnimationClipLibrary* library) noexcept;

    [[nodiscard]] Result<void> attach(const std::string& entity_id, const std::string& controller_path,
        const std::string& default_state_override = {});
    void detach(const std::string& entity_id);
    void reset() noexcept;

    [[nodiscard]] Result<void> set_float(const std::string& entity_id, const std::string& name, float value);
    [[nodiscard]] Result<void> set_bool(const std::string& entity_id, const std::string& name, bool value);
    [[nodiscard]] Result<void> set_trigger(const std::string& entity_id, const std::string& name);
    [[nodiscard]] Result<void> crossfade(const std::string& entity_id, const std::string& state_name,
        float duration_seconds = 0.15f, const std::string& layer_name = {});
    [[nodiscard]] Result<std::string> current_state(const std::string& entity_id,
        const std::string& layer_name = {}) const;
    [[nodiscard]] Result<AnimatorInstanceStatus> status(const std::string& entity_id) const;
    [[nodiscard]] Result<bool> apply_root_motion(const std::string& entity_id) const;
    [[nodiscard]] Result<AnimatorRootMotionDelta> root_motion_delta(const std::string& entity_id) const;

    void tick(float dt_seconds);
    /** Drain timeline events fired during the last `tick` (DEC-0031). */
    [[nodiscard]] std::vector<AnimatorFiredEvent> take_fired_events();
    [[nodiscard]] const std::vector<EngineError>& recent_errors() const noexcept { return recent_errors_; }
    void clear_recent_errors() { recent_errors_.clear(); }

private:
    struct ParamValue {
        AnimatorParameterType type = AnimatorParameterType::Float;
        float float_value = 0.0f;
        bool bool_value = false;
        bool trigger_armed = false;
    };

    struct LayerRuntime {
        std::string name;
        std::string current_state;
        float state_time = 0.0f;
        bool in_transition = false;
        std::string next_state;
        float transition_duration = 0.0f;
        float transition_elapsed = 0.0f;
        std::vector<std::uint8_t> events_fired_mask; // parallel to controller.timeline_events
    };

    struct Instance {
        std::string entity_id;
        std::string controller_path;
        AnimatorControllerAsset controller;
        std::string default_state_override;
        std::map<std::string, ParamValue> params;
        std::vector<LayerRuntime> layers;
        bool bound = false;
        AnimatorRootMotionDelta last_root_delta{};
    };

    [[nodiscard]] Instance* find_instance(const std::string& entity_id);
    [[nodiscard]] const Instance* find_instance(const std::string& entity_id) const;
    [[nodiscard]] Result<void> resolve_clip(const AnimatorClipRef& ref, const std::string& entity_id,
        const AnimationClip** out_clip);
    [[nodiscard]] static bool evaluate_condition(const AnimatorCondition& condition, const ParamValue& value);
    [[nodiscard]] bool conditions_met(const Instance& instance, const AnimatorTransition& transition) const;
    void consume_triggers(Instance& instance, const AnimatorTransition& transition);
    void evaluate_transitions(Instance& instance, LayerRuntime& layer, const AnimatorLayerDef& layer_def);
    void advance_layer(Instance& instance, LayerRuntime& layer, const AnimatorLayerDef& layer_def, float dt);
    [[nodiscard]] std::vector<AnimatorClipWeight> sample_motion(Instance& instance, const AnimatorMotion& motion,
        float state_time, float weight_scale);
    void accumulate_root_motion(Instance& instance, float dt);
    void evaluate_timeline_events(Instance& instance, float dt);
    [[nodiscard]] float motion_duration(Instance& instance, const AnimatorMotion& motion);
    [[nodiscard]] bool motion_loops(const AnimatorMotion& motion) const;
    [[nodiscard]] AnimatorInstanceStatus build_status(const Instance& instance) const;
    void push_error(EngineError error);

    std::filesystem::path project_root_;
    AnimationClipLibrary* clip_library_ = nullptr;
    std::map<std::string, Instance> instances_;
    std::vector<AnimatorFiredEvent> fired_events_;
    std::vector<EngineError> recent_errors_;
};

} // namespace engine
