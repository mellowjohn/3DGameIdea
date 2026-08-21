#include "engine/assets/animator_controller_asset.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <unordered_set>

namespace engine {
namespace {

EngineError controller_error(std::string code, std::string message,
    std::string remedy = "Fix the animator controller JSON and reload.") {
    return {std::move(code), Severity::Error, ErrorCategory::Validation, "animator-controller",
        std::move(message), ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

std::string normalize_key(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

Result<AnimatorParameterType> parse_parameter_type(const std::string& value) {
    const auto key = normalize_key(value);
    if (key == "float") return Result<AnimatorParameterType>::success(AnimatorParameterType::Float);
    if (key == "bool" || key == "boolean") return Result<AnimatorParameterType>::success(AnimatorParameterType::Bool);
    if (key == "trigger") return Result<AnimatorParameterType>::success(AnimatorParameterType::Trigger);
    return Result<AnimatorParameterType>::failure(
        controller_error("ANIM-CTRL-PARAM-TYPE", "Unsupported parameter type: " + value));
}

Result<AnimatorConditionOp> parse_condition_op(const std::string& value) {
    const auto key = normalize_key(value);
    if (key == "greater" || key == ">") return Result<AnimatorConditionOp>::success(AnimatorConditionOp::Greater);
    if (key == "greaterorequal" || key == ">=")
        return Result<AnimatorConditionOp>::success(AnimatorConditionOp::GreaterOrEqual);
    if (key == "less" || key == "<") return Result<AnimatorConditionOp>::success(AnimatorConditionOp::Less);
    if (key == "lessorequal" || key == "<=")
        return Result<AnimatorConditionOp>::success(AnimatorConditionOp::LessOrEqual);
    if (key == "equal" || key == "==" || key == "=")
        return Result<AnimatorConditionOp>::success(AnimatorConditionOp::Equal);
    if (key == "notequal" || key == "!=") return Result<AnimatorConditionOp>::success(AnimatorConditionOp::NotEqual);
    if (key == "trigger") return Result<AnimatorConditionOp>::success(AnimatorConditionOp::Trigger);
    return Result<AnimatorConditionOp>::failure(
        controller_error("ANIM-CTRL-CONDITION-OP", "Unsupported condition op: " + value));
}

Result<AnimatorClipRef> parse_clip_ref(const nlohmann::json& value, const char* label) {
    if (!value.is_object())
        return Result<AnimatorClipRef>::failure(
            controller_error("ANIM-CTRL-CLIP-INVALID", std::string(label) + " must be an object"));
    AnimatorClipRef clip;
    clip.clip_source = value.value("clipSource", value.value("clip_source", std::string{}));
    clip.clip = value.value("clip", value.value("clipName", std::string{}));
    clip.loop = value.value("loop", true);
    clip.speed = value.value("speed", 1.0f);
    if (clip.clip_source.empty() || clip.clip.empty())
        return Result<AnimatorClipRef>::failure(controller_error("ANIM-CTRL-CLIP-INVALID",
            std::string(label) + " requires clipSource and clip"));
    if (!(clip.speed > 0.0f))
        return Result<AnimatorClipRef>::failure(
            controller_error("ANIM-CTRL-CLIP-SPEED", std::string(label) + " speed must be > 0"));
    return Result<AnimatorClipRef>::success(std::move(clip));
}

Result<void> parse_blend_tree_2d_position(const nlohmann::json& child_json, float& out_x, float& out_y) {
    if (child_json.contains("position") && child_json["position"].is_array()
        && child_json["position"].size() >= 2) {
        out_x = child_json["position"][0].get<float>();
        out_y = child_json["position"][1].get<float>();
        return Result<void>::success();
    }
    if (child_json.contains("x") || child_json.contains("y")) {
        out_x = child_json.value("x", 0.0f);
        out_y = child_json.value("y", 0.0f);
        return Result<void>::success();
    }
    return Result<void>::failure(
        controller_error("ANIM-CTRL-BLEND-POSITION", "blendTree2D child requires position [x,y]"));
}

/// Bowyer–Watson Delaunay triangulation over blend-tree child positions.
Result<void> build_blend_tree_2d_triangles(AnimatorBlendTree2D& tree) {
    tree.triangles.clear();
    const auto n = tree.children.size();
    if (n < 3)
        return Result<void>::failure(controller_error("ANIM-CTRL-BLEND-CHILDREN",
            "blendTree2D requires at least 3 children"));

    struct Pt {
        float x = 0.0f;
        float y = 0.0f;
        bool super = false;
    };
    std::vector<Pt> pts;
    pts.reserve(n + 3);
    float min_x = tree.children[0].position_x;
    float max_x = min_x;
    float min_y = tree.children[0].position_y;
    float max_y = min_y;
    for (const auto& child : tree.children) {
        pts.push_back({child.position_x, child.position_y, false});
        min_x = std::min(min_x, child.position_x);
        max_x = std::max(max_x, child.position_x);
        min_y = std::min(min_y, child.position_y);
        max_y = std::max(max_y, child.position_y);
    }
    const float dx = std::max(max_x - min_x, 1.0f);
    const float dy = std::max(max_y - min_y, 1.0f);
    const float mid_x = 0.5f * (min_x + max_x);
    const float mid_y = 0.5f * (min_y + max_y);
    const float pad = 20.0f * std::max(dx, dy);
    pts.push_back({mid_x - pad, mid_y - pad, true});
    pts.push_back({mid_x + pad, mid_y - pad, true});
    pts.push_back({mid_x, mid_y + pad, true});

    struct Tri {
        int a = 0;
        int b = 0;
        int c = 0;
    };
    std::vector<Tri> tris;
    const int s0 = static_cast<int>(n);
    const int s1 = s0 + 1;
    const int s2 = s0 + 2;
    tris.push_back({s0, s1, s2});

    auto circumcircle_contains = [&](const Tri& tri, float px, float py) {
        const float ax = pts[static_cast<std::size_t>(tri.a)].x;
        const float ay = pts[static_cast<std::size_t>(tri.a)].y;
        const float bx = pts[static_cast<std::size_t>(tri.b)].x;
        const float by = pts[static_cast<std::size_t>(tri.b)].y;
        const float cx = pts[static_cast<std::size_t>(tri.c)].x;
        const float cy = pts[static_cast<std::size_t>(tri.c)].y;
        const float a = ax * (by - cy) - ay * (bx - cx) + bx * cy - cx * by;
        if (std::fabs(a) < 1e-12f) return false;
        const float a_sq = ax * ax + ay * ay;
        const float b_sq = bx * bx + by * by;
        const float c_sq = cx * cx + cy * cy;
        const float ux = (a_sq * (by - cy) + b_sq * (cy - ay) + c_sq * (ay - by)) / (2.0f * a);
        const float uy = (a_sq * (cx - bx) + b_sq * (ax - cx) + c_sq * (bx - ax)) / (2.0f * a);
        const float r2 = (ax - ux) * (ax - ux) + (ay - uy) * (ay - uy);
        const float d2 = (px - ux) * (px - ux) + (py - uy) * (py - uy);
        return d2 <= r2 + 1e-8f;
    };

    auto orient = [&](int i, int j, int k) {
        const float ax = pts[static_cast<std::size_t>(i)].x;
        const float ay = pts[static_cast<std::size_t>(i)].y;
        const float bx = pts[static_cast<std::size_t>(j)].x;
        const float by = pts[static_cast<std::size_t>(j)].y;
        const float cx = pts[static_cast<std::size_t>(k)].x;
        const float cy = pts[static_cast<std::size_t>(k)].y;
        return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    };

    for (int pi = 0; pi < static_cast<int>(n); ++pi) {
        const float px = pts[static_cast<std::size_t>(pi)].x;
        const float py = pts[static_cast<std::size_t>(pi)].y;
        std::vector<std::pair<int, int>> edges;
        std::vector<Tri> next;
        next.reserve(tris.size());
        for (const auto& tri : tris) {
            if (circumcircle_contains(tri, px, py)) {
                edges.push_back({tri.a, tri.b});
                edges.push_back({tri.b, tri.c});
                edges.push_back({tri.c, tri.a});
            } else {
                next.push_back(tri);
            }
        }
        std::vector<std::pair<int, int>> unique;
        for (std::size_t i = 0; i < edges.size(); ++i) {
            auto e = edges[i];
            if (e.first > e.second) std::swap(e.first, e.second);
            bool dup = false;
            for (std::size_t j = 0; j < edges.size(); ++j) {
                if (i == j) continue;
                auto o = edges[j];
                if (o.first > o.second) std::swap(o.first, o.second);
                if (e == o) {
                    dup = true;
                    break;
                }
            }
            if (!dup) unique.push_back(edges[i]);
        }
        for (const auto& e : unique) {
            Tri t{e.first, e.second, pi};
            if (orient(t.a, t.b, t.c) < 0.0f) std::swap(t.b, t.c);
            next.push_back(t);
        }
        tris = std::move(next);
    }

    for (const auto& tri : tris) {
        if (pts[static_cast<std::size_t>(tri.a)].super || pts[static_cast<std::size_t>(tri.b)].super
            || pts[static_cast<std::size_t>(tri.c)].super)
            continue;
        if (std::fabs(orient(tri.a, tri.b, tri.c)) < 1e-10f) continue;
        AnimatorBlendTree2DTriangle out;
        out.i0 = static_cast<std::uint32_t>(tri.a);
        out.i1 = static_cast<std::uint32_t>(tri.b);
        out.i2 = static_cast<std::uint32_t>(tri.c);
        tree.triangles.push_back(out);
    }
    if (tree.triangles.empty())
        return Result<void>::failure(controller_error("ANIM-CTRL-BLEND-TRIANGULATE",
            "blendTree2D children are collinear or degenerate; need a non-degenerate 2D hull"));
    return Result<void>::success();
}

Result<AnimatorMotion> parse_motion(const nlohmann::json& value) {
    if (!value.is_object())
        return Result<AnimatorMotion>::failure(controller_error("ANIM-CTRL-MOTION-INVALID", "motion must be an object"));
    AnimatorMotion motion;
    const auto type = normalize_key(value.value("type", std::string{"clip"}));
    if (type == "clip") {
        motion.type = AnimatorMotionType::Clip;
        const auto clip = parse_clip_ref(value, "clip motion");
        if (!clip) return Result<AnimatorMotion>::failure(clip.error());
        motion.clip = clip.value();
        return Result<AnimatorMotion>::success(std::move(motion));
    }
    if (type == "none" || type == "passthrough" || type == "empty") {
        motion.type = AnimatorMotionType::None;
        return Result<AnimatorMotion>::success(std::move(motion));
    }
    if (type == "blendtree1d" || type == "blend_tree_1d" || type == "blendtree") {
        motion.type = AnimatorMotionType::BlendTree1D;
        motion.blend_tree.parameter = value.value("parameter", std::string{});
        if (motion.blend_tree.parameter.empty())
            return Result<AnimatorMotion>::failure(
                controller_error("ANIM-CTRL-BLEND-PARAM", "blendTree1D requires parameter"));
        if (!value.contains("children") || !value["children"].is_array() || value["children"].empty())
            return Result<AnimatorMotion>::failure(
                controller_error("ANIM-CTRL-BLEND-CHILDREN", "blendTree1D requires non-empty children"));
        for (const auto& child_json : value["children"]) {
            AnimatorBlendTreeChild child;
            child.threshold = child_json.value("threshold", 0.0f);
            const auto clip = parse_clip_ref(child_json, "blendTree1D child");
            if (!clip) return Result<AnimatorMotion>::failure(clip.error());
            child.clip = clip.value();
            motion.blend_tree.children.push_back(std::move(child));
        }
        std::sort(motion.blend_tree.children.begin(), motion.blend_tree.children.end(),
            [](const AnimatorBlendTreeChild& a, const AnimatorBlendTreeChild& b) {
                return a.threshold < b.threshold;
            });
        return Result<AnimatorMotion>::success(std::move(motion));
    }
    if (type == "blendtree2d" || type == "blend_tree_2d") {
        motion.type = AnimatorMotionType::BlendTree2D;
        motion.blend_tree_2d.parameter_x =
            value.value("parameterX", value.value("parameter_x", std::string{}));
        motion.blend_tree_2d.parameter_y =
            value.value("parameterY", value.value("parameter_y", std::string{}));
        if (motion.blend_tree_2d.parameter_x.empty() || motion.blend_tree_2d.parameter_y.empty())
            return Result<AnimatorMotion>::failure(
                controller_error("ANIM-CTRL-BLEND-PARAM", "blendTree2D requires parameterX and parameterY"));
        if (!value.contains("children") || !value["children"].is_array())
            return Result<AnimatorMotion>::failure(
                controller_error("ANIM-CTRL-BLEND-CHILDREN", "blendTree2D requires children array"));
        for (const auto& child_json : value["children"]) {
            AnimatorBlendTree2DChild child;
            const auto pos = parse_blend_tree_2d_position(child_json, child.position_x, child.position_y);
            if (!pos) return Result<AnimatorMotion>::failure(pos.error());
            const auto clip = parse_clip_ref(child_json, "blendTree2D child");
            if (!clip) return Result<AnimatorMotion>::failure(clip.error());
            child.clip = clip.value();
            motion.blend_tree_2d.children.push_back(std::move(child));
        }
        if (const auto built = build_blend_tree_2d_triangles(motion.blend_tree_2d); !built)
            return Result<AnimatorMotion>::failure(built.error());
        return Result<AnimatorMotion>::success(std::move(motion));
    }
    return Result<AnimatorMotion>::failure(controller_error("ANIM-CTRL-MOTION-TYPE", "Unsupported motion type: " + type));
}

Result<AnimatorCondition> parse_condition(const nlohmann::json& value) {
    if (!value.is_object())
        return Result<AnimatorCondition>::failure(
            controller_error("ANIM-CTRL-CONDITION-INVALID", "condition must be an object"));
    AnimatorCondition condition;
    condition.parameter = value.value("parameter", value.value("param", std::string{}));
    if (condition.parameter.empty())
        return Result<AnimatorCondition>::failure(
            controller_error("ANIM-CTRL-CONDITION-PARAM", "condition requires parameter"));
    const auto op = parse_condition_op(value.value("op", std::string{"greater"}));
    if (!op) return Result<AnimatorCondition>::failure(op.error());
    condition.op = op.value();
    if (value.contains("value")) {
        if (value["value"].is_boolean()) condition.bool_value = value["value"].get<bool>();
        else if (value["value"].is_number()) condition.value = value["value"].get<float>();
    }
    if (value.contains("boolValue")) condition.bool_value = value["boolValue"].get<bool>();
    return Result<AnimatorCondition>::success(std::move(condition));
}

nlohmann::json write_clip_ref(const AnimatorClipRef& clip) {
    return {{"clipSource", clip.clip_source}, {"clip", clip.clip}, {"loop", clip.loop}, {"speed", clip.speed}};
}

nlohmann::json write_motion(const AnimatorMotion& motion) {
    if (motion.type == AnimatorMotionType::None) return {{"type", "none"}};
    if (motion.type == AnimatorMotionType::Clip) {
        auto json = write_clip_ref(motion.clip);
        json["type"] = "clip";
        return json;
    }
    if (motion.type == AnimatorMotionType::BlendTree2D) {
        nlohmann::json children = nlohmann::json::array();
        for (const auto& child : motion.blend_tree_2d.children) {
            auto entry = write_clip_ref(child.clip);
            entry["position"] = nlohmann::json::array({child.position_x, child.position_y});
            children.push_back(std::move(entry));
        }
        return {{"type", "blendTree2D"}, {"parameterX", motion.blend_tree_2d.parameter_x},
            {"parameterY", motion.blend_tree_2d.parameter_y}, {"children", std::move(children)}};
    }
    nlohmann::json children = nlohmann::json::array();
    for (const auto& child : motion.blend_tree.children) {
        auto entry = write_clip_ref(child.clip);
        entry["threshold"] = child.threshold;
        children.push_back(std::move(entry));
    }
    return {{"type", "blendTree1D"}, {"parameter", motion.blend_tree.parameter}, {"children", std::move(children)}};
}

} // namespace

const char* to_string(AnimatorParameterType value) noexcept {
    switch (value) {
    case AnimatorParameterType::Float: return "float";
    case AnimatorParameterType::Bool: return "bool";
    case AnimatorParameterType::Trigger: return "trigger";
    }
    return "float";
}

const char* to_string(AnimatorConditionOp value) noexcept {
    switch (value) {
    case AnimatorConditionOp::Greater: return "greater";
    case AnimatorConditionOp::GreaterOrEqual: return "greaterOrEqual";
    case AnimatorConditionOp::Less: return "less";
    case AnimatorConditionOp::LessOrEqual: return "lessOrEqual";
    case AnimatorConditionOp::Equal: return "equal";
    case AnimatorConditionOp::NotEqual: return "notEqual";
    case AnimatorConditionOp::Trigger: return "trigger";
    }
    return "greater";
}

const char* to_string(AnimatorMotionType value) noexcept {
    switch (value) {
    case AnimatorMotionType::Clip: return "clip";
    case AnimatorMotionType::BlendTree1D: return "blendTree1D";
    case AnimatorMotionType::BlendTree2D: return "blendTree2D";
    case AnimatorMotionType::None: return "none";
    }
    return "clip";
}

const char* to_string(AnimatorLayerBlendMode value) noexcept {
    switch (value) {
    case AnimatorLayerBlendMode::Override: return "override";
    }
    return "override";
}

Result<void> AnimatorControllerAsset::validate() const {
    if (schema_version != 1)
        return Result<void>::failure(controller_error("ANIM-CTRL-SCHEMA", "Unsupported schemaVersion"));
    if (id.empty()) return Result<void>::failure(controller_error("ANIM-CTRL-ID", "Controller id is required"));
    if (layers.empty())
        return Result<void>::failure(controller_error("ANIM-CTRL-LAYERS", "Controller requires at least one layer"));

    std::unordered_set<std::string> param_names;
    for (const auto& param : parameters) {
        if (param.name.empty())
            return Result<void>::failure(controller_error("ANIM-CTRL-PARAM-NAME", "Parameter name is required"));
        if (!param_names.insert(param.name).second)
            return Result<void>::failure(
                controller_error("ANIM-CTRL-PARAM-DUP", "Duplicate parameter: " + param.name));
    }

    std::unordered_set<std::string> layer_names;
    for (const auto& layer : layers) {
        if (layer.name.empty())
            return Result<void>::failure(controller_error("ANIM-CTRL-LAYER-NAME", "Layer name is required"));
        if (!layer_names.insert(layer.name).second)
            return Result<void>::failure(controller_error("ANIM-CTRL-LAYER-DUP", "Duplicate layer: " + layer.name));
        if (layer.states.empty())
            return Result<void>::failure(
                controller_error("ANIM-CTRL-STATES", "Layer '" + layer.name + "' requires at least one state"));
        if (!(layer.weight >= 0.0f) || !std::isfinite(layer.weight))
            return Result<void>::failure(controller_error(
                "ANIM-CTRL-LAYER-WEIGHT", "Layer '" + layer.name + "' weight must be finite and >= 0"));
        for (const auto& joint : layer.mask.joints) {
            if (joint.empty())
                return Result<void>::failure(
                    controller_error("ANIM-CTRL-MASK-JOINT", "Layer '" + layer.name + "' mask joint is empty"));
        }
        if (layer.default_state.empty())
            return Result<void>::failure(
                controller_error("ANIM-CTRL-DEFAULT", "Layer '" + layer.name + "' requires defaultState"));

        std::unordered_set<std::string> state_names;
        for (const auto& state : layer.states) {
            if (state.name.empty())
                return Result<void>::failure(controller_error("ANIM-CTRL-STATE-NAME", "State name is required"));
            if (!state_names.insert(state.name).second)
                return Result<void>::failure(controller_error("ANIM-CTRL-STATE-DUP",
                    "Duplicate state '" + state.name + "' in layer '" + layer.name + "'"));
            if (state.motion.type == AnimatorMotionType::BlendTree1D) {
                if (!param_names.count(state.motion.blend_tree.parameter))
                    return Result<void>::failure(controller_error("ANIM-CTRL-BLEND-PARAM-MISSING",
                        "blendTree1D parameter '" + state.motion.blend_tree.parameter + "' is not declared"));
                const auto* param = find_parameter(state.motion.blend_tree.parameter);
                if (!param || param->type != AnimatorParameterType::Float)
                    return Result<void>::failure(controller_error("ANIM-CTRL-BLEND-PARAM-TYPE",
                        "blendTree1D parameter must be float: " + state.motion.blend_tree.parameter));
            }
            if (state.motion.type == AnimatorMotionType::BlendTree2D) {
                const auto& tree = state.motion.blend_tree_2d;
                if (!param_names.count(tree.parameter_x) || !param_names.count(tree.parameter_y))
                    return Result<void>::failure(controller_error("ANIM-CTRL-BLEND-PARAM-MISSING",
                        "blendTree2D parameterX/parameterY must be declared"));
                const auto* px = find_parameter(tree.parameter_x);
                const auto* py = find_parameter(tree.parameter_y);
                if (!px || px->type != AnimatorParameterType::Float || !py
                    || py->type != AnimatorParameterType::Float)
                    return Result<void>::failure(controller_error("ANIM-CTRL-BLEND-PARAM-TYPE",
                        "blendTree2D parameterX and parameterY must be float"));
                if (tree.children.size() < 3)
                    return Result<void>::failure(controller_error("ANIM-CTRL-BLEND-CHILDREN",
                        "blendTree2D requires at least 3 children"));
                if (tree.triangles.empty())
                    return Result<void>::failure(controller_error("ANIM-CTRL-BLEND-TRIANGULATE",
                        "blendTree2D has no triangulation"));
            }
        }
        if (!state_names.count(layer.default_state))
            return Result<void>::failure(controller_error("ANIM-CTRL-DEFAULT-MISSING",
                "defaultState '" + layer.default_state + "' missing on layer '" + layer.name + "'"));

        for (const auto& transition : layer.transitions) {
            if (transition.to.empty())
                return Result<void>::failure(controller_error("ANIM-CTRL-TRANSITION-TO", "transition.to is required"));
            if (!state_names.count(transition.to))
                return Result<void>::failure(controller_error("ANIM-CTRL-TRANSITION-TO-MISSING",
                    "transition.to '" + transition.to + "' missing on layer '" + layer.name + "'"));
            if (transition.from != "*" && !state_names.count(transition.from))
                return Result<void>::failure(controller_error("ANIM-CTRL-TRANSITION-FROM-MISSING",
                    "transition.from '" + transition.from + "' missing on layer '" + layer.name + "'"));
            if (transition.duration < 0.0f)
                return Result<void>::failure(
                    controller_error("ANIM-CTRL-TRANSITION-DURATION", "transition.duration must be >= 0"));
            for (const auto& condition : transition.conditions) {
                if (!param_names.count(condition.parameter))
                    return Result<void>::failure(controller_error("ANIM-CTRL-CONDITION-PARAM-MISSING",
                        "condition parameter '" + condition.parameter + "' is not declared"));
                const auto* param = find_parameter(condition.parameter);
                if (!param) continue;
                if (condition.op == AnimatorConditionOp::Trigger && param->type != AnimatorParameterType::Trigger)
                    return Result<void>::failure(controller_error("ANIM-CTRL-CONDITION-TRIGGER",
                        "trigger op requires trigger parameter: " + condition.parameter));
                if (condition.op != AnimatorConditionOp::Trigger && param->type == AnimatorParameterType::Trigger)
                    return Result<void>::failure(controller_error("ANIM-CTRL-CONDITION-TRIGGER-OP",
                        "trigger parameter requires trigger op: " + condition.parameter));
            }
        }
    }

    for (const auto& event : timeline_events) {
        if (event.name.empty())
            return Result<void>::failure(controller_error("ANIM-CTRL-EVENT-NAME", "timelineEvent.name is required"));
        if (event.state.empty())
            return Result<void>::failure(controller_error("ANIM-CTRL-EVENT-STATE", "timelineEvent.state is required"));
        if (event.time < 0.0f)
            return Result<void>::failure(controller_error("ANIM-CTRL-EVENT-TIME", "timelineEvent.time must be >= 0"));
        bool state_found = false;
        for (const auto& layer : layers) {
            if (!event.layer.empty() && layer.name != event.layer) continue;
            for (const auto& state : layer.states) {
                if (state.name == event.state) {
                    state_found = true;
                    break;
                }
            }
            if (state_found) break;
        }
        if (!state_found)
            return Result<void>::failure(controller_error("ANIM-CTRL-EVENT-STATE-MISSING",
                "timelineEvent state '" + event.state + "' not found"
                    + (event.layer.empty() ? "" : (" on layer '" + event.layer + "'"))));
        if (!event.layer.empty() && !layer_names.count(event.layer))
            return Result<void>::failure(
                controller_error("ANIM-CTRL-EVENT-LAYER-MISSING", "timelineEvent.layer missing: " + event.layer));
    }
    return Result<void>::success();
}

const AnimatorLayerDef* AnimatorControllerAsset::find_layer(const std::string& name) const {
    for (const auto& layer : layers) {
        if (layer.name == name) return &layer;
    }
    return nullptr;
}

const AnimatorParameterDef* AnimatorControllerAsset::find_parameter(const std::string& name) const {
    for (const auto& param : parameters) {
        if (param.name == name) return &param;
    }
    return nullptr;
}

Result<AnimatorControllerAsset> AnimatorControllerAsset::parse(const std::string& text,
    const std::string& source_name) {
    nlohmann::json document;
    try {
        document = nlohmann::json::parse(text);
    } catch (const std::exception& exception) {
        auto error = controller_error("ANIM-CTRL-JSON-PARSE", "Could not parse animator controller: " + source_name);
        error.causes.push_back(exception.what());
        return Result<AnimatorControllerAsset>::failure(std::move(error));
    }
    if (!document.is_object())
        return Result<AnimatorControllerAsset>::failure(
            controller_error("ANIM-CTRL-JSON-OBJECT", "Animator controller root must be an object"));

    AnimatorControllerAsset asset;
    asset.schema_version = document.value("schemaVersion", 1);
    asset.id = document.value("id", std::string{});
    asset.apply_root_motion = document.value("applyRootMotion", document.value("apply_root_motion", false));
    asset.root_joint = document.value("rootJoint", document.value("root_joint", std::string{}));
    asset.root_motion_y = document.value("rootMotionY", document.value("root_motion_y", false));
    const auto kind = normalize_key(document.value("kind", std::string{"animatorcontroller"}));
    if (kind != "animatorcontroller" && kind != "animator")
        return Result<AnimatorControllerAsset>::failure(
            controller_error("ANIM-CTRL-KIND", "kind must be animatorController"));

    if (document.contains("parameters") && document["parameters"].is_array()) {
        for (const auto& entry : document["parameters"]) {
            AnimatorParameterDef param;
            param.name = entry.value("name", std::string{});
            const auto type = parse_parameter_type(entry.value("type", std::string{"float"}));
            if (!type) return Result<AnimatorControllerAsset>::failure(type.error());
            param.type = type.value();
            if (entry.contains("default")) {
                if (entry["default"].is_boolean()) param.default_bool = entry["default"].get<bool>();
                else if (entry["default"].is_number()) param.default_float = entry["default"].get<float>();
            }
            asset.parameters.push_back(std::move(param));
        }
    }

    if (!document.contains("layers") || !document["layers"].is_array())
        return Result<AnimatorControllerAsset>::failure(
            controller_error("ANIM-CTRL-LAYERS", "layers array is required"));
    for (const auto& layer_json : document["layers"]) {
        AnimatorLayerDef layer;
        layer.name = layer_json.value("name", std::string{});
        layer.default_state = layer_json.value("defaultState", layer_json.value("default_state", std::string{}));
        const auto blend = normalize_key(layer_json.value("blendMode", std::string{"override"}));
        if (blend != "override")
            return Result<AnimatorControllerAsset>::failure(
                controller_error("ANIM-CTRL-BLEND-MODE", "Unsupported blendMode (v1 supports override only)"));
        layer.blend_mode = AnimatorLayerBlendMode::Override;
        layer.weight = layer_json.value("weight", 1.0f);
        if (layer_json.contains("mask") && layer_json["mask"].is_object()) {
            const auto& mask_json = layer_json["mask"];
            layer.mask.include_children =
                mask_json.value("includeChildren", mask_json.value("include_children", true));
            if (mask_json.contains("joints") && mask_json["joints"].is_array()) {
                for (const auto& joint_json : mask_json["joints"]) {
                    if (!joint_json.is_string())
                        return Result<AnimatorControllerAsset>::failure(
                            controller_error("ANIM-CTRL-MASK-JOINT", "mask.joints entries must be strings"));
                    const auto name = joint_json.get<std::string>();
                    if (name.empty())
                        return Result<AnimatorControllerAsset>::failure(
                            controller_error("ANIM-CTRL-MASK-JOINT", "mask.joints entries must be non-empty"));
                    layer.mask.joints.push_back(name);
                }
            }
        }

        if (!layer_json.contains("states") || !layer_json["states"].is_array())
            return Result<AnimatorControllerAsset>::failure(
                controller_error("ANIM-CTRL-STATES", "layer.states array is required"));
        for (const auto& state_json : layer_json["states"]) {
            AnimatorStateDef state;
            state.name = state_json.value("name", std::string{});
            const auto& motion_json = state_json.contains("motion") ? state_json.at("motion") : state_json;
            const auto motion = parse_motion(motion_json);
            if (!motion) return Result<AnimatorControllerAsset>::failure(motion.error());
            state.motion = motion.value();
            layer.states.push_back(std::move(state));
        }

        if (layer_json.contains("transitions") && layer_json["transitions"].is_array()) {
            for (const auto& transition_json : layer_json["transitions"]) {
                AnimatorTransition transition;
                transition.from = transition_json.value("from", std::string{});
                transition.to = transition_json.value("to", std::string{});
                transition.duration = transition_json.value("duration", 0.15f);
                transition.has_exit_time = transition_json.value("hasExitTime", false);
                transition.exit_time = transition_json.value("exitTime", 1.0f);
                if (transition_json.contains("conditions") && transition_json["conditions"].is_array()) {
                    for (const auto& condition_json : transition_json["conditions"]) {
                        const auto condition = parse_condition(condition_json);
                        if (!condition) return Result<AnimatorControllerAsset>::failure(condition.error());
                        transition.conditions.push_back(condition.value());
                    }
                }
                layer.transitions.push_back(std::move(transition));
            }
        }
        asset.layers.push_back(std::move(layer));
    }

    if (document.contains("timelineEvents") && document["timelineEvents"].is_array()) {
        for (const auto& event_json : document["timelineEvents"]) {
            AnimatorTimelineEvent event;
            event.state = event_json.value("state", std::string{});
            event.layer = event_json.value("layer", std::string{});
            event.time = event_json.value("time", 0.0f);
            event.name = event_json.value("name", std::string{});
            if (event_json.contains("payload")) {
                if (event_json["payload"].is_string())
                    event.payload_json = event_json["payload"].get<std::string>();
                else
                    event.payload_json = event_json["payload"].dump();
            }
            if (event.payload_json.empty()) event.payload_json = "{}";
            asset.timeline_events.push_back(std::move(event));
        }
    }

    const auto valid = asset.validate();
    if (!valid) return Result<AnimatorControllerAsset>::failure(valid.error());
    return Result<AnimatorControllerAsset>::success(std::move(asset));
}

Result<AnimatorControllerAsset> AnimatorControllerAsset::load(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input)
        return Result<AnimatorControllerAsset>::failure(
            controller_error("ANIM-CTRL-READ", "Could not read animator controller: " + path.generic_string(),
                "Create a *.animator.json under the project."));
    const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    return parse(text, path.generic_string());
}

std::string AnimatorControllerAsset::to_json() const {
    nlohmann::json document;
    document["schemaVersion"] = schema_version;
    document["kind"] = "animatorController";
    document["id"] = id;
    document["applyRootMotion"] = apply_root_motion;
    if (!root_joint.empty()) document["rootJoint"] = root_joint;
    document["rootMotionY"] = root_motion_y;
    nlohmann::json parameters_json = nlohmann::json::array();
    for (const auto& param : parameters) {
        nlohmann::json entry{{"name", param.name}, {"type", to_string(param.type)}};
        if (param.type == AnimatorParameterType::Bool) entry["default"] = param.default_bool;
        else if (param.type == AnimatorParameterType::Float) entry["default"] = param.default_float;
        parameters_json.push_back(std::move(entry));
    }
    document["parameters"] = std::move(parameters_json);

    nlohmann::json layers_json = nlohmann::json::array();
    for (const auto& layer : layers) {
        nlohmann::json layer_json{{"name", layer.name}, {"defaultState", layer.default_state},
            {"blendMode", to_string(layer.blend_mode)}, {"weight", layer.weight}};
        if (!layer.mask.joints.empty()) {
            layer_json["mask"] = {{"joints", layer.mask.joints}, {"includeChildren", layer.mask.include_children}};
        }
        nlohmann::json states_json = nlohmann::json::array();
        for (const auto& state : layer.states) {
            states_json.push_back({{"name", state.name}, {"motion", write_motion(state.motion)}});
        }
        layer_json["states"] = std::move(states_json);
        nlohmann::json transitions_json = nlohmann::json::array();
        for (const auto& transition : layer.transitions) {
            nlohmann::json conditions_json = nlohmann::json::array();
            for (const auto& condition : transition.conditions) {
                nlohmann::json condition_json{{"parameter", condition.parameter}, {"op", to_string(condition.op)}};
                if (condition.op == AnimatorConditionOp::Equal || condition.op == AnimatorConditionOp::NotEqual) {
                    // Prefer bool when comparing bools; callers still may set float.
                    condition_json["value"] = condition.bool_value;
                    condition_json["floatValue"] = condition.value;
                } else if (condition.op != AnimatorConditionOp::Trigger) {
                    condition_json["value"] = condition.value;
                }
                conditions_json.push_back(std::move(condition_json));
            }
            nlohmann::json transition_json{{"from", transition.from}, {"to", transition.to},
                {"duration", transition.duration}, {"hasExitTime", transition.has_exit_time},
                {"exitTime", transition.exit_time}, {"conditions", std::move(conditions_json)}};
            transitions_json.push_back(std::move(transition_json));
        }
        layer_json["transitions"] = std::move(transitions_json);
        layers_json.push_back(std::move(layer_json));
    }
    document["layers"] = std::move(layers_json);
    nlohmann::json events_json = nlohmann::json::array();
    for (const auto& event : timeline_events) {
        nlohmann::json entry{{"state", event.state}, {"time", event.time}, {"name", event.name}};
        if (!event.layer.empty()) entry["layer"] = event.layer;
        try {
            entry["payload"] = nlohmann::json::parse(event.payload_json.empty() ? "{}" : event.payload_json);
        } catch (...) {
            entry["payload"] = nlohmann::json::object();
        }
        events_json.push_back(std::move(entry));
    }
    document["timelineEvents"] = std::move(events_json);
    return document.dump(2);
}

Result<void> AnimatorControllerAsset::save_atomic(const std::filesystem::path& path) const {
    const auto valid = validate();
    if (!valid) return valid;
    const auto parent = path.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    const auto temp = path.string() + ".tmp";
    const auto backup = path.string() + ".bak";
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out)
            return Result<void>::failure(
                controller_error("ANIM-CTRL-WRITE", "Could not write temp controller file", "Check permissions."));
        out << to_json();
    }
    if (std::filesystem::exists(path))
        std::filesystem::copy_file(path, backup, std::filesystem::copy_options::overwrite_existing);
    std::filesystem::rename(temp, path);
    if (std::filesystem::exists(backup)) std::filesystem::remove(backup);
    return Result<void>::success();
}

} // namespace engine
