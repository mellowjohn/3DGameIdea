#pragma once

#include "engine/assets/animation_clip_asset.h"
#include "engine/animation/bone_attachment.h"
#include "engine/animation/cpu_skinning.h"
#include "engine/assets/mesh_asset.h"
#include "engine/world/components.h"

#include <nlohmann/json.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace engine {

/** Local XYZ euler degrees from quaternion xyzw (matches Studio upsert eulerDeg convention). */
[[nodiscard]] std::array<float, 3> quat_xyzw_to_euler_deg(const std::array<float, 4>& q) noexcept;

/** Quaternion xyzw from local XYZ euler degrees. */
[[nodiscard]] std::array<float, 4> euler_deg_to_quat_xyzw(const std::array<float, 3>& euler_deg) noexcept;

/** Ease curves on [0,1] → [0,1]. */
enum class AnimEasePreset : std::uint8_t {
    Linear = 0,
    EaseIn = 1,
    EaseOut = 2,
    EaseInOut = 3,
};

[[nodiscard]] AnimEasePreset parse_anim_ease_preset(const std::string& name) noexcept;
[[nodiscard]] float evaluate_anim_ease(AnimEasePreset preset, float t01) noexcept;

/**
 * Dump open-clip channels as JSON array entries.
 * Filters: joint names match channelName OR skinName (sagittal counterpart); empty = all.
 * path_filter empty = all paths; else translation|rotation|scale.
 */
[[nodiscard]] nlohmann::json list_clip_keys_json(const AnimationClip& clip, bool apply_sagittal_names,
    const std::vector<std::string>& joint_filters, const std::string& path_filter);

/**
 * Sample channel-local TRS from a clip at `time` for matching joints (channel names).
 * Does not apply sagittal skin reflection — returns authored channel pose.
 */
[[nodiscard]] nlohmann::json sample_clip_channel_poses_json(const AnimationClip& clip, float time_seconds,
    const std::vector<std::string>& joint_filters, bool include_euler);

/**
 * Sample skin-local + model-space world positions for a skinned subject.
 * When apply_sagittal_handedness is true, mirrors player RH→LH like runtime.
 */
[[nodiscard]] nlohmann::json sample_skinned_pose_json(const ImportedSkin& skin, const AnimationClip& clip,
    float time_seconds, bool apply_sagittal_handedness, const std::vector<std::string>& joint_filters,
    bool include_euler);

/** Diff two pose JSON arrays keyed by skinName/channelName (local + world). */
[[nodiscard]] nlohmann::json diff_pose_json(const nlohmann::json& pose_a, const nlohmann::json& pose_b);

/**
 * Insert N LINEAR breakdown keys between t0 and t1 on channels matching joints/path.
 * Clears keys strictly inside (t0, t1) first so eased samples are not zigzagged against
 * denser authored keys (ease samples endpoints only — intermediate conflict caused choppiness).
 * Returns number of keys inserted.
 */
[[nodiscard]] int insert_ease_breakdowns(AnimationClip& clip, float t0, float t1, int count,
    AnimEasePreset preset, const std::vector<std::string>& joint_filters, const std::string& path_filter);

/** Time-shift keys on matching channels by dt; clamp times to [0, duration]. Returns keys moved. */
[[nodiscard]] int shift_clip_keys(AnimationClip& clip, float dt, float duration_seconds,
    const std::vector<std::string>& joint_filters, const std::string& path_filter);

/**
 * Copy authored channel values from `from_time` onto each `to_times` key.
 * Only existing source channels are copied — never invents bind/default
 * translation or scale tracks (those collapse the skeleton on sagittal subjects).
 * When `&source == &dest`, dest channels are updated in place. When source is
 * another clip, matching dest channels are created as needed.
 * Returns number of keys written (channels × times).
 */
[[nodiscard]] int copy_clip_pose_at(AnimationClip& dest, const AnimationClip& source, float from_time,
    const std::vector<float>& to_times, const std::vector<std::string>& joint_filters,
    const std::string& path_filter);

/** First-vs-last sampled pose report for loop seams + hip/foot Y series. */
[[nodiscard]] nlohmann::json loop_report_json(const AnimationClip& clip, const ImportedSkin* skin_or_null,
    bool apply_sagittal_handedness, int sample_count);

/** Composite equal-size RGBA frames into a contact sheet (row-major left→right, wrap cols). */
[[nodiscard]] std::vector<std::uint8_t> composite_rgba_contact_sheet(
    const std::vector<std::vector<std::uint8_t>>& frames, std::uint32_t frame_w, std::uint32_t frame_h,
    std::uint32_t columns, std::uint32_t& out_w, std::uint32_t& out_h);

/**
 * Sample joint world (and local) poses at each time. Optional grip joint adds a "grip" entry from that
 * skin/channel world position (held tip approximation without mesh AABB).
 */
[[nodiscard]] nlohmann::json sample_world_series_json(const AnimationClip& clip, const ImportedSkin* skin_or_null,
    bool apply_sagittal_handedness, const std::vector<float>& times, const std::vector<std::string>& joint_filters,
    const std::string& grip_joint);

/**
 * Weld-aware held tip / grip world series (TICKET-0267).
 * tip_local_offset is in held-mesh local space after the weld (override with tipLocal MCP arg).
 */
[[nodiscard]] nlohmann::json sample_held_tip_series_json(const AnimationClip& clip, const ImportedSkin& skin,
    bool apply_sagittal_handedness, const BoneWeld& weld, const TransformComponent& owner_world,
    const TransformComponent& visual_local, const std::array<float, 3>& tip_local_offset,
    const std::vector<float>& times);

/** Farthest AABB corner from origin — default tip when mesh bounds are known. */
[[nodiscard]] std::array<float, 3> tip_local_from_mesh_bounds(const MeshBounds& bounds) noexcept;

/**
 * Stamp a cumulative tip arc onto an RGBA frame using an orthographic side/front/top map
 * (no view-projection required). view: side|front|top.
 */
void stamp_tip_trail_rgba(std::vector<std::uint8_t>& rgba, std::uint32_t width, std::uint32_t height,
    const std::vector<std::array<float, 3>>& tip_worlds, const std::string& view);

/** Build a short contact-sheet slot label: "t=0.52 | hitFrame" for events near `time` on `state`. */
[[nodiscard]] std::string format_contact_slot_label(float time_seconds, const std::string& state_filter,
    const std::vector<std::pair<std::string, float>>& event_name_times, float tolerance_seconds = 0.06f);

} // namespace engine
