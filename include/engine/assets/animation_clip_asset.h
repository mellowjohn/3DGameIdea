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
 * Validate-then-cache library with write-time hot reload for previously loaded sources.
 * Failed reloads keep the previous clip set and return a structured error.
 */
class AnimationClipLibrary final {
public:
    [[nodiscard]] Result<const ImportedAnimationSet*> load(const std::filesystem::path& path);
    [[nodiscard]] Result<const ImportedAnimationSet*> get(const std::filesystem::path& path) const;
    [[nodiscard]] Result<const ImportedAnimationSet*> reload(const std::filesystem::path& path);
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

    std::map<std::filesystem::path, Entry> entries_;
};

/** CPU sample of a translation channel at time `t` (seconds). Linear lerp / step hold. */
[[nodiscard]] Result<std::array<float, 3>> sample_translation_channel(
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
