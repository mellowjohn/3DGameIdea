#include "engine/animation/anim_studio_agent_ops.h"

#include "engine/world/transform_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <optional>
#include <utility>

namespace engine {
namespace {

constexpr float k_pi = 3.14159265358979323846f;
constexpr float k_rad_to_deg = 180.0f / k_pi;
constexpr float k_deg_to_rad = k_pi / 180.0f;

std::string to_lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

const char* path_to_string(AnimationChannelPath path) {
    switch (path) {
    case AnimationChannelPath::Rotation: return "rotation";
    case AnimationChannelPath::Scale: return "scale";
    case AnimationChannelPath::Translation:
    default: return "translation";
    }
}

bool path_matches_filter(AnimationChannelPath path, const std::string& path_filter) {
    if (path_filter.empty()) return true;
    const auto want = to_lower_copy(path_filter);
    if (want == "rot") return path == AnimationChannelPath::Rotation;
    return want == path_to_string(path);
}

bool joint_matches_filters(const std::string& channel_name, bool apply_sagittal,
    const std::vector<std::string>& filters) {
    if (filters.empty()) return true;
    const std::string skin_name = apply_sagittal ? sagittal_joint_name(channel_name) : channel_name;
    for (const auto& filter : filters) {
        if (filter.empty()) continue;
        if (filter == channel_name || filter == skin_name) return true;
        if (apply_sagittal && sagittal_joint_name(filter) == channel_name) return true;
    }
    return false;
}

int channel_components(AnimationChannelPath path) {
    return path == AnimationChannelPath::Rotation ? 4 : 3;
}

nlohmann::json float_array_json(const float* data, int count) {
    nlohmann::json arr = nlohmann::json::array();
    for (int i = 0; i < count; ++i) arr.push_back(data[i]);
    return arr;
}

void upsert_channel_key(AnimationClipChannel& channel, float time, const float* values, int components) {
    const float eps = 1.0e-4f;
    int replace = -1;
    for (int i = 0; i < static_cast<int>(channel.times.size()); ++i) {
        if (std::abs(channel.times[static_cast<std::size_t>(i)] - time) <= eps) {
            replace = i;
            break;
        }
    }
    if (replace >= 0) {
        const std::size_t offset = static_cast<std::size_t>(replace) * static_cast<std::size_t>(components);
        for (int c = 0; c < components; ++c) channel.values[offset + static_cast<std::size_t>(c)] = values[c];
        channel.times[static_cast<std::size_t>(replace)] = time;
        return;
    }
    std::size_t insert_at = 0;
    while (insert_at < channel.times.size() && channel.times[insert_at] < time) ++insert_at;
    channel.times.insert(channel.times.begin() + static_cast<std::ptrdiff_t>(insert_at), time);
    channel.values.insert(channel.values.begin() + static_cast<std::ptrdiff_t>(insert_at * components), values,
        values + components);
}

std::array<float, 3> translation_at(const AnimationClip& clip, const std::string& channel_name, float time) {
    for (const auto& channel : clip.channels) {
        if (channel.target_node_name != channel_name || channel.path != AnimationChannelPath::Translation)
            continue;
        if (auto sampled = sample_translation_channel(channel, time)) return sampled.value();
    }
    return {0, 0, 0};
}

std::array<float, 4> rotation_at(const AnimationClip& clip, const std::string& channel_name, float time) {
    for (const auto& channel : clip.channels) {
        if (channel.target_node_name != channel_name || channel.path != AnimationChannelPath::Rotation) continue;
        if (auto sampled = sample_rotation_channel(channel, time)) return sampled.value();
    }
    return {0, 0, 0, 1};
}

std::array<float, 3> scale_at(const AnimationClip& clip, const std::string& channel_name, float time) {
    for (const auto& channel : clip.channels) {
        if (channel.target_node_name != channel_name || channel.path != AnimationChannelPath::Scale) continue;
        if (auto sampled = sample_scale_channel(channel, time)) return sampled.value();
    }
    return {1, 1, 1};
}

std::array<float, 4> slerp_quat(std::array<float, 4> a, std::array<float, 4> b, float t) {
    auto norm = [](std::array<float, 4> q) {
        const float len = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
        if (!(len > 1e-8f)) return std::array<float, 4>{0, 0, 0, 1};
        return std::array<float, 4>{q[0] / len, q[1] / len, q[2] / len, q[3] / len};
    };
    a = norm(a);
    b = norm(b);
    float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    if (dot < 0.0f) {
        b = {-b[0], -b[1], -b[2], -b[3]};
        dot = -dot;
    }
    if (dot > 0.9995f) {
        return norm({a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t,
            a[3] + (b[3] - a[3]) * t});
    }
    const float theta = std::acos(std::clamp(dot, -1.0f, 1.0f));
    const float sin_theta = std::sin(theta);
    const float w1 = std::sin((1.0f - t) * theta) / sin_theta;
    const float w2 = std::sin(t * theta) / sin_theta;
    return norm({a[0] * w1 + b[0] * w2, a[1] * w1 + b[1] * w2, a[2] * w1 + b[2] * w2, a[3] * w1 + b[3] * w2});
}

nlohmann::json joint_pose_entry(const std::string& channel_name, const std::string& skin_name,
    const std::array<float, 3>& t, const std::array<float, 4>& r, const std::array<float, 3>& s,
    bool include_euler, const float* world_xyz_or_null) {
    nlohmann::json j;
    j["channelName"] = channel_name;
    j["skinName"] = skin_name;
    j["translation"] = float_array_json(t.data(), 3);
    j["rotation"] = float_array_json(r.data(), 4);
    j["scale"] = float_array_json(s.data(), 3);
    if (include_euler) j["eulerDeg"] = float_array_json(quat_xyzw_to_euler_deg(r).data(), 3);
    if (world_xyz_or_null) {
        j["world"] = {{"x", world_xyz_or_null[0]}, {"y", world_xyz_or_null[1]}, {"z", world_xyz_or_null[2]}};
    }
    return j;
}

} // namespace

std::array<float, 3> quat_xyzw_to_euler_deg(const std::array<float, 4>& q) noexcept {
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    // XYZ intrinsic (matches RollPitchYaw / Studio upsert path).
    const float sinr_cosp = 2.0f * (w * x + y * z);
    const float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
    const float roll = std::atan2(sinr_cosp, cosr_cosp);

    const float sinp = 2.0f * (w * y - z * x);
    const float pitch = std::abs(sinp) >= 1.0f ? std::copysign(k_pi * 0.5f, sinp) : std::asin(sinp);

    const float siny_cosp = 2.0f * (w * z + x * y);
    const float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
    const float yaw = std::atan2(siny_cosp, cosy_cosp);

    return {roll * k_rad_to_deg, pitch * k_rad_to_deg, yaw * k_rad_to_deg};
}

std::array<float, 4> euler_deg_to_quat_xyzw(const std::array<float, 3>& euler_deg) noexcept {
    const float rx = euler_deg[0] * k_deg_to_rad;
    const float ry = euler_deg[1] * k_deg_to_rad;
    const float rz = euler_deg[2] * k_deg_to_rad;
    const float cx = std::cos(rx * 0.5f);
    const float sx = std::sin(rx * 0.5f);
    const float cy = std::cos(ry * 0.5f);
    const float sy = std::sin(ry * 0.5f);
    const float cz = std::cos(rz * 0.5f);
    const float sz = std::sin(rz * 0.5f);
    std::array<float, 4> values{
        sx * cy * cz - cx * sy * sz,
        cx * sy * cz + sx * cy * sz,
        cx * cy * sz - sx * sy * cz,
        cx * cy * cz + sx * sy * sz,
    };
    const float n = std::sqrt(values[0] * values[0] + values[1] * values[1] + values[2] * values[2]
        + values[3] * values[3]);
    if (n > 1e-8f) {
        values[0] /= n;
        values[1] /= n;
        values[2] /= n;
        values[3] /= n;
    }
    return values;
}

AnimEasePreset parse_anim_ease_preset(const std::string& name) noexcept {
    const auto lower = to_lower_copy(name);
    if (lower == "easein" || lower == "ease_in" || lower == "in") return AnimEasePreset::EaseIn;
    if (lower == "easeout" || lower == "ease_out" || lower == "out") return AnimEasePreset::EaseOut;
    if (lower == "easeinout" || lower == "ease_in_out" || lower == "inout" || lower == "in_out")
        return AnimEasePreset::EaseInOut;
    return AnimEasePreset::Linear;
}

float evaluate_anim_ease(AnimEasePreset preset, float t01) noexcept {
    const float t = std::clamp(t01, 0.0f, 1.0f);
    switch (preset) {
    case AnimEasePreset::EaseIn: return t * t;
    case AnimEasePreset::EaseOut: {
        const float u = 1.0f - t;
        return 1.0f - u * u;
    }
    case AnimEasePreset::EaseInOut:
        return t < 0.5f ? (2.0f * t * t) : (1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f);
    case AnimEasePreset::Linear:
    default: return t;
    }
}

nlohmann::json list_clip_keys_json(const AnimationClip& clip, bool apply_sagittal_names,
    const std::vector<std::string>& joint_filters, const std::string& path_filter) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& channel : clip.channels) {
        if (!path_matches_filter(channel.path, path_filter)) continue;
        if (!joint_matches_filters(channel.target_node_name, apply_sagittal_names, joint_filters)) continue;
        const int components = channel_components(channel.path);
        nlohmann::json entry;
        entry["channelName"] = channel.target_node_name;
        entry["skinName"] =
            apply_sagittal_names ? sagittal_joint_name(channel.target_node_name) : channel.target_node_name;
        entry["path"] = path_to_string(channel.path);
        entry["interpolation"] = to_string(channel.interpolation);
        entry["times"] = channel.times;
        nlohmann::json values = nlohmann::json::array();
        nlohmann::json eulers = nlohmann::json::array();
        for (std::size_t i = 0; i < channel.times.size(); ++i) {
            const std::size_t offset = i * static_cast<std::size_t>(components);
            if (offset + static_cast<std::size_t>(components) > channel.values.size()) break;
            nlohmann::json key_vals = nlohmann::json::array();
            for (int c = 0; c < components; ++c) key_vals.push_back(channel.values[offset + static_cast<std::size_t>(c)]);
            values.push_back(std::move(key_vals));
            if (channel.path == AnimationChannelPath::Rotation) {
                std::array<float, 4> q{channel.values[offset], channel.values[offset + 1],
                    channel.values[offset + 2], channel.values[offset + 3]};
                eulers.push_back(float_array_json(quat_xyzw_to_euler_deg(q).data(), 3));
            }
        }
        entry["values"] = std::move(values);
        if (channel.path == AnimationChannelPath::Rotation) entry["eulerDeg"] = std::move(eulers);
        out.push_back(std::move(entry));
    }
    return out;
}

nlohmann::json sample_clip_channel_poses_json(const AnimationClip& clip, float time_seconds,
    const std::vector<std::string>& joint_filters, bool include_euler) {
    std::vector<std::string> names;
    for (const auto& channel : clip.channels) {
        if (std::find(names.begin(), names.end(), channel.target_node_name) == names.end())
            names.push_back(channel.target_node_name);
    }
    nlohmann::json out = nlohmann::json::array();
    for (const auto& channel_name : names) {
        if (!joint_matches_filters(channel_name, false, joint_filters)) continue;
        const auto t = translation_at(clip, channel_name, time_seconds);
        const auto r = rotation_at(clip, channel_name, time_seconds);
        const auto s = scale_at(clip, channel_name, time_seconds);
        out.push_back(joint_pose_entry(channel_name, channel_name, t, r, s, include_euler, nullptr));
    }
    return out;
}

    nlohmann::json sample_skinned_pose_json(const ImportedSkin& skin, const AnimationClip& clip, float time_seconds,
    bool apply_sagittal_handedness, const std::vector<std::string>& joint_filters, bool include_euler) {
    std::vector<JointLocalPose> locals(skin.joint_names.size());
    for (std::size_t j = 0; j < skin.joint_names.size(); ++j) {
        JointLocalPose rest{};
        if (j < skin.joint_rest_locals.size()) {
            rest.translation = skin.joint_rest_locals[j].translation;
            rest.rotation = skin.joint_rest_locals[j].rotation;
            rest.scale = skin.joint_rest_locals[j].scale;
        }
        const std::string& skin_name = skin.joint_names[j];
        JointLocalPose pose = apply_sagittal_handedness ? reflect_pose_across_x(rest) : rest;
        const std::string channel_name =
            apply_sagittal_handedness ? sagittal_joint_name(skin_name) : skin_name;
        for (const auto& channel : clip.channels) {
            if (channel.target_node_name != channel_name) continue;
            if (channel.path == AnimationChannelPath::Translation) {
                if (auto sampled = sample_translation_channel(channel, time_seconds))
                    pose.translation = sampled.value();
            } else if (channel.path == AnimationChannelPath::Rotation) {
                if (auto sampled = sample_rotation_channel(channel, time_seconds))
                    pose.rotation = sampled.value();
            } else if (channel.path == AnimationChannelPath::Scale) {
                if (auto sampled = sample_scale_channel(channel, time_seconds)) pose.scale = sampled.value();
            }
        }
        if (apply_sagittal_handedness) pose = reflect_pose_across_x(pose);
        locals[j] = pose;
    }

    auto globals = build_joint_global_matrices(skin, locals);
    nlohmann::json out = nlohmann::json::array();
    if (!globals) return out;
    for (std::size_t j = 0; j < skin.joint_names.size(); ++j) {
        const std::string& skin_name = skin.joint_names[j];
        const std::string channel_name =
            apply_sagittal_handedness ? sagittal_joint_name(skin_name) : skin_name;
        if (!joint_matches_filters(channel_name, apply_sagittal_handedness, joint_filters)
            && !joint_matches_filters(skin_name, false, joint_filters))
            continue;
        const auto& pose = locals[j];
        const auto& m = globals.value()[j];
        // Column-major translation in elements 12,13,14.
        const float world[3] = {m[12], m[13], m[14]};
        out.push_back(joint_pose_entry(channel_name, skin_name, pose.translation, pose.rotation, pose.scale,
            include_euler, world));
    }
    return out;
}

nlohmann::json diff_pose_json(const nlohmann::json& pose_a, const nlohmann::json& pose_b) {
    auto index_by_name = [](const nlohmann::json& poses) {
        std::map<std::string, nlohmann::json> map;
        if (!poses.is_array()) return map;
        for (const auto& p : poses) {
            if (!p.is_object()) continue;
            std::string key = p.value("channelName", std::string{});
            if (key.empty()) key = p.value("skinName", std::string{});
            if (!key.empty()) map[key] = p;
        }
        return map;
    };
    const auto a_map = index_by_name(pose_a);
    const auto b_map = index_by_name(pose_b);
    nlohmann::json out = nlohmann::json::array();
    for (const auto& [key, a] : a_map) {
        const auto found = b_map.find(key);
        if (found == b_map.end()) continue;
        const auto& b = found->second;
        nlohmann::json d;
        d["channelName"] = a.value("channelName", key);
        d["skinName"] = a.value("skinName", key);
        auto delta3 = [](const nlohmann::json& ja, const nlohmann::json& jb, const char* field) {
            nlohmann::json delta = nlohmann::json::array();
            if (!ja.contains(field) || !jb.contains(field) || !ja[field].is_array() || !jb[field].is_array())
                return delta;
            const auto& aa = ja[field];
            const auto& bb = jb[field];
            const std::size_t n = (std::min)(aa.size(), bb.size());
            for (std::size_t i = 0; i < n; ++i) {
                if (aa[i].is_number() && bb[i].is_number())
                    delta.push_back(bb[i].get<float>() - aa[i].get<float>());
            }
            return delta;
        };
        d["deltaTranslation"] = delta3(a, b, "translation");
        d["deltaEulerDeg"] = delta3(a, b, "eulerDeg");
        d["deltaScale"] = delta3(a, b, "scale");
        if (a.contains("world") && b.contains("world") && a["world"].is_object() && b["world"].is_object()) {
            d["deltaWorld"] = {
                {"x", b["world"].value("x", 0.0f) - a["world"].value("x", 0.0f)},
                {"y", b["world"].value("y", 0.0f) - a["world"].value("y", 0.0f)},
                {"z", b["world"].value("z", 0.0f) - a["world"].value("z", 0.0f)},
            };
        }
        out.push_back(std::move(d));
    }
    return out;
}

int insert_ease_breakdowns(AnimationClip& clip, float t0, float t1, int count, AnimEasePreset preset,
    const std::vector<std::string>& joint_filters, const std::string& path_filter) {
    if (!(t1 > t0) || count <= 0) return 0;
    count = std::clamp(count, 1, 32);
    int inserted = 0;
    constexpr float k_eps = 1.0e-4f;
    for (auto& channel : clip.channels) {
        if (!path_matches_filter(channel.path, path_filter)) continue;
        if (!joint_matches_filters(channel.target_node_name, false, joint_filters)
            && !joint_matches_filters(channel.target_node_name, true, joint_filters))
            continue;
        const int components = channel_components(channel.path);
        if (channel.times.empty()) continue;

        std::vector<float> start(static_cast<std::size_t>(components));
        std::vector<float> end(static_cast<std::size_t>(components));
        if (channel.path == AnimationChannelPath::Rotation) {
            auto a = sample_rotation_channel(channel, t0);
            auto b = sample_rotation_channel(channel, t1);
            if (!a || !b) continue;
            for (int c = 0; c < 4; ++c) {
                start[static_cast<std::size_t>(c)] = a.value()[static_cast<std::size_t>(c)];
                end[static_cast<std::size_t>(c)] = b.value()[static_cast<std::size_t>(c)];
            }
        } else if (channel.path == AnimationChannelPath::Translation) {
            auto a = sample_translation_channel(channel, t0);
            auto b = sample_translation_channel(channel, t1);
            if (!a || !b) continue;
            for (int c = 0; c < 3; ++c) {
                start[static_cast<std::size_t>(c)] = a.value()[static_cast<std::size_t>(c)];
                end[static_cast<std::size_t>(c)] = b.value()[static_cast<std::size_t>(c)];
            }
        } else {
            auto a = sample_scale_channel(channel, t0);
            auto b = sample_scale_channel(channel, t1);
            if (!a || !b) continue;
            for (int c = 0; c < 3; ++c) {
                start[static_cast<std::size_t>(c)] = a.value()[static_cast<std::size_t>(c)];
                end[static_cast<std::size_t>(c)] = b.value()[static_cast<std::size_t>(c)];
            }
        }

        // Drop keys strictly inside the segment so eased endpoint slerps do not fight denser motion.
        {
            std::vector<float> keep_times;
            std::vector<float> keep_values;
            keep_times.reserve(channel.times.size());
            keep_values.reserve(channel.values.size());
            for (std::size_t i = 0; i < channel.times.size(); ++i) {
                const float t = channel.times[i];
                if (t > t0 + k_eps && t < t1 - k_eps) continue;
                keep_times.push_back(t);
                const auto* src = channel.values.data() + i * static_cast<std::size_t>(components);
                keep_values.insert(keep_values.end(), src, src + components);
            }
            channel.times = std::move(keep_times);
            channel.values = std::move(keep_values);
        }

        // Pin segment endpoints so breakdowns have stable anchors.
        upsert_channel_key(channel, t0, start.data(), components);
        upsert_channel_key(channel, t1, end.data(), components);

        for (int i = 1; i <= count; ++i) {
            const float u = static_cast<float>(i) / static_cast<float>(count + 1);
            const float eased = evaluate_anim_ease(preset, u);
            const float time = t0 + (t1 - t0) * u;
            std::vector<float> values(static_cast<std::size_t>(components));
            if (channel.path == AnimationChannelPath::Rotation) {
                std::array<float, 4> qa{start[0], start[1], start[2], start[3]};
                std::array<float, 4> qb{end[0], end[1], end[2], end[3]};
                const auto q = slerp_quat(qa, qb, eased);
                for (int c = 0; c < 4; ++c) values[static_cast<std::size_t>(c)] = q[static_cast<std::size_t>(c)];
            } else {
                for (int c = 0; c < components; ++c) {
                    values[static_cast<std::size_t>(c)] =
                        start[static_cast<std::size_t>(c)]
                        + (end[static_cast<std::size_t>(c)] - start[static_cast<std::size_t>(c)]) * eased;
                }
            }
            upsert_channel_key(channel, time, values.data(), components);
            ++inserted;
        }
        channel.interpolation = AnimationInterpolationMode::Linear;
    }
    clip.duration_seconds = std::max(clip.duration_seconds, t1);
    return inserted;
}

int shift_clip_keys(AnimationClip& clip, float dt, float duration_seconds,
    const std::vector<std::string>& joint_filters, const std::string& path_filter) {
    if (!(std::abs(dt) > 1.0e-8f)) return 0;
    const float duration = std::max(0.01f, duration_seconds);
    int moved = 0;
    for (auto& channel : clip.channels) {
        if (!path_matches_filter(channel.path, path_filter)) continue;
        if (!joint_matches_filters(channel.target_node_name, false, joint_filters)
            && !joint_matches_filters(channel.target_node_name, true, joint_filters))
            continue;
        const int components = channel_components(channel.path);
        std::vector<float> new_times = channel.times;
        for (float& t : new_times) {
            t = std::clamp(t + dt, 0.0f, duration);
            ++moved;
        }
        // Re-sort by time while keeping values aligned (stable for collisions: last write wins via rebuild).
        std::vector<std::size_t> order(new_times.size());
        for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::stable_sort(order.begin(), order.end(),
            [&](std::size_t a, std::size_t b) { return new_times[a] < new_times[b]; });
        std::vector<float> sorted_times;
        std::vector<float> sorted_values;
        sorted_times.reserve(new_times.size());
        sorted_values.reserve(channel.values.size());
        const float eps = 1.0e-4f;
        for (std::size_t idx : order) {
            const float t = new_times[idx];
            if (!sorted_times.empty() && std::abs(sorted_times.back() - t) <= eps) {
                // Overwrite previous colliding key.
                const std::size_t base = (sorted_times.size() - 1) * static_cast<std::size_t>(components);
                for (int c = 0; c < components; ++c)
                    sorted_values[base + static_cast<std::size_t>(c)] =
                        channel.values[idx * static_cast<std::size_t>(components) + static_cast<std::size_t>(c)];
                sorted_times.back() = t;
                continue;
            }
            sorted_times.push_back(t);
            for (int c = 0; c < components; ++c)
                sorted_values.push_back(
                    channel.values[idx * static_cast<std::size_t>(components) + static_cast<std::size_t>(c)]);
        }
        channel.times = std::move(sorted_times);
        channel.values = std::move(sorted_values);
    }
    return moved;
}

static bool sample_channel_values(const AnimationClipChannel& channel, float time, std::vector<float>& out) {
    const int components = channel_components(channel.path);
    out.assign(static_cast<std::size_t>(components), 0.0f);
    if (channel.path == AnimationChannelPath::Rotation) {
        auto sampled = sample_rotation_channel(channel, time);
        if (!sampled) return false;
        for (int c = 0; c < 4; ++c) out[static_cast<std::size_t>(c)] = sampled.value()[static_cast<std::size_t>(c)];
        return true;
    }
    if (channel.path == AnimationChannelPath::Translation) {
        auto sampled = sample_translation_channel(channel, time);
        if (!sampled) return false;
        for (int c = 0; c < 3; ++c) out[static_cast<std::size_t>(c)] = sampled.value()[static_cast<std::size_t>(c)];
        return true;
    }
    auto sampled = sample_scale_channel(channel, time);
    if (!sampled) return false;
    for (int c = 0; c < 3; ++c) out[static_cast<std::size_t>(c)] = sampled.value()[static_cast<std::size_t>(c)];
    return true;
}

static AnimationClipChannel* find_or_add_channel(AnimationClip& clip, const std::string& name,
    AnimationChannelPath path) {
    for (auto& channel : clip.channels) {
        if (channel.target_node_name == name && channel.path == path) return &channel;
    }
    AnimationClipChannel added;
    added.target_node_name = name;
    added.path = path;
    added.interpolation = AnimationInterpolationMode::Linear;
    clip.channels.push_back(std::move(added));
    return &clip.channels.back();
}

int copy_clip_pose_at(AnimationClip& dest, const AnimationClip& source, float from_time,
    const std::vector<float>& to_times, const std::vector<std::string>& joint_filters,
    const std::string& path_filter) {
    if (to_times.empty()) return 0;
    struct SampledChannel {
        std::string name;
        AnimationChannelPath path;
        AnimationInterpolationMode interpolation = AnimationInterpolationMode::Linear;
        std::vector<float> values;
    };
    std::vector<SampledChannel> samples;
    samples.reserve(source.channels.size());
    for (const auto& channel : source.channels) {
        if (!path_matches_filter(channel.path, path_filter)) continue;
        if (!joint_matches_filters(channel.target_node_name, false, joint_filters)
            && !joint_matches_filters(channel.target_node_name, true, joint_filters))
            continue;
        SampledChannel sampled;
        sampled.name = channel.target_node_name;
        sampled.path = channel.path;
        sampled.interpolation = channel.interpolation;
        if (!sample_channel_values(channel, from_time, sampled.values)) continue;
        samples.push_back(std::move(sampled));
    }

    const bool same_clip = &dest == &source;
    int written = 0;
    for (const auto& sampled : samples) {
        AnimationClipChannel* dest_channel = nullptr;
        if (same_clip) {
            for (auto& channel : dest.channels) {
                if (channel.target_node_name == sampled.name && channel.path == sampled.path) {
                    dest_channel = &channel;
                    break;
                }
            }
        } else {
            dest_channel = find_or_add_channel(dest, sampled.name, sampled.path);
            if (dest_channel && dest_channel->times.empty())
                dest_channel->interpolation = sampled.interpolation;
        }
        if (!dest_channel) continue;
        const int components = static_cast<int>(sampled.values.size());
        if (components <= 0) continue;
        for (float time : to_times) {
            upsert_channel_key(*dest_channel, time, sampled.values.data(), components);
            dest.duration_seconds = std::max(dest.duration_seconds, time);
            ++written;
        }
    }
    return written;
}

nlohmann::json loop_report_json(const AnimationClip& clip, const ImportedSkin* skin_or_null,
    bool apply_sagittal_handedness, int sample_count) {
    sample_count = std::clamp(sample_count, 2, 64);
    nlohmann::json report;
    report["clip"] = clip.name;
    report["duration"] = clip.duration_seconds;
    const float t0 = 0.0f;
    const float t1 = std::max(0.0f, clip.duration_seconds);

    nlohmann::json pose_a;
    nlohmann::json pose_b;
    if (skin_or_null && !skin_or_null->joint_names.empty()) {
        pose_a = sample_skinned_pose_json(*skin_or_null, clip, t0, apply_sagittal_handedness, {}, true);
        pose_b = sample_skinned_pose_json(*skin_or_null, clip, t1, apply_sagittal_handedness, {}, true);
    } else {
        pose_a = sample_clip_channel_poses_json(clip, t0, {}, true);
        pose_b = sample_clip_channel_poses_json(clip, t1, {}, true);
    }
    report["seamDiff"] = diff_pose_json(pose_a, pose_b);

    nlohmann::json series = nlohmann::json::array();
    float hip_y_min = 1.0e9f, hip_y_max = -1.0e9f;
    float foot_y_min = 1.0e9f;
    for (int i = 0; i < sample_count; ++i) {
        const float u = sample_count == 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(sample_count - 1);
        const float t = t0 + (t1 - t0) * u;
        nlohmann::json sample;
        sample["time"] = t;
        nlohmann::json poses;
        if (skin_or_null && !skin_or_null->joint_names.empty()) {
            poses = sample_skinned_pose_json(*skin_or_null, clip, t, apply_sagittal_handedness, {}, false);
        } else {
            poses = sample_clip_channel_poses_json(clip, t, {}, false);
        }
        float hip_y = 0.0f;
        float lowest_foot = 1.0e9f;
        bool have_hip = false;
        for (const auto& p : poses) {
            if (!p.is_object() || !p.contains("world")) continue;
            const float y = p["world"].value("y", 0.0f);
            const std::string skin = p.value("skinName", std::string{});
            const std::string channel = p.value("channelName", std::string{});
            if (skin == "Hips" || skin == "Hip" || channel == "Hips" || channel == "Hip") {
                hip_y = y;
                have_hip = true;
            }
            if (skin.find("Foot") != std::string::npos || channel.find("Foot") != std::string::npos
                || skin.find("Toe") != std::string::npos) {
                lowest_foot = std::min(lowest_foot, y);
            }
        }
        if (have_hip) {
            hip_y_min = std::min(hip_y_min, hip_y);
            hip_y_max = std::max(hip_y_max, hip_y);
            sample["hipY"] = hip_y;
        }
        if (lowest_foot < 1.0e8f) {
            foot_y_min = std::min(foot_y_min, lowest_foot);
            sample["lowestFootY"] = lowest_foot;
        }
        series.push_back(std::move(sample));
    }
    report["samples"] = std::move(series);
    if (hip_y_min <= hip_y_max) {
        report["hipYMin"] = hip_y_min;
        report["hipYMax"] = hip_y_max;
        report["hipYRange"] = hip_y_max - hip_y_min;
    }
    if (foot_y_min < 1.0e8f) report["lowestFootY"] = foot_y_min;

    int flagged = 0;
    if (report["seamDiff"].is_array()) {
        for (const auto& d : report["seamDiff"]) {
            float mag = 0.0f;
            if (d.contains("deltaTranslation") && d["deltaTranslation"].is_array()) {
                for (const auto& v : d["deltaTranslation"])
                    if (v.is_number()) mag += std::abs(v.get<float>());
            }
            if (d.contains("deltaEulerDeg") && d["deltaEulerDeg"].is_array()) {
                for (const auto& v : d["deltaEulerDeg"])
                    if (v.is_number()) mag += std::abs(v.get<float>()) * 0.01f;
            }
            if (mag > 0.05f) ++flagged;
        }
    }
    report["seamFlagCount"] = flagged;
    return report;
}

nlohmann::json sample_world_series_json(const AnimationClip& clip, const ImportedSkin* skin_or_null,
    bool apply_sagittal_handedness, const std::vector<float>& times, const std::vector<std::string>& joint_filters,
    const std::string& grip_joint) {
    nlohmann::json series = nlohmann::json::array();
    for (float t : times) {
        nlohmann::json sample;
        sample["time"] = t;
        nlohmann::json poses;
        if (skin_or_null && !skin_or_null->joint_names.empty()) {
            poses = sample_skinned_pose_json(*skin_or_null, clip, t, apply_sagittal_handedness, joint_filters, true);
        } else {
            poses = sample_clip_channel_poses_json(clip, t, joint_filters, true);
        }
        sample["poses"] = poses;
        if (!grip_joint.empty() && poses.is_array()) {
            for (const auto& p : poses) {
                if (!p.is_object()) continue;
                const auto channel = p.value("channelName", std::string{});
                const auto skin = p.value("skinName", std::string{});
                if (channel != grip_joint && skin != grip_joint) continue;
                if (!p.contains("world")) continue;
                nlohmann::json grip;
                grip["joint"] = grip_joint;
                grip["channelName"] = channel;
                grip["skinName"] = skin;
                grip["world"] = p["world"];
                sample["grip"] = std::move(grip);
                break;
            }
        }
        series.push_back(std::move(sample));
    }
    return series;
}

std::array<float, 3> tip_local_from_mesh_bounds(const MeshBounds& bounds) noexcept {
    std::array<float, 3> best{0.0f, 0.7f, 0.0f};
    float best_len2 = -1.0f;
    const float xs[2] = {bounds.min_x, bounds.max_x};
    const float ys[2] = {bounds.min_y, bounds.max_y};
    const float zs[2] = {bounds.min_z, bounds.max_z};
    for (float x : xs) {
        for (float y : ys) {
            for (float z : zs) {
                const float len2 = x * x + y * y + z * z;
                if (len2 > best_len2) {
                    best_len2 = len2;
                    best = {x, y, z};
                }
            }
        }
    }
    return best;
}

namespace {

std::vector<JointLocalPose> sample_clip_locals_for_skin(const ImportedSkin& skin, const AnimationClip& clip,
    float time_seconds, bool apply_sagittal_handedness) {
    std::vector<JointLocalPose> locals(skin.joint_names.size());
    for (std::size_t j = 0; j < skin.joint_names.size(); ++j) {
        JointLocalPose rest{};
        if (j < skin.joint_rest_locals.size()) {
            rest.translation = skin.joint_rest_locals[j].translation;
            rest.rotation = skin.joint_rest_locals[j].rotation;
            rest.scale = skin.joint_rest_locals[j].scale;
        }
        const std::string& skin_name = skin.joint_names[j];
        JointLocalPose pose = apply_sagittal_handedness ? reflect_pose_across_x(rest) : rest;
        const std::string channel_name =
            apply_sagittal_handedness ? sagittal_joint_name(skin_name) : skin_name;
        for (const auto& channel : clip.channels) {
            if (channel.target_node_name != channel_name) continue;
            if (channel.path == AnimationChannelPath::Translation) {
                if (auto sampled = sample_translation_channel(channel, time_seconds))
                    pose.translation = sampled.value();
            } else if (channel.path == AnimationChannelPath::Rotation) {
                if (auto sampled = sample_rotation_channel(channel, time_seconds))
                    pose.rotation = sampled.value();
            } else if (channel.path == AnimationChannelPath::Scale) {
                if (auto sampled = sample_scale_channel(channel, time_seconds)) pose.scale = sampled.value();
            }
        }
        if (apply_sagittal_handedness) pose = reflect_pose_across_x(pose);
        locals[j] = pose;
    }
    return locals;
}

std::optional<std::size_t> resolve_weld_joint_index(const ImportedSkin& skin, const std::string& weld_joint) {
    if (weld_joint.empty()) return std::nullopt;
    if (auto idx = find_skin_joint_index(skin, weld_joint)) return idx;
    const std::string mirror = sagittal_joint_name(weld_joint);
    if (mirror != weld_joint) return find_skin_joint_index(skin, mirror);
    return std::nullopt;
}

void plot_line_rgba(std::vector<std::uint8_t>& rgba, std::uint32_t width, std::uint32_t height, int x0, int y0,
    int x1, int y1, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    if (rgba.size() < static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u) return;
    auto plot = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= static_cast<int>(width) || y >= static_cast<int>(height)) return;
        const std::size_t i = (static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)) * 4u;
        const float src_a = static_cast<float>(a) / 255.0f;
        rgba[i + 0] = static_cast<std::uint8_t>(
            rgba[i + 0] * (1.0f - src_a) + static_cast<float>(r) * src_a);
        rgba[i + 1] = static_cast<std::uint8_t>(
            rgba[i + 1] * (1.0f - src_a) + static_cast<float>(g) * src_a);
        rgba[i + 2] = static_cast<std::uint8_t>(
            rgba[i + 2] * (1.0f - src_a) + static_cast<float>(b) * src_a);
        rgba[i + 3] = 255;
    };
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        plot(x0, y0);
        plot(x0 + 1, y0);
        plot(x0, y0 + 1);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

} // namespace

nlohmann::json sample_held_tip_series_json(const AnimationClip& clip, const ImportedSkin& skin,
    bool apply_sagittal_handedness, const BoneWeld& weld, const TransformComponent& owner_world,
    const TransformComponent& visual_local, const std::array<float, 3>& tip_local_offset,
    const std::vector<float>& times) {
    nlohmann::json series = nlohmann::json::array();
    const auto joint_idx = resolve_weld_joint_index(skin, weld.joint);
    float path_length = 0.0f;
    std::optional<std::array<float, 3>> prev_tip;
    for (float t : times) {
        nlohmann::json sample;
        sample["time"] = t;
        sample["joint"] = weld.joint;
        if (!joint_idx) {
            sample["error"] = "weld joint not found on skin";
            series.push_back(std::move(sample));
            continue;
        }
        const auto locals = sample_clip_locals_for_skin(skin, clip, t, apply_sagittal_handedness);
        auto globals = build_joint_global_matrices(skin, locals);
        if (!globals || joint_idx.value() >= globals.value().size()) {
            sample["error"] = "failed to build joint globals";
            series.push_back(std::move(sample));
            continue;
        }
        BoneSocketChain chain;
        chain.owner_world = owner_world;
        chain.visual_local = visual_local;
        chain.joint_model = globals.value()[joint_idx.value()];
        const TransformComponent socket = bone_socket_world(chain);
        const TransformComponent held = weld_world_transform(socket, weld);
        TransformComponent tip_local{};
        tip_local.position = tip_local_offset;
        const TransformComponent tip = multiply_transforms(held, tip_local);
        const float dx = tip.position[0] - held.position[0];
        const float dy = tip.position[1] - held.position[1];
        const float dz = tip.position[2] - held.position[2];
        const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
        const float inv = len > 1.0e-6f ? (1.0f / len) : 0.0f;
        if (prev_tip) {
            const float pdx = tip.position[0] - (*prev_tip)[0];
            const float pdy = tip.position[1] - (*prev_tip)[1];
            const float pdz = tip.position[2] - (*prev_tip)[2];
            path_length += std::sqrt(pdx * pdx + pdy * pdy + pdz * pdz);
        }
        prev_tip = tip.position;
        sample["gripWorld"] = float_array_json(socket.position.data(), 3);
        sample["heldWorld"] = float_array_json(held.position.data(), 3);
        sample["tipWorld"] = float_array_json(tip.position.data(), 3);
        const float dir[3] = {dx * inv, dy * inv, dz * inv};
        sample["tipDir"] = float_array_json(dir, 3);
        sample["tipLocal"] = float_array_json(tip_local_offset.data(), 3);
        sample["pathLength"] = path_length;
        series.push_back(std::move(sample));
    }
    return series;
}

void stamp_tip_trail_rgba(std::vector<std::uint8_t>& rgba, std::uint32_t width, std::uint32_t height,
    const std::vector<std::array<float, 3>>& tip_worlds, const std::string& view) {
    if (tip_worlds.empty() || width < 8 || height < 8) return;
    const std::string mode = to_lower_copy(view);
    auto map_axes = [&](const std::array<float, 3>& p, float& u, float& v) {
        if (mode == "front") {
            u = p[0];
            v = p[1];
        } else if (mode == "top") {
            u = p[0];
            v = p[2];
        } else {
            // side: look along +X (character right) — Z horizontal, Y up
            u = p[2];
            v = p[1];
        }
    };
    float min_u = 1.0e9f, max_u = -1.0e9f, min_v = 1.0e9f, max_v = -1.0e9f;
    for (const auto& p : tip_worlds) {
        float u = 0.0f, v = 0.0f;
        map_axes(p, u, v);
        min_u = (std::min)(min_u, u);
        max_u = (std::max)(max_u, u);
        min_v = (std::min)(min_v, v);
        max_v = (std::max)(max_v, v);
    }
    const float pad_u = (std::max)(0.05f, (max_u - min_u) * 0.12f);
    const float pad_v = (std::max)(0.05f, (max_v - min_v) * 0.12f);
    min_u -= pad_u;
    max_u += pad_u;
    min_v -= pad_v;
    max_v += pad_v;
    const float span_u = (std::max)(1.0e-4f, max_u - min_u);
    const float span_v = (std::max)(1.0e-4f, max_v - min_v);
    const int margin = static_cast<int>((std::min)(width, height) / 12u);
    const int plot_w = static_cast<int>(width) - margin * 2;
    const int plot_h = static_cast<int>(height) - margin * 2;
    if (plot_w < 4 || plot_h < 4) return;
    auto to_px = [&](const std::array<float, 3>& p, int& x, int& y) {
        float u = 0.0f, v = 0.0f;
        map_axes(p, u, v);
        x = margin + static_cast<int>(((u - min_u) / span_u) * static_cast<float>(plot_w));
        y = margin + static_cast<int>((1.0f - (v - min_v) / span_v) * static_cast<float>(plot_h));
    };
    for (std::size_t i = 1; i < tip_worlds.size(); ++i) {
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        to_px(tip_worlds[i - 1], x0, y0);
        to_px(tip_worlds[i], x1, y1);
        const float t = static_cast<float>(i) / static_cast<float>(tip_worlds.size());
        const auto r = static_cast<std::uint8_t>(80 + 175 * t);
        const auto g = static_cast<std::uint8_t>(220 - 80 * t);
        const auto b = static_cast<std::uint8_t>(40 + 40 * t);
        plot_line_rgba(rgba, width, height, x0, y0, x1, y1, r, g, b, 220);
    }
    // Endpoint markers: start cyan, tip hot yellow.
    auto mark = [&](const std::array<float, 3>& p, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
        int x = 0, y = 0;
        to_px(p, x, y);
        for (int dy = -2; dy <= 2; ++dy)
            for (int dx = -2; dx <= 2; ++dx)
                plot_line_rgba(rgba, width, height, x + dx, y + dy, x + dx, y + dy, r, g, b, 255);
    };
    mark(tip_worlds.front(), 80, 200, 255);
    mark(tip_worlds.back(), 255, 230, 80);
}

std::string format_contact_slot_label(float time_seconds, const std::string& state_filter,
    const std::vector<std::pair<std::string, float>>& event_name_times, float tolerance_seconds) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "t=%.2f", static_cast<double>(time_seconds));
    std::string label = buf;
    std::vector<std::string> hits;
    for (const auto& [name, et] : event_name_times) {
        if (std::abs(et - time_seconds) <= tolerance_seconds) hits.push_back(name);
    }
    if (!hits.empty()) {
        label += " | ";
        for (std::size_t i = 0; i < hits.size(); ++i) {
            if (i) label += ",";
            label += hits[i];
        }
    }
    (void)state_filter;
    return label;
}

std::vector<std::uint8_t> composite_rgba_contact_sheet(const std::vector<std::vector<std::uint8_t>>& frames,
    std::uint32_t frame_w, std::uint32_t frame_h, std::uint32_t columns, std::uint32_t& out_w,
    std::uint32_t& out_h) {
    out_w = 0;
    out_h = 0;
    if (frames.empty() || frame_w == 0 || frame_h == 0) return {};
    columns = std::max(1u, columns);
    const std::uint32_t rows = (static_cast<std::uint32_t>(frames.size()) + columns - 1u) / columns;
    out_w = columns * frame_w;
    out_h = rows * frame_h;
    std::vector<std::uint8_t> sheet(static_cast<std::size_t>(out_w) * static_cast<std::size_t>(out_h) * 4u, 0);
    const std::size_t frame_bytes =
        static_cast<std::size_t>(frame_w) * static_cast<std::size_t>(frame_h) * 4u;
    for (std::size_t i = 0; i < frames.size(); ++i) {
        if (frames[i].size() < frame_bytes) continue;
        const std::uint32_t col = static_cast<std::uint32_t>(i) % columns;
        const std::uint32_t row = static_cast<std::uint32_t>(i) / columns;
        for (std::uint32_t y = 0; y < frame_h; ++y) {
            const std::uint8_t* src =
                frames[i].data() + static_cast<std::size_t>(y) * frame_w * 4u;
            std::uint8_t* dst = sheet.data()
                + (static_cast<std::size_t>(row * frame_h + y) * out_w + col * frame_w) * 4u;
            std::memcpy(dst, src, static_cast<std::size_t>(frame_w) * 4u);
        }
    }
    return sheet;
}

} // namespace engine
