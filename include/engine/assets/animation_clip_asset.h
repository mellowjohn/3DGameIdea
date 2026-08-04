#pragma once

#include "engine/core/result.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace engine {

enum class AnimationChannelPath : std::uint8_t {
    Translation = 1,
    Rotation = 2,
    Scale = 3,
};

enum class AnimationInterpolationMode : std::uint8_t {
    Linear = 0,
    Step = 1,
};

/** One TRS channel targeting a glTF node (joint). */
struct AnimationClipChannel {
    std::uint32_t target_node_index = 0;
    std::string target_node_name;
    AnimationChannelPath path = AnimationChannelPath::Translation;
    AnimationInterpolationMode interpolation = AnimationInterpolationMode::Linear;
    std::vector<float> times;
    /** Tightly packed key values: 3 floats (translation/scale) or 4 (rotation xyzw). */
    std::vector<float> values;
};

struct AnimationClip {
    std::string name;
    float duration_seconds = 0.0f;
    std::vector<AnimationClipChannel> channels;

    [[nodiscard]] Result<void> validate() const;
};

/** All clips imported from one glTF/GLB source asset. */
struct ImportedAnimationSet {
    std::filesystem::path source_path;
    std::vector<AnimationClip> clips;

    [[nodiscard]] Result<void> validate() const;
    [[nodiscard]] bool empty() const noexcept { return clips.empty(); }
};

/** Import glTF `animations[]` into engine-owned clips. Static meshes without animations succeed with an empty set. */
[[nodiscard]] Result<ImportedAnimationSet> import_gltf_animation_clips(const std::filesystem::path& path);

/**
 * Engine-owned clip override (DEC-0052 / TICKET-0253). Sidecar next to the glTF:
 * `hero_clips.gltf` + clip `Idle` → `hero_clips.Idle.anim.json`.
 * Runtime sampling prefers these channels when the sidecar exists.
 */
struct AnimationClipOverrideAsset {
    int schema_version = 1;
    std::string clip_source; // project-relative glTF/GLB path (informational; disk path is authoritative)
    std::string clip_name;
    float duration_seconds = 0.0f;
    std::vector<AnimationClipChannel> channels;

    [[nodiscard]] Result<void> validate() const;
    [[nodiscard]] AnimationClip to_clip() const;
    [[nodiscard]] static Result<AnimationClipOverrideAsset> load(const std::filesystem::path& path);
    [[nodiscard]] static Result<AnimationClipOverrideAsset> parse(const std::string& text,
        const std::string& source_name = "clip.anim.json");
    [[nodiscard]] static AnimationClipOverrideAsset from_clip(const std::string& clip_source,
        const AnimationClip& clip);
    [[nodiscard]] Result<void> save_atomic(const std::filesystem::path& path) const;
    [[nodiscard]] std::string to_json() const;
};

[[nodiscard]] const char* to_string(AnimationChannelPath value) noexcept;
[[nodiscard]] const char* to_string(AnimationInterpolationMode value) noexcept;

/** Sidecar path for `(gltf absolute/relative path, clipName)`. */
[[nodiscard]] std::filesystem::path animation_clip_override_path(const std::filesystem::path& clip_source_path,
    const std::string& clip_name);

/** Replace (or insert) the named clip in `set` with override channels. */
[[nodiscard]] Result<void> apply_animation_clip_override(ImportedAnimationSet& set,
    const AnimationClipOverrideAsset& override_asset);

/**
 * Write override TRS channels back into a `.gltf` (JSON) source. Fail-closed for `.glb`,
 * missing animations/nodes, and non LINEAR/STEP samplers.
 */
[[nodiscard]] Result<void> sync_animation_clip_override_to_gltf(const std::filesystem::path& gltf_path,
    const AnimationClipOverrideAsset& override_asset);

/**
 * Validate-then-cache library with write-time hot reload for previously loaded sources.
 * Failed reloads keep the previous clip set and return a structured error.
 * After each successful glTF import, matching `*.anim.json` sidecars are merged (override wins).
 */
class AnimationClipLibrary final {
public:
    [[nodiscard]] Result<const ImportedAnimationSet*> load(const std::filesystem::path& path);
    [[nodiscard]] Result<const ImportedAnimationSet*> get(const std::filesystem::path& path) const;
    [[nodiscard]] Result<const ImportedAnimationSet*> reload(const std::filesystem::path& path);
    /** Replace one named clip in a loaded set (studio live preview before Save). */
    [[nodiscard]] Result<void> replace_clip(const std::filesystem::path& path, AnimationClip clip);
    /** Among loaded paths, return those whose write time changed or that were removed. */
    [[nodiscard]] std::vector<std::filesystem::path> poll_changed();
    /** Reload every changed loaded path; returns how many succeeded. Failures leave prior data. */
    [[nodiscard]] Result<std::size_t> reload_changed();
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    void clear() { entries_.clear(); }

private:
    struct Entry {
        std::filesystem::path path;
        std::filesystem::file_time_type write_time{};
        ImportedAnimationSet set;
        bool has_write_time = false;
    };

    [[nodiscard]] Result<void> merge_disk_overrides(ImportedAnimationSet& set,
        const std::filesystem::path& gltf_path) const;

    std::map<std::filesystem::path, Entry> entries_;
};

/** CPU sample of a translation channel at time `t` (seconds). Linear lerp / step hold. */
[[nodiscard]] Result<std::array<float, 3>> sample_translation_channel(
    const AnimationClipChannel& channel, float time_seconds);

/** CPU sample of a rotation channel (xyzw quaternion). Linear slerp / step hold. */
[[nodiscard]] Result<std::array<float, 4>> sample_rotation_channel(
    const AnimationClipChannel& channel, float time_seconds);

/** CPU sample of a scale channel at time `t` (seconds). Linear lerp / step hold. */
[[nodiscard]] Result<std::array<float, 3>> sample_scale_channel(
    const AnimationClipChannel& channel, float time_seconds);

struct RootMotionDelta {
    std::array<float, 3> translation{0.0f, 0.0f, 0.0f}; // clip-space meters
    bool found_root_channel = false;
};

/**
 * Translation delta of the root joint from time `from_seconds` to `to_seconds`.
 * Handles looping wrap when `loop` is true and `to` wraps past duration.
 * Joint match: exact `root_joint_name`, else first channel targeting "Root" / "Hip".
 */
[[nodiscard]] Result<RootMotionDelta> extract_clip_root_motion_delta(const AnimationClip& clip,
    const std::string& root_joint_name, float from_seconds, float to_seconds, bool loop);

} // namespace engine
