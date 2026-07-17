#include "engine/animation/animator_runtime.h"

#include "engine/diagnostics/logger.h"

#include <algorithm>
#include <cmath>

namespace engine {
namespace {

EngineError animator_error(std::string code, std::string message, std::string entity_id = {},
    std::string remedy = "Fix the animator controller, clip references, or Lua drive call.") {
    EngineError error{std::move(code), Severity::Error, ErrorCategory::Validation, "animator", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
    if (!entity_id.empty()) error.causes.push_back("entityId=" + entity_id);
    return error;
}

const AnimatorStateDef* find_state(const AnimatorLayerDef& layer, const std::string& name) {
    for (const auto& state : layer.states) {
        if (state.name == name) return &state;
    }
    return nullptr;
}

float wrap_time(float time, float duration, bool loop) {
    if (!(duration > 0.0f)) return 0.0f;
    if (loop) {
        const float wrapped = std::fmod(time, duration);
        return wrapped < 0.0f ? wrapped + duration : wrapped;
    }
    return std::clamp(time, 0.0f, duration);
}

} // namespace

void AnimatorRuntime::set_project_root(std::filesystem::path project_root) {
    project_root_ = std::move(project_root);
}

void AnimatorRuntime::set_clip_library(AnimationClipLibrary* library) noexcept {
    clip_library_ = library;
}

void AnimatorRuntime::push_error(EngineError error) {
    Logger::instance().write(error);
    recent_errors_.push_back(std::move(error));
    if (recent_errors_.size() > 32) recent_errors_.erase(recent_errors_.begin());
}

AnimatorRuntime::Instance* AnimatorRuntime::find_instance(const std::string& entity_id) {
    const auto it = instances_.find(entity_id);
    return it == instances_.end() ? nullptr : &it->second;
}

const AnimatorRuntime::Instance* AnimatorRuntime::find_instance(const std::string& entity_id) const {
    const auto it = instances_.find(entity_id);
    return it == instances_.end() ? nullptr : &it->second;
}

Result<void> AnimatorRuntime::attach(const std::string& entity_id, const std::string& controller_path,
    const std::string& default_state_override) {
    if (entity_id.empty())
        return Result<void>::failure(animator_error("ANIM-ATTACH-ENTITY", "entity id is required"));
    if (controller_path.empty())
        return Result<void>::failure(
            animator_error("ANIM-ATTACH-CONTROLLER", "controller path is required", entity_id));

    const auto absolute = project_root_.empty() ? std::filesystem::path(controller_path)
                                                : project_root_ / controller_path;
    const auto loaded = AnimatorControllerAsset::load(absolute);
    if (!loaded) {
        auto error = loaded.error();
        error.causes.push_back("entityId=" + entity_id);
        error.causes.push_back("controller=" + controller_path);
        push_error(error);
        return Result<void>::failure(std::move(error));
    }

    Instance instance;
    instance.entity_id = entity_id;
    instance.controller_path = controller_path;
    instance.controller = loaded.value();
    instance.default_state_override = default_state_override;
    instance.bound = true;

    for (const auto& param : instance.controller.parameters) {
        ParamValue value;
        value.type = param.type;
        value.float_value = param.default_float;
        value.bool_value = param.default_bool;
        value.trigger_armed = false;
        instance.params[param.name] = value;
    }

    for (const auto& layer_def : instance.controller.layers) {
        LayerRuntime layer;
        layer.name = layer_def.name;
        layer.current_state = default_state_override.empty() ? layer_def.default_state : default_state_override;
        if (!find_state(layer_def, layer.current_state)) {
            auto error = animator_error("ANIM-ATTACH-STATE",
                "default state '" + layer.current_state + "' missing on layer '" + layer.name + "'", entity_id);
            push_error(error);
            return Result<void>::failure(std::move(error));
        }
        layer.events_fired_mask.assign(instance.controller.timeline_events.size(), 0);
        instance.layers.push_back(std::move(layer));
    }

    instances_[entity_id] = std::move(instance);
    return Result<void>::success();
}

void AnimatorRuntime::detach(const std::string& entity_id) {
    instances_.erase(entity_id);
}

void AnimatorRuntime::reset() noexcept {
    instances_.clear();
    fired_events_.clear();
    recent_errors_.clear();
}

Result<void> AnimatorRuntime::resolve_clip(const AnimatorClipRef& ref, const std::string& entity_id,
    const AnimationClip** out_clip) {
    if (!clip_library_)
        return Result<void>::failure(
            animator_error("ANIM-CLIP-LIBRARY", "Animation clip library is not bound", entity_id));
    const auto absolute = project_root_.empty() ? std::filesystem::path(ref.clip_source)
                                                : project_root_ / ref.clip_source;
    auto loaded = clip_library_->get(absolute);
    if (!loaded) loaded = clip_library_->load(absolute);
    if (!loaded) {
        auto error = loaded.error();
        error.causes.push_back("entityId=" + entity_id);
        error.causes.push_back("clipSource=" + ref.clip_source);
        return Result<void>::failure(std::move(error));
    }
    for (const auto& clip : loaded.value()->clips) {
        if (clip.name == ref.clip) {
            *out_clip = &clip;
            return Result<void>::success();
        }
    }
    return Result<void>::failure(animator_error("ANIM-CLIP-MISSING",
        "Clip '" + ref.clip + "' not found in " + ref.clip_source, entity_id,
        "Export the named glTF animation or fix the controller clip reference."));
}

bool AnimatorRuntime::evaluate_condition(const AnimatorCondition& condition, const ParamValue& value) {
    switch (condition.op) {
    case AnimatorConditionOp::Trigger:
        return value.type == AnimatorParameterType::Trigger && value.trigger_armed;
    case AnimatorConditionOp::Greater:
        return value.float_value > condition.value;
    case AnimatorConditionOp::GreaterOrEqual:
        return value.float_value >= condition.value;
    case AnimatorConditionOp::Less:
        return value.float_value < condition.value;
    case AnimatorConditionOp::LessOrEqual:
        return value.float_value <= condition.value;
    case AnimatorConditionOp::Equal:
        if (value.type == AnimatorParameterType::Bool) return value.bool_value == condition.bool_value;
        return std::fabs(value.float_value - condition.value) <= 1e-5f;
    case AnimatorConditionOp::NotEqual:
        if (value.type == AnimatorParameterType::Bool) return value.bool_value != condition.bool_value;
        return std::fabs(value.float_value - condition.value) > 1e-5f;
    }
    return false;
}

bool AnimatorRuntime::conditions_met(const Instance& instance, const AnimatorTransition& transition) const {
    for (const auto& condition : transition.conditions) {
        const auto it = instance.params.find(condition.parameter);
        if (it == instance.params.end()) return false;
        if (!evaluate_condition(condition, it->second)) return false;
    }
    return true;
}

void AnimatorRuntime::consume_triggers(Instance& instance, const AnimatorTransition& transition) {
    for (const auto& condition : transition.conditions) {
        if (condition.op != AnimatorConditionOp::Trigger) continue;
        auto it = instance.params.find(condition.parameter);
        if (it != instance.params.end()) it->second.trigger_armed = false;
    }
}

void AnimatorRuntime::evaluate_transitions(Instance& instance, LayerRuntime& layer,
    const AnimatorLayerDef& layer_def) {
    if (layer.in_transition) return;
    const auto* current = find_state(layer_def, layer.current_state);
    if (!current) return;

    float source_duration = 0.0f;
    bool source_loop = true;
    if (current->motion.type == AnimatorMotionType::Clip) {
        const AnimationClip* clip = nullptr;
        if (resolve_clip(current->motion.clip, instance.entity_id, &clip)) {
            source_duration = clip->duration_seconds;
            source_loop = current->motion.clip.loop;
        }
    }

    for (const auto& transition : layer_def.transitions) {
        if (transition.from != "*" && transition.from != layer.current_state) continue;
        if (transition.to == layer.current_state) continue;
        if (!find_state(layer_def, transition.to)) {
            push_error(animator_error("ANIM-TRANSITION-TARGET",
                "transition target missing: " + transition.to, instance.entity_id));
            continue;
        }
        if (transition.has_exit_time) {
            if (!(source_duration > 0.0f)) continue;
            const float normalized = source_loop ? (wrap_time(layer.state_time, source_duration, true) / source_duration)
                                                 : std::clamp(layer.state_time / source_duration, 0.0f, 1.0f);
            if (normalized + 1e-4f < transition.exit_time) continue;
        }
        if (!conditions_met(instance, transition)) continue;

        layer.in_transition = true;
        layer.next_state = transition.to;
        layer.transition_duration = std::max(0.0f, transition.duration);
        layer.transition_elapsed = 0.0f;
        consume_triggers(instance, transition);
        if (layer.transition_duration <= 0.0f) {
            layer.current_state = layer.next_state;
            layer.state_time = 0.0f;
            layer.in_transition = false;
            layer.next_state.clear();
        }
        return;
    }
}

std::vector<AnimatorClipWeight> AnimatorRuntime::sample_motion(Instance& instance, const AnimatorMotion& motion,
    float state_time, float weight_scale) {
    std::vector<AnimatorClipWeight> weights;
    if (!(weight_scale > 0.0f)) return weights;

    if (motion.type == AnimatorMotionType::Clip) {
        const AnimationClip* clip = nullptr;
        if (const auto resolved = resolve_clip(motion.clip, instance.entity_id, &clip); !resolved) {
            push_error(resolved.error());
            return weights;
        }
        AnimatorClipWeight weight;
        weight.clip_source = motion.clip.clip_source;
        weight.clip = motion.clip.clip;
        weight.weight = weight_scale;
        weight.loop = motion.clip.loop;
        weight.time_seconds = wrap_time(state_time * motion.clip.speed, clip->duration_seconds, motion.clip.loop);
        weights.push_back(std::move(weight));
        return weights;
    }

    const auto param_it = instance.params.find(motion.blend_tree.parameter);
    if (param_it == instance.params.end() || motion.blend_tree.children.empty()) {
        push_error(animator_error("ANIM-BLEND-PARAM",
            "blendTree1D parameter unavailable: " + motion.blend_tree.parameter, instance.entity_id));
        return weights;
    }
    const float param = param_it->second.float_value;
    const auto& children = motion.blend_tree.children;
    if (param <= children.front().threshold) {
        const AnimationClip* clip = nullptr;
        if (const auto resolved = resolve_clip(children.front().clip, instance.entity_id, &clip); !resolved) {
            push_error(resolved.error());
            return weights;
        }
        AnimatorClipWeight weight;
        weight.clip_source = children.front().clip.clip_source;
        weight.clip = children.front().clip.clip;
        weight.weight = weight_scale;
        weight.loop = children.front().clip.loop;
        weight.time_seconds =
            wrap_time(state_time * children.front().clip.speed, clip->duration_seconds, children.front().clip.loop);
        weights.push_back(std::move(weight));
        return weights;
    }
    if (param >= children.back().threshold) {
        const AnimationClip* clip = nullptr;
        if (const auto resolved = resolve_clip(children.back().clip, instance.entity_id, &clip); !resolved) {
            push_error(resolved.error());
            return weights;
        }
        AnimatorClipWeight weight;
        weight.clip_source = children.back().clip.clip_source;
        weight.clip = children.back().clip.clip;
        weight.weight = weight_scale;
        weight.loop = children.back().clip.loop;
        weight.time_seconds =
            wrap_time(state_time * children.back().clip.speed, clip->duration_seconds, children.back().clip.loop);
        weights.push_back(std::move(weight));
        return weights;
    }

    for (std::size_t i = 0; i + 1 < children.size(); ++i) {
        const auto& left = children[i];
        const auto& right = children[i + 1];
        if (param < left.threshold || param > right.threshold) continue;
        const float span = right.threshold - left.threshold;
        const float t = span > 1e-6f ? (param - left.threshold) / span : 0.0f;
        const AnimationClip* left_clip = nullptr;
        const AnimationClip* right_clip = nullptr;
        if (const auto resolved = resolve_clip(left.clip, instance.entity_id, &left_clip); !resolved) {
            push_error(resolved.error());
            return weights;
        }
        if (const auto resolved = resolve_clip(right.clip, instance.entity_id, &right_clip); !resolved) {
            push_error(resolved.error());
            return weights;
        }
        AnimatorClipWeight a;
        a.clip_source = left.clip.clip_source;
        a.clip = left.clip.clip;
        a.weight = weight_scale * (1.0f - t);
        a.loop = left.clip.loop;
        a.time_seconds = wrap_time(state_time * left.clip.speed, left_clip->duration_seconds, left.clip.loop);
        AnimatorClipWeight b;
        b.clip_source = right.clip.clip_source;
        b.clip = right.clip.clip;
        b.weight = weight_scale * t;
        b.loop = right.clip.loop;
        b.time_seconds = wrap_time(state_time * right.clip.speed, right_clip->duration_seconds, right.clip.loop);
        if (a.weight > 0.0f) weights.push_back(std::move(a));
        if (b.weight > 0.0f) weights.push_back(std::move(b));
        return weights;
    }
    return weights;
}

void AnimatorRuntime::advance_layer(Instance& instance, LayerRuntime& layer, const AnimatorLayerDef& layer_def,
    float dt) {
    const std::string previous_state = layer.current_state;
    evaluate_transitions(instance, layer, layer_def);
    if (layer.in_transition) {
        layer.transition_elapsed += dt;
        layer.state_time += dt;
        if (layer.transition_elapsed >= layer.transition_duration) {
            layer.current_state = layer.next_state;
            layer.state_time = 0.0f;
            layer.in_transition = false;
            layer.next_state.clear();
            layer.transition_elapsed = 0.0f;
            layer.transition_duration = 0.0f;
        }
    } else {
        layer.state_time += dt;
    }
    if (layer.current_state != previous_state)
        std::fill(layer.events_fired_mask.begin(), layer.events_fired_mask.end(), std::uint8_t{0});
}

float AnimatorRuntime::motion_duration(Instance& instance, const AnimatorMotion& motion) {
    if (motion.type == AnimatorMotionType::Clip) {
        const AnimationClip* clip = nullptr;
        if (!resolve_clip(motion.clip, instance.entity_id, &clip) || !clip) return 0.0f;
        const float speed = motion.clip.speed > 0.0f ? motion.clip.speed : 1.0f;
        return clip->duration_seconds / speed;
    }
    float max_duration = 0.0f;
    for (const auto& child : motion.blend_tree.children) {
        const AnimationClip* clip = nullptr;
        if (!resolve_clip(child.clip, instance.entity_id, &clip) || !clip) continue;
        const float speed = child.clip.speed > 0.0f ? child.clip.speed : 1.0f;
        max_duration = std::max(max_duration, clip->duration_seconds / speed);
    }
    return max_duration;
}

bool AnimatorRuntime::motion_loops(const AnimatorMotion& motion) const {
    if (motion.type == AnimatorMotionType::Clip) return motion.clip.loop;
    for (const auto& child : motion.blend_tree.children) {
        if (child.clip.loop) return true;
    }
    return false;
}

void AnimatorRuntime::evaluate_timeline_events(Instance& instance, float dt) {
    if (!(dt > 0.0f) || instance.controller.timeline_events.empty()) return;
    for (std::size_t layer_index = 0;
         layer_index < instance.layers.size() && layer_index < instance.controller.layers.size(); ++layer_index) {
        auto& layer = instance.layers[layer_index];
        const auto& layer_def = instance.controller.layers[layer_index];
        if (layer.in_transition) continue;
        const auto* state = find_state(layer_def, layer.current_state);
        if (!state) continue;
        if (layer.events_fired_mask.size() != instance.controller.timeline_events.size())
            layer.events_fired_mask.assign(instance.controller.timeline_events.size(), 0);

        const float duration = motion_duration(instance, state->motion);
        const bool loops = motion_loops(state->motion);
        const float prev_time = std::max(0.0f, layer.state_time - dt);
        float from = prev_time;
        float to = layer.state_time;
        bool wrapped = false;
        if (duration > 0.0f) {
            if (loops) {
                from = wrap_time(prev_time, duration, true);
                to = wrap_time(layer.state_time, duration, true);
                wrapped = layer.state_time > prev_time && to + 1e-6f < from;
                if (wrapped) std::fill(layer.events_fired_mask.begin(), layer.events_fired_mask.end(), std::uint8_t{0});
            } else {
                from = std::min(prev_time, duration);
                to = std::min(layer.state_time, duration);
            }
        }

        for (std::size_t event_index = 0; event_index < instance.controller.timeline_events.size(); ++event_index) {
            const auto& event = instance.controller.timeline_events[event_index];
            if (event.state != layer.current_state) continue;
            if (!event.layer.empty() && event.layer != layer.name) continue;
            if (event_index < layer.events_fired_mask.size() && layer.events_fired_mask[event_index]) continue;

            // Half-open (from, to], plus t==0 on first advance / wrap restart.
            bool crossed = false;
            if (wrapped) {
                crossed = (event.time > from - 1e-6f) || (event.time <= to + 1e-6f);
            } else if (event.time <= 1e-6f) {
                crossed = prev_time <= 1e-6f && layer.state_time > 1e-6f;
            } else {
                crossed = event.time > from + 1e-6f && event.time <= to + 1e-6f;
            }
            if (!crossed) continue;

            if (event_index < layer.events_fired_mask.size()) layer.events_fired_mask[event_index] = 1;
            AnimatorFiredEvent fired;
            fired.entity_id = instance.entity_id;
            fired.name = event.name;
            fired.state = event.state;
            fired.layer = layer.name;
            fired.time = event.time;
            fired.payload_json = event.payload_json.empty() ? "{}" : event.payload_json;
            fired_events_.push_back(std::move(fired));
        }
    }
}

void AnimatorRuntime::accumulate_root_motion(Instance& instance, float dt) {
    instance.last_root_delta = {};
    instance.last_root_delta.include_y = instance.controller.root_motion_y;
    instance.last_root_delta.applied = instance.controller.apply_root_motion;
    if (!instance.controller.apply_root_motion || !(dt > 0.0f)) return;

    const std::string& root_joint = instance.controller.root_joint;
    std::array<float, 3> sum{0.0f, 0.0f, 0.0f};

    auto add_motion = [&](const AnimatorMotion& motion, float state_time, float weight) {
        if (!(weight > 0.0f)) return;
        if (motion.type == AnimatorMotionType::Clip) {
            const AnimationClip* clip = nullptr;
            if (const auto resolved = resolve_clip(motion.clip, instance.entity_id, &clip); !resolved) {
                push_error(resolved.error());
                return;
            }
            const float from = state_time * motion.clip.speed;
            const float to = (state_time + dt) * motion.clip.speed;
            const auto extracted =
                extract_clip_root_motion_delta(*clip, root_joint, from, to, motion.clip.loop);
            if (!extracted) {
                push_error(extracted.error());
                return;
            }
            if (!extracted.value().found_root_channel) return;
            sum[0] += extracted.value().translation[0] * weight;
            sum[1] += extracted.value().translation[1] * weight;
            sum[2] += extracted.value().translation[2] * weight;
            return;
        }
        // blendTree1D: sample children with same weight split as sample_motion
        const auto clips = sample_motion(instance, motion, state_time, weight);
        for (const auto& entry : clips) {
            AnimatorClipRef ref;
            ref.clip_source = entry.clip_source;
            ref.clip = entry.clip;
            ref.loop = entry.loop;
            ref.speed = 1.0f;
            const AnimationClip* clip = nullptr;
            if (const auto resolved = resolve_clip(ref, instance.entity_id, &clip); !resolved) {
                push_error(resolved.error());
                continue;
            }
            // entry.time_seconds is already wrapped absolute time; advance by dt (speed baked into tree children)
            float child_speed = 1.0f;
            for (const auto& child : motion.blend_tree.children) {
                if (child.clip.clip == entry.clip && child.clip.clip_source == entry.clip_source) {
                    child_speed = child.clip.speed;
                    break;
                }
            }
            const float from = entry.time_seconds;
            const float to = entry.time_seconds + dt * child_speed;
            const auto extracted =
                extract_clip_root_motion_delta(*clip, root_joint, from, to, entry.loop);
            if (!extracted || !extracted.value().found_root_channel) continue;
            sum[0] += extracted.value().translation[0] * entry.weight;
            sum[1] += extracted.value().translation[1] * entry.weight;
            sum[2] += extracted.value().translation[2] * entry.weight;
        }
    };

    for (std::size_t i = 0; i < instance.layers.size() && i < instance.controller.layers.size(); ++i) {
        const auto& layer = instance.layers[i];
        const auto& layer_def = instance.controller.layers[i];
        const auto* current = find_state(layer_def, layer.current_state);
        if (!current) continue;
        if (!layer.in_transition) {
            add_motion(current->motion, layer.state_time, 1.0f);
            continue;
        }
        const auto* next = find_state(layer_def, layer.next_state);
        const float alpha =
            layer.transition_duration > 0.0f
                ? std::clamp(layer.transition_elapsed / layer.transition_duration, 0.0f, 1.0f)
                : 1.0f;
        add_motion(current->motion, layer.state_time, 1.0f - alpha);
        if (next) add_motion(next->motion, 0.0f, alpha);
    }

    instance.last_root_delta.translation = sum;
}

AnimatorInstanceStatus AnimatorRuntime::build_status(const Instance& instance) const {
    AnimatorInstanceStatus status;
    status.entity_id = instance.entity_id;
    status.controller_path = instance.controller_path;
    status.bound = instance.bound;
    status.apply_root_motion = instance.controller.apply_root_motion;
    for (const auto& layer : instance.layers) {
        AnimatorLayerStatus layer_status;
        layer_status.layer = layer.name;
        layer_status.state = layer.current_state;
        layer_status.state_time_seconds = layer.state_time;
        layer_status.in_transition = layer.in_transition;
        layer_status.next_state = layer.next_state;
        layer_status.transition_alpha =
            layer.in_transition && layer.transition_duration > 0.0f
                ? std::clamp(layer.transition_elapsed / layer.transition_duration, 0.0f, 1.0f)
                : 0.0f;
        status.layers.push_back(std::move(layer_status));
    }
    // Rebuild active clips without mutating (const_cast path avoided: sample via non-const helper copy).
    return status;
}

void AnimatorRuntime::tick(float dt_seconds) {
    if (!(dt_seconds >= 0.0f)) return;
    for (auto& [entity_id, instance] : instances_) {
        (void)entity_id;
        if (!instance.bound) continue;
        accumulate_root_motion(instance, dt_seconds);
        for (std::size_t i = 0; i < instance.layers.size() && i < instance.controller.layers.size(); ++i)
            advance_layer(instance, instance.layers[i], instance.controller.layers[i], dt_seconds);
        evaluate_timeline_events(instance, dt_seconds);
    }
}

std::vector<AnimatorFiredEvent> AnimatorRuntime::take_fired_events() {
    std::vector<AnimatorFiredEvent> out;
    out.swap(fired_events_);
    return out;
}

Result<bool> AnimatorRuntime::apply_root_motion(const std::string& entity_id) const {
    const auto* instance = find_instance(entity_id);
    if (!instance)
        return Result<bool>::failure(animator_error("ANIM-INSTANCE-MISSING", "No animator on entity", entity_id));
    return Result<bool>::success(instance->controller.apply_root_motion);
}

Result<AnimatorRootMotionDelta> AnimatorRuntime::root_motion_delta(const std::string& entity_id) const {
    const auto* instance = find_instance(entity_id);
    if (!instance)
        return Result<AnimatorRootMotionDelta>::failure(
            animator_error("ANIM-INSTANCE-MISSING", "No animator on entity", entity_id));
    return Result<AnimatorRootMotionDelta>::success(instance->last_root_delta);
}

Result<void> AnimatorRuntime::set_float(const std::string& entity_id, const std::string& name, float value) {
    auto* instance = find_instance(entity_id);
    if (!instance)
        return Result<void>::failure(animator_error("ANIM-INSTANCE-MISSING", "No animator on entity", entity_id));
    auto it = instance->params.find(name);
    if (it == instance->params.end() || it->second.type != AnimatorParameterType::Float)
        return Result<void>::failure(
            animator_error("ANIM-PARAM-FLOAT", "Float parameter not found: " + name, entity_id));
    it->second.float_value = value;
    return Result<void>::success();
}

Result<void> AnimatorRuntime::set_bool(const std::string& entity_id, const std::string& name, bool value) {
    auto* instance = find_instance(entity_id);
    if (!instance)
        return Result<void>::failure(animator_error("ANIM-INSTANCE-MISSING", "No animator on entity", entity_id));
    auto it = instance->params.find(name);
    if (it == instance->params.end() || it->second.type != AnimatorParameterType::Bool)
        return Result<void>::failure(
            animator_error("ANIM-PARAM-BOOL", "Bool parameter not found: " + name, entity_id));
    it->second.bool_value = value;
    return Result<void>::success();
}

Result<void> AnimatorRuntime::set_trigger(const std::string& entity_id, const std::string& name) {
    auto* instance = find_instance(entity_id);
    if (!instance)
        return Result<void>::failure(animator_error("ANIM-INSTANCE-MISSING", "No animator on entity", entity_id));
    auto it = instance->params.find(name);
    if (it == instance->params.end() || it->second.type != AnimatorParameterType::Trigger)
        return Result<void>::failure(
            animator_error("ANIM-PARAM-TRIGGER", "Trigger parameter not found: " + name, entity_id));
    it->second.trigger_armed = true;
    return Result<void>::success();
}

Result<void> AnimatorRuntime::crossfade(const std::string& entity_id, const std::string& state_name,
    float duration_seconds, const std::string& layer_name) {
    auto* instance = find_instance(entity_id);
    if (!instance)
        return Result<void>::failure(animator_error("ANIM-INSTANCE-MISSING", "No animator on entity", entity_id));
    if (state_name.empty())
        return Result<void>::failure(animator_error("ANIM-CROSSFADE-STATE", "state name is required", entity_id));

    LayerRuntime* layer = nullptr;
    const AnimatorLayerDef* layer_def = nullptr;
    if (layer_name.empty()) {
        if (instance->layers.empty() || instance->controller.layers.empty())
            return Result<void>::failure(animator_error("ANIM-CROSSFADE-LAYER", "controller has no layers", entity_id));
        layer = &instance->layers.front();
        layer_def = &instance->controller.layers.front();
    } else {
        for (std::size_t i = 0; i < instance->layers.size(); ++i) {
            if (instance->layers[i].name == layer_name) {
                layer = &instance->layers[i];
                layer_def = &instance->controller.layers[i];
                break;
            }
        }
    }
    if (!layer || !layer_def)
        return Result<void>::failure(
            animator_error("ANIM-CROSSFADE-LAYER", "layer not found: " + layer_name, entity_id));
    if (!find_state(*layer_def, state_name))
        return Result<void>::failure(
            animator_error("ANIM-CROSSFADE-TARGET", "state not found: " + state_name, entity_id));

    if (duration_seconds <= 0.0f) {
        // Restarting playback (even to the same state) must re-arm timeline events.
        std::fill(layer->events_fired_mask.begin(), layer->events_fired_mask.end(), std::uint8_t{0});
        layer->current_state = state_name;
        layer->state_time = 0.0f;
        layer->in_transition = false;
        layer->next_state.clear();
        return Result<void>::success();
    }
    layer->in_transition = true;
    layer->next_state = state_name;
    layer->transition_duration = duration_seconds;
    layer->transition_elapsed = 0.0f;
    return Result<void>::success();
}

Result<std::string> AnimatorRuntime::current_state(const std::string& entity_id,
    const std::string& layer_name) const {
    const auto* instance = find_instance(entity_id);
    if (!instance)
        return Result<std::string>::failure(
            animator_error("ANIM-INSTANCE-MISSING", "No animator on entity", entity_id));
    if (instance->layers.empty())
        return Result<std::string>::failure(animator_error("ANIM-LAYER-EMPTY", "No layers", entity_id));
    if (layer_name.empty()) return Result<std::string>::success(instance->layers.front().current_state);
    for (const auto& layer : instance->layers) {
        if (layer.name == layer_name) return Result<std::string>::success(layer.current_state);
    }
    return Result<std::string>::failure(
        animator_error("ANIM-LAYER-MISSING", "layer not found: " + layer_name, entity_id));
}

Result<AnimatorInstanceStatus> AnimatorRuntime::status(const std::string& entity_id) const {
    const auto* instance = find_instance(entity_id);
    if (!instance)
        return Result<AnimatorInstanceStatus>::failure(
            animator_error("ANIM-INSTANCE-MISSING", "No animator on entity", entity_id));
    auto status = build_status(*instance);
    // Sample active clips via mutable temporary path.
    AnimatorRuntime* self = const_cast<AnimatorRuntime*>(this);
    for (std::size_t i = 0; i < instance->layers.size() && i < instance->controller.layers.size(); ++i) {
        const auto& layer = instance->layers[i];
        const auto& layer_def = instance->controller.layers[i];
        const auto* current = find_state(layer_def, layer.current_state);
        if (!current) continue;
        if (!layer.in_transition) {
            auto clips = self->sample_motion(*const_cast<Instance*>(instance), current->motion, layer.state_time, 1.0f);
            status.active_clips.insert(status.active_clips.end(), clips.begin(), clips.end());
            continue;
        }
        const auto* next = find_state(layer_def, layer.next_state);
        const float alpha =
            layer.transition_duration > 0.0f ? std::clamp(layer.transition_elapsed / layer.transition_duration, 0.0f, 1.0f)
                                             : 1.0f;
        auto from_clips =
            self->sample_motion(*const_cast<Instance*>(instance), current->motion, layer.state_time, 1.0f - alpha);
        status.active_clips.insert(status.active_clips.end(), from_clips.begin(), from_clips.end());
        if (next) {
            auto to_clips = self->sample_motion(*const_cast<Instance*>(instance), next->motion, 0.0f, alpha);
            status.active_clips.insert(status.active_clips.end(), to_clips.begin(), to_clips.end());
        }
    }
    return Result<AnimatorInstanceStatus>::success(std::move(status));
}

} // namespace engine
