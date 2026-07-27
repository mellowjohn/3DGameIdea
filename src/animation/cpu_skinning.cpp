#include "engine/animation/cpu_skinning.h"

#include <algorithm>
#include <cmath>

namespace engine {
namespace {

EngineError skin_error(std::string code, std::string message,
    std::string remedy = "Export a skinned glTF with rest TRS, JOINTS_0/WEIGHTS_0, and inverse-bind matrices.") {
    return {std::move(code), Severity::Error, ErrorCategory::AssetImport, "cpu-skinning", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

std::array<float, 4> normalize_quat(std::array<float, 4> q) {
    const float len = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (!(len > 1e-8f)) return {0.0f, 0.0f, 0.0f, 1.0f};
    const float inv = 1.0f / len;
    return {q[0] * inv, q[1] * inv, q[2] * inv, q[3] * inv};
}

std::array<float, 4> slerp_quat(std::array<float, 4> a, std::array<float, 4> b, float t) {
    a = normalize_quat(a);
    b = normalize_quat(b);
    float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    if (dot < 0.0f) {
        b = {-b[0], -b[1], -b[2], -b[3]};
        dot = -dot;
    }
    if (dot > 0.9995f) {
        return normalize_quat({
            a[0] + (b[0] - a[0]) * t,
            a[1] + (b[1] - a[1]) * t,
            a[2] + (b[2] - a[2]) * t,
            a[3] + (b[3] - a[3]) * t,
        });
    }
    const float theta = std::acos(std::clamp(dot, -1.0f, 1.0f));
    const float sin_theta = std::sin(theta);
    const float w1 = std::sin((1.0f - t) * theta) / sin_theta;
    const float w2 = std::sin(t * theta) / sin_theta;
    return normalize_quat({
        a[0] * w1 + b[0] * w2,
        a[1] * w1 + b[1] * w2,
        a[2] * w1 + b[2] * w2,
        a[3] * w1 + b[3] * w2,
    });
}

std::array<float, 16> trs_to_matrix(const JointLocalPose& pose) {
    const auto q = normalize_quat(pose.rotation);
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    const float x2 = x + x, y2 = y + y, z2 = z + z;
    const float xx = x * x2, xy = x * y2, xz = x * z2;
    const float yy = y * y2, yz = y * z2, zz = z * z2;
    const float wx = w * x2, wy = w * y2, wz = w * z2;
    const float sx = pose.scale[0], sy = pose.scale[1], sz = pose.scale[2];
    // Column-major 4x4
    return {
        (1.0f - (yy + zz)) * sx, (xy + wz) * sx, (xz - wy) * sx, 0.0f,
        (xy - wz) * sy, (1.0f - (xx + zz)) * sy, (yz + wx) * sy, 0.0f,
        (xz + wy) * sz, (yz - wx) * sz, (1.0f - (xx + yy)) * sz, 0.0f,
        pose.translation[0], pose.translation[1], pose.translation[2], 1.0f,
    };
}

std::array<float, 16> mul_mat4(const std::array<float, 16>& a, const std::array<float, 16>& b) {
    std::array<float, 16> out{};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            out[col * 4 + row] = a[0 * 4 + row] * b[col * 4 + 0] + a[1 * 4 + row] * b[col * 4 + 1]
                + a[2 * 4 + row] * b[col * 4 + 2] + a[3 * 4 + row] * b[col * 4 + 3];
        }
    }
    return out;
}

void transform_point(const std::array<float, 16>& m, float x, float y, float z, float& ox, float& oy, float& oz) {
    ox = m[0] * x + m[4] * y + m[8] * z + m[12];
    oy = m[1] * x + m[5] * y + m[9] * z + m[13];
    oz = m[2] * x + m[6] * y + m[10] * z + m[14];
}

JointLocalPose rest_pose_for(const ImportedSkin& skin, std::size_t joint_index) {
    JointLocalPose pose;
    if (joint_index < skin.joint_rest_locals.size()) {
        const auto& rest = skin.joint_rest_locals[joint_index];
        pose.translation = rest.translation;
        pose.rotation = rest.rotation;
        pose.scale = rest.scale;
    }
    return pose;
}

Result<const AnimationClip*> find_clip(const AnimationClipLibrary& library,
    const std::filesystem::path& project_root, const AnimatorClipWeight& weight) {
    if (weight.clip_source.empty() || weight.clip.empty() || weight.weight <= 0.0f) {
        return Result<const AnimationClip*>::failure(
            skin_error("SKIN-CLIP-EMPTY", "Animator clip weight is missing source/name"));
    }
    std::filesystem::path path = weight.clip_source;
    if (path.is_relative() && !project_root.empty()) path = project_root / path;
    // Prefer cached get() — load() re-parses the glTF every call and dominated play-test "Skin matrices".
    Result<const ImportedAnimationSet*> loaded = library.get(path);
    if (!loaded) {
        loaded = const_cast<AnimationClipLibrary&>(library).load(path);
        if (!loaded) return Result<const AnimationClip*>::failure(loaded.error());
    }
    for (const auto& clip : loaded.value()->clips) {
        if (clip.name == weight.clip) return Result<const AnimationClip*>::success(&clip);
    }
    return Result<const AnimationClip*>::failure(skin_error("SKIN-CLIP-MISSING",
        "Clip '" + weight.clip + "' not found in " + weight.clip_source,
        "Match animator clip names to glTF animations[].name."));
}

Result<JointLocalPose> sample_clip_pose_for_joint(const AnimationClip& clip, float time_seconds,
    const std::string& joint_name, const JointLocalPose& rest) {
    JointLocalPose pose = rest;
    if (joint_name.empty()) return Result<JointLocalPose>::success(pose);
    for (const auto& channel : clip.channels) {
        // Name-only: node indices are local to each glTF and must not retarget across clip sources
        // (player_clips node 0 is Hip; player.gltf node 0 is Head).
        if (channel.target_node_name != joint_name) continue;
        if (channel.path == AnimationChannelPath::Translation) {
            auto sampled = sample_translation_channel(channel, time_seconds);
            if (!sampled) return Result<JointLocalPose>::failure(sampled.error());
            pose.translation = sampled.value();
        } else if (channel.path == AnimationChannelPath::Rotation) {
            auto sampled = sample_rotation_channel(channel, time_seconds);
            if (!sampled) return Result<JointLocalPose>::failure(sampled.error());
            pose.rotation = sampled.value();
        } else if (channel.path == AnimationChannelPath::Scale) {
            auto sampled = sample_scale_channel(channel, time_seconds);
            if (!sampled) return Result<JointLocalPose>::failure(sampled.error());
            pose.scale = sampled.value();
        }
    }
    return Result<JointLocalPose>::success(pose);
}

} // namespace

Result<std::vector<JointLocalPose>> sample_skinned_local_poses(const ImportedSkin& skin,
    const AnimationClipLibrary& library, const std::filesystem::path& project_root,
    const std::vector<AnimatorClipWeight>& clips) {
    if (skin.joint_node_indices.empty()) {
        return Result<std::vector<JointLocalPose>>::failure(
            skin_error("SKIN-EMPTY", "Cannot sample poses for an empty skin"));
    }
    if (skin.joint_rest_locals.size() != skin.joint_node_indices.size()) {
        return Result<std::vector<JointLocalPose>>::failure(skin_error("SKIN-REST-MISSING",
            "Imported skin is missing rest-pose locals",
            "Re-import the skinned mesh so joint_rest_locals are populated."));
    }

    std::vector<AnimatorClipWeight> active;
    active.reserve(clips.size());
    float total_weight = 0.0f;
    for (const auto& clip : clips) {
        if (clip.weight <= 0.0f || clip.clip.empty()) continue;
        active.push_back(clip);
        total_weight += clip.weight;
    }

    const std::size_t joint_count = skin.joint_node_indices.size();
    std::vector<JointLocalPose> out(joint_count);
    for (std::size_t j = 0; j < joint_count; ++j) out[j] = rest_pose_for(skin, j);

    if (active.empty() || !(total_weight > 0.0f)) {
        return Result<std::vector<JointLocalPose>>::success(std::move(out));
    }

    if (active.size() == 1) {
        auto clip = find_clip(library, project_root, active[0]);
        if (!clip) return Result<std::vector<JointLocalPose>>::failure(clip.error());
        for (std::size_t j = 0; j < joint_count; ++j) {
            auto sampled = sample_clip_pose_for_joint(*clip.value(), active[0].time_seconds,
                skin.joint_names[j], out[j]);
            if (!sampled) return Result<std::vector<JointLocalPose>>::failure(sampled.error());
            out[j] = sampled.value();
        }
        return Result<std::vector<JointLocalPose>>::success(std::move(out));
    }

    std::vector<std::vector<JointLocalPose>> per_clip(active.size());
    for (std::size_t i = 0; i < active.size(); ++i) {
        auto clip = find_clip(library, project_root, active[i]);
        if (!clip) return Result<std::vector<JointLocalPose>>::failure(clip.error());
        per_clip[i].resize(joint_count);
        for (std::size_t j = 0; j < joint_count; ++j) {
            auto sampled = sample_clip_pose_for_joint(*clip.value(), active[i].time_seconds,
                skin.joint_names[j], rest_pose_for(skin, j));
            if (!sampled) return Result<std::vector<JointLocalPose>>::failure(sampled.error());
            per_clip[i][j] = sampled.value();
        }
    }

    for (std::size_t j = 0; j < joint_count; ++j) {
        JointLocalPose blended{};
        blended.translation = {0, 0, 0};
        blended.scale = {0, 0, 0};
        blended.rotation = per_clip[0][j].rotation;
        float rot_weight = 0.0f;
        for (std::size_t i = 0; i < active.size(); ++i) {
            const float w = active[i].weight / total_weight;
            const auto& pose = per_clip[i][j];
            blended.translation[0] += pose.translation[0] * w;
            blended.translation[1] += pose.translation[1] * w;
            blended.translation[2] += pose.translation[2] * w;
            blended.scale[0] += pose.scale[0] * w;
            blended.scale[1] += pose.scale[1] * w;
            blended.scale[2] += pose.scale[2] * w;
            if (i == 0) {
                blended.rotation = pose.rotation;
                rot_weight = w;
            } else {
                const float t = (rot_weight + w) > 0.0f ? (w / (rot_weight + w)) : 0.0f;
                blended.rotation = slerp_quat(blended.rotation, pose.rotation, t);
                rot_weight += w;
            }
        }
        out[j] = blended;
    }
    return Result<std::vector<JointLocalPose>>::success(std::move(out));
}

Result<std::vector<std::array<float, 16>>> build_skin_matrices(const ImportedSkin& skin,
    const std::vector<JointLocalPose>& locals) {
    if (locals.size() != skin.joint_node_indices.size()
        || skin.inverse_bind_matrices.size() != skin.joint_node_indices.size()
        || skin.joint_rest_locals.size() != skin.joint_node_indices.size()) {
        return Result<std::vector<std::array<float, 16>>>::failure(
            skin_error("SKIN-PARALLEL", "Skin joint arrays must be parallel for matrix build"));
    }
    const std::size_t joint_count = locals.size();
    std::vector<std::array<float, 16>> global(joint_count);
    std::vector<std::uint8_t> computed(joint_count, 0);

    auto compute = [&](auto&& self, std::size_t joint) -> Result<void> {
        if (computed[joint]) return Result<void>::success();
        const auto local = trs_to_matrix(locals[joint]);
        const std::int32_t parent = skin.joint_rest_locals[joint].parent_joint;
        if (parent < 0) {
            global[joint] = local;
        } else {
            if (static_cast<std::size_t>(parent) >= joint_count) {
                return Result<void>::failure(skin_error("SKIN-PARENT-RANGE", "Joint parent index out of range"));
            }
            auto parent_ok = self(self, static_cast<std::size_t>(parent));
            if (!parent_ok) return parent_ok;
            global[joint] = mul_mat4(global[static_cast<std::size_t>(parent)], local);
        }
        computed[joint] = 1;
        return Result<void>::success();
    };

    for (std::size_t j = 0; j < joint_count; ++j) {
        auto ok = compute(compute, j);
        if (!ok) return Result<std::vector<std::array<float, 16>>>::failure(ok.error());
    }

    std::vector<std::array<float, 16>> skins(joint_count);
    for (std::size_t j = 0; j < joint_count; ++j) {
        skins[j] = mul_mat4(global[j], skin.inverse_bind_matrices[j]);
    }
    return Result<std::vector<std::array<float, 16>>>::success(std::move(skins));
}

Result<void> cpu_skin_positions(const ImportedMesh& mesh, std::size_t skin_index,
    const std::vector<std::array<float, 16>>& skin_matrices, std::vector<std::array<float, 3>>& out_positions) {
    if (skin_index >= mesh.skins.size()) {
        return Result<void>::failure(skin_error("SKIN-INDEX", "skin_index out of range"));
    }
    if (mesh.influences.size() != mesh.vertices.size()) {
        return Result<void>::failure(skin_error("SKIN-INFLUENCE-COUNT",
            "Mesh influences must match vertex count for CPU skinning"));
    }
    const auto& skin = mesh.skins[skin_index];
    if (skin_matrices.size() != skin.joint_node_indices.size()) {
        return Result<void>::failure(skin_error("SKIN-MATRIX-COUNT",
            "Skin matrix count must match joint count"));
    }
    out_positions.resize(mesh.vertices.size());
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
        const auto& v = mesh.vertices[i];
        const auto& inf = mesh.influences[i];
        float ox = 0.0f, oy = 0.0f, oz = 0.0f;
        float wsum = 0.0f;
        for (int k = 0; k < 4; ++k) {
            const float w = inf.weights[k];
            if (!(w > 0.0f)) continue;
            const auto joint = inf.joints[k];
            if (joint >= skin_matrices.size()) {
                return Result<void>::failure(skin_error("SKIN-JOINT-RANGE",
                    "Vertex influence joint index out of range"));
            }
            float px = 0.0f, py = 0.0f, pz = 0.0f;
            transform_point(skin_matrices[joint], v.x, v.y, v.z, px, py, pz);
            ox += px * w;
            oy += py * w;
            oz += pz * w;
            wsum += w;
        }
        if (wsum > 1e-6f) {
            out_positions[i] = {ox / wsum, oy / wsum, oz / wsum};
        } else {
            out_positions[i] = {v.x, v.y, v.z};
        }
    }
    return Result<void>::success();
}

} // namespace engine
