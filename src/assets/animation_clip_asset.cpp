#include "engine/assets/animation_clip_asset.h"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
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

Result<void> AnimationClipLibrary::merge_disk_overrides(ImportedAnimationSet& set,
    const std::filesystem::path& gltf_path) const {
    // Sidecars for clips already in the glTF (Idle, Attack, …).
    std::vector<std::string> clip_names;
    clip_names.reserve(set.clips.size());
    for (const auto& clip : set.clips) clip_names.push_back(clip.name);
    std::set<std::string> merged_names;
    for (const auto& name : clip_names) {
        const auto sidecar = animation_clip_override_path(gltf_path, name);
        std::error_code ec;
        if (!std::filesystem::exists(sidecar, ec) || ec) continue;
        auto loaded = AnimationClipOverrideAsset::load(sidecar);
        if (!loaded) return Result<void>::failure(std::move(loaded.error()));
        auto applied = apply_animation_clip_override(set, loaded.value());
        if (!applied) return applied;
        merged_names.insert(name);
    }
    // Studio create_clip may add override-only clips (e.g. player.BowShoot.anim.json) that are not
    // yet in the glTF. Scan the mesh stem so reload/save_override keeps them in the library.
    const auto stem = gltf_path.stem().string();
    const auto prefix = stem + ".";
    const auto suffix = std::string{".anim.json"};
    std::error_code dir_ec;
    if (!std::filesystem::is_directory(gltf_path.parent_path(), dir_ec) || dir_ec)
        return Result<void>::success();
    for (const auto& entry : std::filesystem::directory_iterator(gltf_path.parent_path(), dir_ec)) {
        if (dir_ec) break;
        if (!entry.is_regular_file()) continue;
        const auto filename = entry.path().filename().string();
        if (filename.size() <= prefix.size() + suffix.size()) continue;
        if (filename.compare(0, prefix.size(), prefix) != 0) continue;
        if (filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) != 0) continue;
        auto loaded = AnimationClipOverrideAsset::load(entry.path());
        if (!loaded) return Result<void>::failure(std::move(loaded.error()));
        if (merged_names.count(loaded.value().clip_name) != 0) continue;
        auto applied = apply_animation_clip_override(set, loaded.value());
        if (!applied) return applied;
        merged_names.insert(loaded.value().clip_name);
    }
    return Result<void>::success();
}

Result<const ImportedAnimationSet*> AnimationClipLibrary::load(const std::filesystem::path& path) {
    const auto key = canonical_key(path);
    auto imported = import_gltf_animation_clips(path);
    if (!imported) return Result<const ImportedAnimationSet*>::failure(std::move(imported.error()));

    Entry entry;
    entry.path = path;
    entry.set = std::move(imported.value());
    entry.set.source_path = path;
    if (auto merged = merge_disk_overrides(entry.set, path); !merged) {
        return Result<const ImportedAnimationSet*>::failure(std::move(merged.error()));
    }
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
    // Keep-override policy on re-import: sidecars always re-merge after glTF load (DEC-0052).
    if (auto merged = merge_disk_overrides(found->second.set, path); !merged) {
        return Result<const ImportedAnimationSet*>::failure(std::move(merged.error()));
    }
    std::error_code ec;
    const auto write_time = std::filesystem::last_write_time(path, ec);
    if (!ec) {
        found->second.write_time = write_time;
        found->second.has_write_time = true;
    }
    return Result<const ImportedAnimationSet*>::success(&found->second.set);
}

Result<void> AnimationClipLibrary::replace_clip(const std::filesystem::path& path, AnimationClip clip) {
    const auto key = canonical_key(path);
    const auto found = entries_.find(key);
    if (found == entries_.end()) {
        return Result<void>::failure(anim_error("ANIM-CLIP-NOT-LOADED",
            "Animation clip source is not loaded in the library",
            "Call AnimationClipLibrary::load before replace_clip."));
    }
    if (auto valid = clip.validate(); !valid) return valid;
    bool replaced = false;
    for (auto& existing : found->second.set.clips) {
        if (existing.name != clip.name) continue;
        existing = std::move(clip);
        replaced = true;
        break;
    }
    if (!replaced) found->second.set.clips.push_back(std::move(clip));
    return Result<void>::success();
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

std::array<float, 4> normalize_quat4(std::array<float, 4> q) {
    const float len = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (!(len > 1e-8f)) return {0.0f, 0.0f, 0.0f, 1.0f};
    const float inv = 1.0f / len;
    return {q[0] * inv, q[1] * inv, q[2] * inv, q[3] * inv};
}

std::array<float, 4> slerp_quat4(std::array<float, 4> a, std::array<float, 4> b, float t) {
    a = normalize_quat4(a);
    b = normalize_quat4(b);
    float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    if (dot < 0.0f) {
        b = {-b[0], -b[1], -b[2], -b[3]};
        dot = -dot;
    }
    if (dot > 0.9995f) {
        return normalize_quat4({
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
    return normalize_quat4({
        a[0] * w1 + b[0] * w2,
        a[1] * w1 + b[1] * w2,
        a[2] * w1 + b[2] * w2,
        a[3] * w1 + b[3] * w2,
    });
}

} // namespace

Result<std::array<float, 4>> sample_rotation_channel(const AnimationClipChannel& channel, float time_seconds) {
    if (channel.path != AnimationChannelPath::Rotation) {
        return Result<std::array<float, 4>>::failure(anim_error("ANIM-CLIP-SAMPLE-PATH",
            "sample_rotation_channel requires a rotation channel"));
    }
    if (channel.times.empty() || channel.values.size() != channel.times.size() * 4) {
        return Result<std::array<float, 4>>::failure(anim_error("ANIM-CLIP-EMPTY-KEYS",
            "Rotation channel has no usable keyframes"));
    }
    if (!std::isfinite(time_seconds)) {
        return Result<std::array<float, 4>>::failure(anim_error("ANIM-CLIP-NONFINITE",
            "Sample time must be finite"));
    }

    const auto read_key = [&](std::size_t index) -> std::array<float, 4> {
        const std::size_t offset = index * 4;
        return normalize_quat4({channel.values[offset], channel.values[offset + 1], channel.values[offset + 2],
            channel.values[offset + 3]});
    };

    if (time_seconds <= channel.times.front()) return Result<std::array<float, 4>>::success(read_key(0));
    if (time_seconds >= channel.times.back()) {
        return Result<std::array<float, 4>>::success(read_key(channel.times.size() - 1));
    }

    std::size_t upper = 1;
    while (upper < channel.times.size() && channel.times[upper] < time_seconds) ++upper;
    const std::size_t lower = upper - 1;
    if (channel.interpolation == AnimationInterpolationMode::Step) {
        return Result<std::array<float, 4>>::success(read_key(lower));
    }
    const float t0 = channel.times[lower];
    const float t1 = channel.times[upper];
    const float alpha = (t1 > t0) ? ((time_seconds - t0) / (t1 - t0)) : 0.0f;
    return Result<std::array<float, 4>>::success(slerp_quat4(read_key(lower), read_key(upper), alpha));
}

Result<std::array<float, 3>> sample_scale_channel(const AnimationClipChannel& channel, float time_seconds) {
    if (channel.path != AnimationChannelPath::Scale) {
        return Result<std::array<float, 3>>::failure(anim_error("ANIM-CLIP-SAMPLE-PATH",
            "sample_scale_channel requires a scale channel"));
    }
    if (channel.times.empty() || channel.values.size() != channel.times.size() * 3) {
        return Result<std::array<float, 3>>::failure(anim_error("ANIM-CLIP-EMPTY-KEYS",
            "Scale channel has no usable keyframes"));
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

const char* to_string(AnimationChannelPath value) noexcept {
    switch (value) {
    case AnimationChannelPath::Translation: return "translation";
    case AnimationChannelPath::Rotation: return "rotation";
    case AnimationChannelPath::Scale: return "scale";
    }
    return "translation";
}

const char* to_string(AnimationInterpolationMode value) noexcept {
    switch (value) {
    case AnimationInterpolationMode::Linear: return "LINEAR";
    case AnimationInterpolationMode::Step: return "STEP";
    }
    return "LINEAR";
}

namespace {

EngineError override_error(std::string code, std::string message,
    std::string remedy = "Fix the *.anim.json override (finite sorted keys, TRS channels) and retry.") {
    return {std::move(code), Severity::Error, ErrorCategory::AssetImport, "animation-clip-override",
        std::move(message), ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

std::string sanitize_clip_name_for_path(const std::string& clip_name) {
    std::string safe;
    safe.reserve(clip_name.size());
    for (unsigned char c : clip_name) {
        if (std::isalnum(c) || c == '_' || c == '-') safe.push_back(static_cast<char>(c));
        else safe.push_back('_');
    }
    while (!safe.empty() && safe.front() == '_') safe.erase(safe.begin());
    while (!safe.empty() && safe.back() == '_') safe.pop_back();
    if (safe.empty()) safe = "clip";
    return safe;
}

Result<AnimationChannelPath> parse_channel_path(const std::string& value) {
    if (value == "translation") return Result<AnimationChannelPath>::success(AnimationChannelPath::Translation);
    if (value == "rotation") return Result<AnimationChannelPath>::success(AnimationChannelPath::Rotation);
    if (value == "scale") return Result<AnimationChannelPath>::success(AnimationChannelPath::Scale);
    return Result<AnimationChannelPath>::failure(
        override_error("ANIM-OV-PATH", "Unknown channel path '" + value + "'",
            "Use translation, rotation, or scale."));
}

Result<AnimationInterpolationMode> parse_interpolation(const std::string& value) {
    if (value == "LINEAR" || value == "linear")
        return Result<AnimationInterpolationMode>::success(AnimationInterpolationMode::Linear);
    if (value == "STEP" || value == "step")
        return Result<AnimationInterpolationMode>::success(AnimationInterpolationMode::Step);
    return Result<AnimationInterpolationMode>::failure(
        override_error("ANIM-OV-INTERP", "Unsupported interpolation '" + value + "'",
            "Overrides support LINEAR and STEP only."));
}

std::string encode_base64(const std::vector<std::uint8_t>& bytes) {
    static constexpr char k_table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((bytes.size() + 2) / 3) * 4);
    std::size_t i = 0;
    while (i + 2 < bytes.size()) {
        const std::uint32_t n = (static_cast<std::uint32_t>(bytes[i]) << 16) |
            (static_cast<std::uint32_t>(bytes[i + 1]) << 8) | bytes[i + 2];
        out.push_back(k_table[(n >> 18) & 63]);
        out.push_back(k_table[(n >> 12) & 63]);
        out.push_back(k_table[(n >> 6) & 63]);
        out.push_back(k_table[n & 63]);
        i += 3;
    }
    if (i < bytes.size()) {
        const std::uint32_t n = static_cast<std::uint32_t>(bytes[i]) << 16 |
            (i + 1 < bytes.size() ? static_cast<std::uint32_t>(bytes[i + 1]) << 8 : 0u);
        out.push_back(k_table[(n >> 18) & 63]);
        out.push_back(k_table[(n >> 12) & 63]);
        out.push_back(i + 1 < bytes.size() ? k_table[(n >> 6) & 63] : '=');
        out.push_back('=');
    }
    return out;
}

void append_floats_le(std::vector<std::uint8_t>& out, const std::vector<float>& values) {
    for (float value : values) {
        std::uint32_t bits = 0;
        static_assert(sizeof(float) == 4, "float must be 32-bit");
        std::memcpy(&bits, &value, sizeof(bits));
        out.push_back(static_cast<std::uint8_t>(bits & 0xffu));
        out.push_back(static_cast<std::uint8_t>((bits >> 8) & 0xffu));
        out.push_back(static_cast<std::uint8_t>((bits >> 16) & 0xffu));
        out.push_back(static_cast<std::uint8_t>((bits >> 24) & 0xffu));
    }
}

} // namespace

std::filesystem::path animation_clip_override_path(const std::filesystem::path& clip_source_path,
    const std::string& clip_name) {
    const auto stem = clip_source_path.stem().string();
    const auto file = stem + "." + sanitize_clip_name_for_path(clip_name) + ".anim.json";
    return clip_source_path.parent_path() / file;
}

AnimationClip AnimationClipOverrideAsset::to_clip() const {
    AnimationClip clip;
    clip.name = clip_name;
    clip.duration_seconds = duration_seconds;
    clip.channels = channels;
    if (!(clip.duration_seconds > 0.0f)) {
        for (const auto& channel : clip.channels) {
            if (!channel.times.empty())
                clip.duration_seconds = std::max(clip.duration_seconds, channel.times.back());
        }
    }
    return clip;
}

AnimationClipOverrideAsset AnimationClipOverrideAsset::from_clip(const std::string& clip_source,
    const AnimationClip& clip) {
    AnimationClipOverrideAsset asset;
    asset.clip_source = clip_source;
    asset.clip_name = clip.name;
    asset.duration_seconds = clip.duration_seconds;
    asset.channels = clip.channels;
    return asset;
}

Result<void> AnimationClipOverrideAsset::validate() const {
    if (schema_version != 1) {
        return Result<void>::failure(override_error("ANIM-OV-SCHEMA", "schemaVersion must be 1"));
    }
    if (clip_name.empty()) {
        return Result<void>::failure(override_error("ANIM-OV-CLIP-NAME", "clipName is required"));
    }
    const AnimationClip clip = to_clip();
    if (auto valid = clip.validate(); !valid) {
        return Result<void>::failure(override_error(valid.error().code, valid.error().message,
            valid.error().remediation.empty() ? "Fix channel times/values in the override."
                                              : valid.error().remediation));
    }
    for (std::size_t i = 1; i < clip.channels.size(); ++i) {
        (void)i;
    }
    for (const auto& channel : channels) {
        if (channel.target_node_name.empty()) {
            return Result<void>::failure(override_error("ANIM-OV-TARGET",
                "Override channel targetNodeName is required"));
        }
        for (std::size_t i = 1; i < channel.times.size(); ++i) {
            if (channel.times[i] < channel.times[i - 1]) {
                return Result<void>::failure(override_error("ANIM-OV-TIME-ORDER",
                    "Override channel times must be non-decreasing"));
            }
        }
    }
    return Result<void>::success();
}

std::string AnimationClipOverrideAsset::to_json() const {
    nlohmann::json document;
    document["schemaVersion"] = schema_version;
    document["kind"] = "animationClipOverride";
    document["clipSource"] = clip_source;
    document["clipName"] = clip_name;
    document["durationSeconds"] = duration_seconds;
    nlohmann::json channels_json = nlohmann::json::array();
    for (const auto& channel : channels) {
        nlohmann::json entry;
        entry["targetNodeName"] = channel.target_node_name;
        entry["targetNodeIndex"] = channel.target_node_index;
        entry["path"] = to_string(channel.path);
        entry["interpolation"] = to_string(channel.interpolation);
        entry["times"] = channel.times;
        entry["values"] = channel.values;
        channels_json.push_back(std::move(entry));
    }
    document["channels"] = std::move(channels_json);
    return document.dump(2);
}

Result<AnimationClipOverrideAsset> AnimationClipOverrideAsset::parse(const std::string& text,
    const std::string& source_name) {
    nlohmann::json document;
    try {
        document = nlohmann::json::parse(text);
    } catch (const std::exception& ex) {
        return Result<AnimationClipOverrideAsset>::failure(
            override_error("ANIM-OV-PARSE", std::string(source_name) + ": " + ex.what()));
    }
    if (!document.is_object()) {
        return Result<AnimationClipOverrideAsset>::failure(
            override_error("ANIM-OV-PARSE", source_name + " must be a JSON object"));
    }
    AnimationClipOverrideAsset asset;
    asset.schema_version = document.value("schemaVersion", 1);
    if (document.contains("kind") && document["kind"].is_string() &&
        document["kind"].get<std::string>() != "animationClipOverride") {
        return Result<AnimationClipOverrideAsset>::failure(
            override_error("ANIM-OV-KIND", "kind must be animationClipOverride"));
    }
    asset.clip_source = document.value("clipSource", std::string{});
    asset.clip_name = document.value("clipName", std::string{});
    asset.duration_seconds = document.value("durationSeconds", 0.0f);
    if (!document.contains("channels") || !document["channels"].is_array()) {
        return Result<AnimationClipOverrideAsset>::failure(
            override_error("ANIM-OV-CHANNELS", "channels[] is required"));
    }
    for (const auto& entry : document["channels"]) {
        if (!entry.is_object()) {
            return Result<AnimationClipOverrideAsset>::failure(
                override_error("ANIM-OV-CHANNELS", "each channels[] entry must be an object"));
        }
        AnimationClipChannel channel;
        channel.target_node_name = entry.value("targetNodeName", std::string{});
        channel.target_node_index = entry.value("targetNodeIndex", 0u);
        auto path = parse_channel_path(entry.value("path", std::string{"translation"}));
        if (!path) return Result<AnimationClipOverrideAsset>::failure(std::move(path.error()));
        channel.path = path.value();
        auto interp = parse_interpolation(entry.value("interpolation", std::string{"LINEAR"}));
        if (!interp) return Result<AnimationClipOverrideAsset>::failure(std::move(interp.error()));
        channel.interpolation = interp.value();
        if (!entry.contains("times") || !entry["times"].is_array() || !entry.contains("values") ||
            !entry["values"].is_array()) {
            return Result<AnimationClipOverrideAsset>::failure(
                override_error("ANIM-OV-KEYS", "channels require times[] and values[] arrays"));
        }
        for (const auto& time : entry["times"]) {
            if (!time.is_number()) {
                return Result<AnimationClipOverrideAsset>::failure(
                    override_error("ANIM-OV-KEYS", "times[] must be numbers"));
            }
            channel.times.push_back(time.get<float>());
        }
        for (const auto& value : entry["values"]) {
            if (!value.is_number()) {
                return Result<AnimationClipOverrideAsset>::failure(
                    override_error("ANIM-OV-KEYS", "values[] must be numbers"));
            }
            channel.values.push_back(value.get<float>());
        }
        asset.channels.push_back(std::move(channel));
    }
    if (auto valid = asset.validate(); !valid)
        return Result<AnimationClipOverrideAsset>::failure(std::move(valid.error()));
    return Result<AnimationClipOverrideAsset>::success(std::move(asset));
}

Result<AnimationClipOverrideAsset> AnimationClipOverrideAsset::load(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        return Result<AnimationClipOverrideAsset>::failure(
            override_error("ANIM-OV-READ", "Failed to read " + path.string()));
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return parse(text, path.string());
}

Result<void> AnimationClipOverrideAsset::save_atomic(const std::filesystem::path& path) const {
    const auto valid = validate();
    if (!valid) return valid;
    const auto parent = path.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    const auto temp = path.string() + ".tmp";
    const auto backup = path.string() + ".bak";
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out) {
            return Result<void>::failure(
                override_error("ANIM-OV-WRITE", "Could not write temp override file", "Check permissions."));
        }
        out << to_json();
    }
    std::error_code ec;
    if (std::filesystem::exists(path, ec))
        std::filesystem::copy_file(path, backup, std::filesystem::copy_options::overwrite_existing, ec);
    // Prefer copy-overwrite: Windows rename cannot replace an existing path and remove+rename
    // often hits ACCESS_DENIED when another handle briefly holds the file.
    std::filesystem::copy_file(temp, path, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        if (std::filesystem::exists(backup)) {
            std::error_code restore_ec;
            std::filesystem::copy_file(backup, path, std::filesystem::copy_options::overwrite_existing,
                restore_ec);
        }
        return Result<void>::failure(
            override_error("ANIM-OV-WRITE", "Could not replace override file: " + ec.message()));
    }
    std::filesystem::remove(temp, ec);
    if (std::filesystem::exists(backup, ec)) std::filesystem::remove(backup, ec);
    return Result<void>::success();
}

Result<void> apply_animation_clip_override(ImportedAnimationSet& set,
    const AnimationClipOverrideAsset& override_asset) {
    if (auto valid = override_asset.validate(); !valid) return valid;
    AnimationClip clip = override_asset.to_clip();
    for (auto& existing : set.clips) {
        if (existing.name != clip.name) continue;
        existing = std::move(clip);
        return Result<void>::success();
    }
    set.clips.push_back(std::move(clip));
    return Result<void>::success();
}

Result<void> sync_animation_clip_override_to_gltf(const std::filesystem::path& gltf_path,
    const AnimationClipOverrideAsset& override_asset) {
    if (auto valid = override_asset.validate(); !valid) return valid;
    const auto extension = gltf_path.extension().string();
    if (extension == ".glb" || extension == ".GLB") {
        return Result<void>::failure(override_error("ANIM-OV-SYNC-GLB",
            "Sync to source currently supports .gltf JSON only",
            "Export/save the clip as .gltf, or keep the engine *.anim.json override."));
    }
    std::ifstream in(gltf_path);
    if (!in) {
        return Result<void>::failure(override_error("ANIM-OV-SYNC-READ",
            "Failed to read glTF for sync: " + gltf_path.string()));
    }
    nlohmann::json document;
    try {
        in >> document;
    } catch (const std::exception& ex) {
        return Result<void>::failure(override_error("ANIM-OV-SYNC-PARSE", ex.what()));
    }
    if (!document.contains("nodes") || !document["nodes"].is_array() || !document.contains("animations") ||
        !document["animations"].is_array()) {
        return Result<void>::failure(override_error("ANIM-OV-SYNC-SCHEMA",
            "glTF must contain nodes[] and animations[]"));
    }

    int anim_index = -1;
    for (int i = 0; i < static_cast<int>(document["animations"].size()); ++i) {
        const auto& anim = document["animations"][i];
        if (anim.value("name", std::string{}) == override_asset.clip_name) {
            anim_index = i;
            break;
        }
    }
    if (anim_index < 0) {
        return Result<void>::failure(override_error("ANIM-OV-SYNC-CLIP",
            "Animation '" + override_asset.clip_name + "' not found in glTF",
            "Sync only updates existing named animations; rename to match or re-export."));
    }
    auto& animation = document["animations"][anim_index];
    if (!animation.contains("channels") || !animation.contains("samplers")) {
        return Result<void>::failure(override_error("ANIM-OV-SYNC-SCHEMA",
            "Animation is missing channels/samplers"));
    }
    if (!document.contains("accessors") || !document["accessors"].is_array())
        document["accessors"] = nlohmann::json::array();
    if (!document.contains("bufferViews") || !document["bufferViews"].is_array())
        document["bufferViews"] = nlohmann::json::array();
    if (!document.contains("buffers") || !document["buffers"].is_array())
        document["buffers"] = nlohmann::json::array();

    auto find_node_index = [&](const std::string& name) -> int {
        for (int i = 0; i < static_cast<int>(document["nodes"].size()); ++i) {
            if (document["nodes"][i].value("name", std::string{}) == name) return i;
        }
        return -1;
    };

    for (const auto& channel : override_asset.channels) {
        const int node_index = find_node_index(channel.target_node_name);
        if (node_index < 0) {
            return Result<void>::failure(override_error("ANIM-OV-SYNC-NODE",
                "Node '" + channel.target_node_name + "' not found in glTF"));
        }
        const std::string path = to_string(channel.path);
        int channel_index = -1;
        for (int i = 0; i < static_cast<int>(animation["channels"].size()); ++i) {
            const auto& ch = animation["channels"][i];
            if (!ch.contains("target")) continue;
            if (ch["target"].value("node", -1) != node_index) continue;
            if (ch["target"].value("path", std::string{}) != path) continue;
            channel_index = i;
            break;
        }
        if (channel_index < 0) {
            return Result<void>::failure(override_error("ANIM-OV-SYNC-CHANNEL",
                "No glTF channel for " + channel.target_node_name + "." + path,
                "Sync updates existing TRS channels only; add the channel in the art tool first."));
        }
        const int sampler_index = animation["channels"][channel_index].value("sampler", -1);
        if (sampler_index < 0 || sampler_index >= static_cast<int>(animation["samplers"].size())) {
            return Result<void>::failure(override_error("ANIM-OV-SYNC-SAMPLER",
                "Channel sampler index out of range"));
        }
        auto& sampler = animation["samplers"][sampler_index];
        const std::string interp = sampler.value("interpolation", std::string{"LINEAR"});
        if (interp != "LINEAR" && interp != "STEP") {
            return Result<void>::failure(override_error("ANIM-OV-SYNC-INTERP",
                "Cannot sync onto " + interp + " sampler",
                "Re-export the clip with LINEAR or STEP interpolation."));
        }
        sampler["interpolation"] = to_string(channel.interpolation);

        std::vector<std::uint8_t> blob;
        append_floats_le(blob, channel.times);
        const std::size_t times_bytes = blob.size();
        append_floats_le(blob, channel.values);
        const std::size_t values_bytes = blob.size() - times_bytes;

        const int buffer_index = static_cast<int>(document["buffers"].size());
        document["buffers"].push_back(nlohmann::json{
            {"byteLength", static_cast<int>(blob.size())},
            {"uri", "data:application/octet-stream;base64," + encode_base64(blob)},
        });
        const int times_view = static_cast<int>(document["bufferViews"].size());
        document["bufferViews"].push_back(nlohmann::json{
            {"buffer", buffer_index}, {"byteOffset", 0}, {"byteLength", static_cast<int>(times_bytes)},
        });
        const int values_view = static_cast<int>(document["bufferViews"].size());
        document["bufferViews"].push_back(nlohmann::json{
            {"buffer", buffer_index}, {"byteOffset", static_cast<int>(times_bytes)},
            {"byteLength", static_cast<int>(values_bytes)},
        });
        const int times_accessor = static_cast<int>(document["accessors"].size());
        float t_min = channel.times.front();
        float t_max = channel.times.back();
        document["accessors"].push_back(nlohmann::json{
            {"bufferView", times_view}, {"componentType", 5126},
            {"count", static_cast<int>(channel.times.size())}, {"type", "SCALAR"},
            {"min", nlohmann::json::array({t_min})}, {"max", nlohmann::json::array({t_max})},
        });
        const int values_accessor = static_cast<int>(document["accessors"].size());
        const char* value_type = channel.path == AnimationChannelPath::Rotation ? "VEC4" : "VEC3";
        document["accessors"].push_back(nlohmann::json{
            {"bufferView", values_view}, {"componentType", 5126},
            {"count", static_cast<int>(channel.times.size())}, {"type", value_type},
        });
        sampler["input"] = times_accessor;
        sampler["output"] = values_accessor;
    }

    const auto temp = gltf_path.string() + ".tmp";
    const auto backup = gltf_path.string() + ".bak";
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out) {
            return Result<void>::failure(override_error("ANIM-OV-SYNC-WRITE",
                "Could not write temp glTF file"));
        }
        out << document.dump(2);
    }
    std::error_code ec;
    if (std::filesystem::exists(gltf_path, ec))
        std::filesystem::copy_file(gltf_path, backup, std::filesystem::copy_options::overwrite_existing, ec);
    // Prefer copy-overwrite over rename-replace (Windows rename / remove+rename is brittle).
    std::filesystem::copy_file(temp, gltf_path, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        if (std::filesystem::exists(backup)) {
            std::error_code restore_ec;
            std::filesystem::copy_file(backup, gltf_path, std::filesystem::copy_options::overwrite_existing,
                restore_ec);
        }
        return Result<void>::failure(
            override_error("ANIM-OV-SYNC-WRITE", "Could not replace glTF file: " + ec.message()));
    }
    std::filesystem::remove(temp, ec);
    if (std::filesystem::exists(backup, ec)) std::filesystem::remove(backup, ec);
    return Result<void>::success();
}

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
