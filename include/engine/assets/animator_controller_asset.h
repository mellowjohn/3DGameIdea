#pragma once

#include "engine/core/result.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace engine {

enum class AnimatorParameterType : std::uint8_t { Float, Bool, Trigger };
enum class AnimatorConditionOp : std::uint8_t {
    Greater,
    GreaterOrEqual,
    Less,
    LessOrEqual,
    Equal,
    NotEqual,
    Trigger,
};
enum class AnimatorMotionType : std::uint8_t { Clip, BlendTree1D };
enum class AnimatorLayerBlendMode : std::uint8_t { Override };

struct AnimatorParameterDef {
    std::string name;
    AnimatorParameterType type = AnimatorParameterType::Float;
    float default_float = 0.0f;
    bool default_bool = false;
};

struct AnimatorClipRef {
    std::string clip_source; // project-relative glTF/GLB
    std::string clip;        // animation name inside source
    bool loop = true;
    float speed = 1.0f;
};

struct AnimatorBlendTreeChild {
    float threshold = 0.0f;
    AnimatorClipRef clip;
};

struct AnimatorBlendTree1D {
    std::string parameter;
    std::vector<AnimatorBlendTreeChild> children;
};

struct AnimatorMotion {
    AnimatorMotionType type = AnimatorMotionType::Clip;
    AnimatorClipRef clip;
    AnimatorBlendTree1D blend_tree;
};

struct AnimatorCondition {
    std::string parameter;
    AnimatorConditionOp op = AnimatorConditionOp::Greater;
    float value = 0.0f;
    bool bool_value = true;
};

struct AnimatorTransition {
    std::string from; // state name, or "*" for any
    std::string to;
    float duration = 0.15f;
    bool has_exit_time = false;
    float exit_time = 1.0f; // normalized [0,1] of source clip duration
    std::vector<AnimatorCondition> conditions;
};

struct AnimatorStateDef {
    std::string name;
    AnimatorMotion motion;
};

struct AnimatorLayerDef {
    std::string name;
    std::string default_state;
    AnimatorLayerBlendMode blend_mode = AnimatorLayerBlendMode::Override;
    std::vector<AnimatorStateDef> states;
    std::vector<AnimatorTransition> transitions;
};

/** Timeline marker on a state (DEC-0031 / TICKET-0105). Time is seconds into state playback. */
struct AnimatorTimelineEvent {
    std::string state;
    std::string layer; // empty → any layer that has the state
    float time = 0.0f;
    std::string name;
    std::string payload_json = "{}";
};

struct AnimatorControllerAsset {
    int schema_version = 1;
    std::string id;
    bool apply_root_motion = false;
    std::string root_joint; // empty → Root, then Hip
    bool root_motion_y = false;
    std::vector<AnimatorParameterDef> parameters;
    std::vector<AnimatorLayerDef> layers;
    std::vector<AnimatorTimelineEvent> timeline_events;

    [[nodiscard]] Result<void> validate() const;
    [[nodiscard]] const AnimatorLayerDef* find_layer(const std::string& name) const;
    [[nodiscard]] const AnimatorParameterDef* find_parameter(const std::string& name) const;
    [[nodiscard]] static Result<AnimatorControllerAsset> load(const std::filesystem::path& path);
    [[nodiscard]] static Result<AnimatorControllerAsset> parse(const std::string& text,
        const std::string& source_name = "controller.animator.json");
    [[nodiscard]] Result<void> save_atomic(const std::filesystem::path& path) const;
    [[nodiscard]] std::string to_json() const;
};

[[nodiscard]] const char* to_string(AnimatorParameterType value) noexcept;
[[nodiscard]] const char* to_string(AnimatorConditionOp value) noexcept;
[[nodiscard]] const char* to_string(AnimatorMotionType value) noexcept;
[[nodiscard]] const char* to_string(AnimatorLayerBlendMode value) noexcept;

} // namespace engine
