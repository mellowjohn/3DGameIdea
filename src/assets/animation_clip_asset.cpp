#include "engine/assets/animation_clip_asset.h"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace engine {
namespace {

EngineError anim_error(std::string code, std::string message,
    std::string remedy = "Export glTF animations with LINEAR/STEP TRS channels and finite FLOAT accessors.") {
    return {std::move(code), Severity::Error, ErrorCategory::AssetImport, "animation-clip-import",
        std::move(message), ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

std::filesystem::path canonical_key(const std::filesystem::path& path) {
    std::error_code ec;
    const auto absolute = std::filesystem::weakly_canonical(path, ec);
    if (!ec) return absolute.lexically_normal();
    return path.lexically_normal();
}

Result<std::vector<float>> read_float_scalars(const fastgltf::Asset& asset, std::size_t accessor_index,
    const char* label) {
    if (accessor_index >= asset.accessors.size()) {
        return Result<std::vector<float>>::failure(anim_error("ANIM-CLIP-ACCESSOR-MISSING",
            std::string(label) + " accessor is missing"));
    }
    const auto& accessor = asset.accessors[accessor_index];
    if (accessor.type != fastgltf::AccessorType::Scalar || accessor.componentType != fastgltf::ComponentType::Float) {
        return Result<std::vector<float>>::failure(anim_error("ANIM-CLIP-TIME-TYPE",
            std::string(label) + " must be FLOAT SCALAR",
            "Export animation sampler input as float times."));
    }
    if (accessor.count == 0) {
        return Result<std::vector<float>>::failure(anim_error("ANIM-CLIP-EMPTY-KEYS",
            std::string(label) + " has no keyframes"));
    }
    std::vector<float> values;
    values.reserve(accessor.count);
    bool nonfinite = false;
    bool unsorted = false;
    float previous = -std::numeric_limits<float>::infinity();
    fastgltf::iterateAccessor<float>(asset, accessor, [&](float value) {
        if (!std::isfinite(value)) nonfinite = true;
        if (value < previous) unsorted = true;
        previous = value;
        values.push_back(value);
    });
    if (nonfinite) {
        return Result<std::vector<float>>::failure(anim_error("ANIM-CLIP-NONFINITE",
            std::string(label) + " contains NaN or infinity"));
    }
    if (unsorted) {
        return Result<std::vector<float>>::failure(anim_error("ANIM-CLIP-TIME-ORDER",
            std::string(label) + " key times must be non-decreasing"));
    }
    return Result<std::vector<float>>::success(std::move(values));
}

Result<std::vector<float>> read_float_vectors(const fastgltf::Asset& asset, std::size_t accessor_index,
    fastgltf::AccessorType expected_type, std::size_t components, std::size_t expected_count, const char* label) {
    if (accessor_index >= asset.accessors.size()) {
        return Result<std::vector<float>>::failure(anim_error("ANIM-CLIP-ACCESSOR-MISSING",
            std::string(label) + " accessor is missing"));
    }
    const auto& accessor = asset.accessors[accessor_index];
    if (accessor.type != expected_type || accessor.componentType != fastgltf::ComponentType::Float) {
        return Result<std::vector<float>>::failure(anim_error("ANIM-CLIP-VALUE-TYPE",
            std::string(label) + " must be FLOAT with the expected vector type",
            "Export TRS outputs as FLOAT VEC3 (translation/scale) or VEC4 (rotation)."));
    }
    if (accessor.count != expected_count) {
        return Result<std::vector<float>>::failure(anim_error("ANIM-CLIP-VALUE-COUNT",
            std::string(label) + " count must match sampler input key count"));
    }
    std::vector<float> values;
    values.reserve(accessor.count * components);
    bool nonfinite = false;
    if (components == 3) {
        fastgltf::iterateAccessor<fastgltf::math::fvec3>(asset, accessor, [&](const auto& value) {
            for (std::size_t i = 0; i < 3; ++i) {
                if (!std::isfinite(value[i])) nonfinite = true;
                values.push_back(value[i]);
            }
        });
    } else {
        fastgltf::iterateAccessor<fastgltf::math::fvec4>(asset, accessor, [&](const auto& value) {
            for (std::size_t i = 0; i < 4; ++i) {
                if (!std::isfinite(value[i])) nonfinite = true;
                values.push_back(value[i]);
            }
        });
    }
    if (nonfinite) {
        return Result<std::vector<float>>::failure(anim_error("ANIM-CLIP-NONFINITE",
            std::string(label) + " contains NaN or infinity"));
    }
    return Result<std::vector<float>>::success(std::move(values));
}

Result<AnimationClipChannel> import_channel(const fastgltf::Asset& asset, const fastgltf::Animation& animation,
    const fastgltf::AnimationChannel& channel) {
    if (!channel.nodeIndex) {
        return Result<AnimationClipChannel>::failure(anim_error("ANIM-CLIP-TARGET-MISSING",
            "Animation channel is missing a target node",
            "Bind each channel target.node to a valid node index."));
    }
    if (*channel.nodeIndex >= asset.nodes.size()) {
        return Result<AnimationClipChannel>::failure(anim_error("ANIM-CLIP-TARGET-RANGE",
            "Animation channel target.node is out of range"));
    }
    if (channel.path == fastgltf::AnimationPath::Weights) {
        return Result<AnimationClipChannel>::failure(anim_error("ANIM-CLIP-PATH-UNSUPPORTED",
            "Morph-weight animation channels are not supported in this slice",
            "Export TRS joint channels only (translation, rotation, scale)."));
    }
    if (channel.samplerIndex >= animation.samplers.size()) {
        return Result<AnimationClipChannel>::failure(anim_error("ANIM-CLIP-SAMPLER-RANGE",
            "Animation channel sampler index is out of range"));
    }
    const auto& sampler = animation.samplers[channel.samplerIndex];
    AnimationInterpolationMode interpolation = AnimationInterpolationMode::Linear;
    if (sampler.interpolation == fastgltf::AnimationInterpolation::Linear) {
        interpolation = AnimationInterpolationMode::Linear;
    } else if (sampler.interpolation == fastgltf::AnimationInterpolation::Step) {
        interpolation = AnimationInterpolationMode::Step;
    } else {
        return Result<AnimationClipChannel>::failure(anim_error("ANIM-CLIP-INTERP-UNSUPPORTED",
            "Only LINEAR and STEP animation interpolation are supported",
            "Re-export clips with LINEAR (preferred) or STEP interpolation."));
    }

    auto times = read_float_scalars(asset, sampler.inputAccessor, "Animation sampler input");
    if (!times) return Result<AnimationClipChannel>::failure(std::move(times.error()));

    AnimationChannelPath path = AnimationChannelPath::Translation;
    fastgltf::AccessorType expected = fastgltf::AccessorType::Vec3;
    std::size_t components = 3;
    const char* value_label = "Animation sampler output";
    switch (channel.path) {
    case fastgltf::AnimationPath::Translation:
        path = AnimationChannelPath::Translation;
        break;
    case fastgltf::AnimationPath::Scale:
        path = AnimationChannelPath::Scale;
        break;
    case fastgltf::AnimationPath::Rotation:
        path = AnimationChannelPath::Rotation;
        expected = fastgltf::AccessorType::Vec4;
        components = 4;
        break;
    default:
        return Result<AnimationClipChannel>::failure(anim_error("ANIM-CLIP-PATH-UNSUPPORTED",
            "Unsupported animation channel path"));
    }

    auto values = read_float_vectors(asset, sampler.outputAccessor, expected, components, times.value().size(),
        value_label);
    if (!values) return Result<AnimationClipChannel>::failure(std::move(values.error()));

    AnimationClipChannel output;
    output.target_node_index = static_cast<std::uint32_t>(*channel.nodeIndex);
    output.target_node_name = std::string(asset.nodes[*channel.nodeIndex].name);
    output.path = path;
    output.interpolation = interpolation;
    output.times = std::move(times.value());
    output.values = std::move(values.value());
    return Result<AnimationClipChannel>::success(std::move(output));
}

Result<AnimationClip> import_animation(const fastgltf::Asset& asset, const fastgltf::Animation& animation) {
    if (animation.channels.empty()) {
        return Result<AnimationClip>::failure(anim_error("ANIM-CLIP-EMPTY",
            "Animation must contain at least one channel"));
    }
    AnimationClip clip;
    clip.name = std::string(animation.name);
    clip.channels.reserve(animation.channels.size());
    for (const auto& channel : animation.channels) {
        auto imported = import_channel(asset, animation, channel);
        if (!imported) return Result<AnimationClip>::failure(std::move(imported.error()));
        if (!imported.value().times.empty()) {
            clip.duration_seconds = std::max(clip.duration_seconds, imported.value().times.back());
        }
        clip.channels.push_back(std::move(imported.value()));
    }
    if (auto valid = clip.validate(); !valid) return Result<AnimationClip>::failure(std::move(valid.error()));
    return Result<AnimationClip>::success(std::move(clip));
}

} // namespace

Result<void> AnimationClip::validate() const {
    if (channels.empty()) {
        return Result<void>::failure(anim_error("ANIM-CLIP-EMPTY", "Animation clip has no channels"));
    }
    if (!std::isfinite(duration_seconds) || duration_seconds < 0.0f) {
        return Result<void>::failure(anim_error("ANIM-CLIP-DURATION", "Animation clip duration is invalid"));
    }
    for (const auto& channel : channels) {
        if (channel.times.empty()) {
            return Result<void>::failure(anim_error("ANIM-CLIP-EMPTY-KEYS", "Animation channel has no keyframes"));
        }
        const std::size_t components = channel.path == AnimationChannelPath::Rotation ? 4 : 3;
        if (channel.values.size() != channel.times.size() * components) {
            return Result<void>::failure(anim_error("ANIM-CLIP-VALUE-COUNT",
                "Animation channel value count does not match key times"));
        }
        for (float time : channel.times) {
            if (!std::isfinite(time)) {
                return Result<void>::failure(anim_error("ANIM-CLIP-NONFINITE", "Animation channel times are non-finite"));
            }
        }
        for (float value : channel.values) {
            if (!std::isfinite(value)) {
                return Result<void>::failure(anim_error("ANIM-CLIP-NONFINITE", "Animation channel values are non-finite"));
            }
        }
    }
    return Result<void>::success();
}

Result<void> ImportedAnimationSet::validate() const {
    for (const auto& clip : clips) {
        if (auto valid = clip.validate(); !valid) return valid;
    }
    return Result<void>::success();
}

Result<ImportedAnimationSet> import_gltf_animation_clips(const std::filesystem::path& path) {
    auto data = fastgltf::GltfDataBuffer::FromPath(path);
    if (!data) {
        return Result<ImportedAnimationSet>::failure(anim_error("ANIM-CLIP-READ",
            "Failed to read glTF/GLB for animation import",
            "Confirm the path exists and is a readable .gltf or .glb file."));
    }
    fastgltf::Parser parser;
    auto asset = parser.loadGltf(data.get(), path.parent_path(), fastgltf::Options::LoadExternalBuffers);
    if (asset.error() != fastgltf::Error::None) {
        return Result<ImportedAnimationSet>::failure(anim_error("ANIM-CLIP-PARSE",
            "Failed to parse glTF/GLB animations",
            "Fix glTF JSON/binary errors and re-export."));
    }

    ImportedAnimationSet set;
    set.source_path = path;
    set.clips.reserve(asset->animations.size());
    for (const auto& animation : asset->animations) {
        auto clip = import_animation(asset.get(), animation);
        if (!clip) return Result<ImportedAnimationSet>::failure(std::move(clip.error()));
        set.clips.push_back(std::move(clip.value()));
    }
    if (auto valid = set.validate(); !valid) return Result<ImportedAnimationSet>::failure(std::move(valid.error()));
    return Result<ImportedAnimationSet>::success(std::move(set));
}

Result<const ImportedAnimationSet*> AnimationClipLibrary::load(const std::filesystem::path& path) {
    const auto key = canonical_key(path);
    auto imported = import_gltf_animation_clips(path);
    if (!imported) return Result<const ImportedAnimationSet*>::failure(std::move(imported.error()));

    Entry entry;
    entry.path = path;
    entry.set = std::move(imported.value());
    entry.set.source_path = path;
    std::error_code ec;
    const auto write_time = std::filesystem::last_write_time(path, ec);
    if (!ec) {
        entry.write_time = write_time;
        entry.has_write_time = true;
    }
    auto& slot = entries_[key];
    slot = std::move(entry);
    return Result<const ImportedAnimationSet*>::success(&slot.set);
}

Result<const ImportedAnimationSet*> AnimationClipLibrary::get(const std::filesystem::path& path) const {
    const auto key = canonical_key(path);
    const auto found = entries_.find(key);
    if (found == entries_.end()) {
        return Result<const ImportedAnimationSet*>::failure(anim_error("ANIM-CLIP-NOT-LOADED",
            "Animation clip source is not loaded in the library",
            "Call AnimationClipLibrary::load before get/reload."));
    }
    return Result<const ImportedAnimationSet*>::success(&found->second.set);
}

Result<const ImportedAnimationSet*> AnimationClipLibrary::reload(const std::filesystem::path& path) {
    const auto key = canonical_key(path);
    const auto found = entries_.find(key);
    if (found == entries_.end()) {
        return load(path);
    }
    auto imported = import_gltf_animation_clips(path);
    if (!imported) {
        return Result<const ImportedAnimationSet*>::failure(std::move(imported.error()));
    }
    found->second.set = std::move(imported.value());
    found->second.set.source_path = path;
    found->second.path = path;
    std::error_code ec;
    const auto write_time = std::filesystem::last_write_time(path, ec);
    if (!ec) {
        found->second.write_time = write_time;
        found->second.has_write_time = true;
    }
    return Result<const ImportedAnimationSet*>::success(&found->second.set);
}

std::vector<std::filesystem::path> AnimationClipLibrary::poll_changed() {
    std::vector<std::filesystem::path> changed;
    for (auto& [key, entry] : entries_) {
        (void)key;
        std::error_code ec;
        if (!std::filesystem::exists(entry.path, ec) || ec) {
            changed.push_back(entry.path);
            continue;
        }
        const auto write_time = std::filesystem::last_write_time(entry.path, ec);
        if (ec) continue;
        if (!entry.has_write_time || entry.write_time != write_time) {
            changed.push_back(entry.path);
        }
    }
    return changed;
}

Result<std::size_t> AnimationClipLibrary::reload_changed() {
    std::size_t succeeded = 0;
    EngineError first_error{};
    bool have_error = false;
    for (const auto& path : poll_changed()) {
        if (!std::filesystem::exists(path)) continue;
        auto reloaded = reload(path);
        if (reloaded) {
            ++succeeded;
        } else if (!have_error) {
            first_error = reloaded.error();
            have_error = true;
        }
    }
    if (have_error && succeeded == 0) {
        return Result<std::size_t>::failure(std::move(first_error));
    }
    return Result<std::size_t>::success(succeeded);
}

Result<std::array<float, 3>> sample_translation_channel(const AnimationClipChannel& channel, float time_seconds) {
    if (channel.path != AnimationChannelPath::Translation) {
        return Result<std::array<float, 3>>::failure(anim_error("ANIM-CLIP-SAMPLE-PATH",
            "sample_translation_channel requires a translation channel"));
    }
    if (channel.times.empty() || channel.values.size() != channel.times.size() * 3) {
        return Result<std::array<float, 3>>::failure(anim_error("ANIM-CLIP-EMPTY-KEYS",
            "Translation channel has no usable keyframes"));
    }
    if (!std::isfinite(time_seconds)) {
        return Result<std::array<float, 3>>::failure(anim_error("ANIM-CLIP-NONFINITE",
            "Sample time must be finite"));
    }

    const auto read_key = [&](std::size_t index) -> std::array<float, 3> {
        const std::size_t offset = index * 3;
        return {channel.values[offset], channel.values[offset + 1], channel.values[offset + 2]};
    };

    if (time_seconds <= channel.times.front()) return Result<std::array<float, 3>>::success(read_key(0));
    if (time_seconds >= channel.times.back()) {
        return Result<std::array<float, 3>>::success(read_key(channel.times.size() - 1));
    }

    std::size_t upper = 1;
    while (upper < channel.times.size() && channel.times[upper] < time_seconds) ++upper;
    const std::size_t lower = upper - 1;
    if (channel.interpolation == AnimationInterpolationMode::Step) {
        return Result<std::array<float, 3>>::success(read_key(lower));
    }
    const float t0 = channel.times[lower];
    const float t1 = channel.times[upper];
    const float alpha = (t1 > t0) ? ((time_seconds - t0) / (t1 - t0)) : 0.0f;
    const auto a = read_key(lower);
    const auto b = read_key(upper);
    return Result<std::array<float, 3>>::success({
        a[0] + (b[0] - a[0]) * alpha,
        a[1] + (b[1] - a[1]) * alpha,
        a[2] + (b[2] - a[2]) * alpha,
    });
}

namespace {

const AnimationClipChannel* find_root_translation_channel(const AnimationClip& clip,
    const std::string& root_joint_name) {
    const AnimationClipChannel* fallback_root = nullptr;
    const AnimationClipChannel* fallback_hip = nullptr;
    for (const auto& channel : clip.channels) {
        if (channel.path != AnimationChannelPath::Translation) continue;
        if (!root_joint_name.empty() && channel.target_node_name == root_joint_name) return &channel;
        if (channel.target_node_name == "Root" && !fallback_root) fallback_root = &channel;
        if (channel.target_node_name == "Hip" && !fallback_hip) fallback_hip = &channel;
    }
    if (!root_joint_name.empty()) return nullptr;
    if (fallback_root) return fallback_root;
    return fallback_hip;
}

float wrap_sample_time(float time, float duration, bool loop) {
    if (!(duration > 0.0f)) return 0.0f;
    if (!loop) return std::clamp(time, 0.0f, duration);
    const float wrapped = std::fmod(time, duration);
    return wrapped < 0.0f ? wrapped + duration : wrapped;
}

} // namespace

Result<RootMotionDelta> extract_clip_root_motion_delta(const AnimationClip& clip,
    const std::string& root_joint_name, float from_seconds, float to_seconds, bool loop) {
    RootMotionDelta delta;
    const auto* channel = find_root_translation_channel(clip, root_joint_name);
    if (!channel) return Result<RootMotionDelta>::success(delta);

    delta.found_root_channel = true;
    const float duration = clip.duration_seconds > 0.0f ? clip.duration_seconds : channel->times.back();
    const float from_t = wrap_sample_time(from_seconds, duration, loop);
    const float to_t = wrap_sample_time(to_seconds, duration, loop);

    auto sample_at = [&](float t) -> Result<std::array<float, 3>> {
        return sample_translation_channel(*channel, t);
    };

    if (loop && to_seconds > from_seconds && to_t < from_t - 1e-5f) {
        // Wrapped: (from → end) + (start → to)
        const auto a = sample_at(from_t);
        const auto end = sample_at(duration);
        const auto start = sample_at(0.0f);
        const auto b = sample_at(to_t);
        if (!a || !end || !start || !b) {
            return Result<RootMotionDelta>::failure(a ? (end ? (start ? b.error() : start.error()) : end.error())
                                                      : a.error());
        }
        delta.translation = {
            (end.value()[0] - a.value()[0]) + (b.value()[0] - start.value()[0]),
            (end.value()[1] - a.value()[1]) + (b.value()[1] - start.value()[1]),
            (end.value()[2] - a.value()[2]) + (b.value()[2] - start.value()[2]),
        };
        return Result<RootMotionDelta>::success(delta);
    }

    const auto a = sample_at(from_t);
    const auto b = sample_at(to_t);
    if (!a || !b) return Result<RootMotionDelta>::failure(a ? b.error() : a.error());
    delta.translation = {
        b.value()[0] - a.value()[0],
        b.value()[1] - a.value()[1],
        b.value()[2] - a.value()[2],
    };
    return Result<RootMotionDelta>::success(delta);
}

} // namespace engine
