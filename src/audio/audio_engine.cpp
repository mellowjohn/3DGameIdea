#include "engine/audio/audio_engine.h"

#include "engine/core/error.h"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <list>
#include <vector>

namespace engine {
namespace {

EngineError audio_error(std::string code, std::string message, std::string remedy = {}) {
    if (remedy.empty()) remedy = "Verify the audio file path and that miniaudio initialized successfully.";
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Validation, "audio", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

std::array<float, 3> normalize_or_default(const std::array<float, 3>& forward) {
    const float length =
        std::sqrt(forward[0] * forward[0] + forward[1] * forward[1] + forward[2] * forward[2]);
    if (length <= 1e-5f) return {0.0f, 0.0f, 1.0f};
    return {forward[0] / length, forward[1] / length, forward[2] / length};
}

} // namespace

struct AudioEngine::Impl {
    ma_engine engine{};
    bool initialized = false;
    float master_volume = 1.0f;
    // list: ma_sound is not relocatable; vector growth would invalidate engine pointers.
    std::list<ma_sound> active_sounds;
};

AudioEngine::AudioEngine() : impl_(std::make_unique<Impl>()) {}

AudioEngine::~AudioEngine() { shutdown(); }

Result<void> AudioEngine::initialize(const AudioEngineConfig& config) {
    if (impl_->initialized) return Result<void>::success();
    ma_engine_config engine_config = ma_engine_config_init();
    if (config.no_device) {
        // Headless/offline engines must declare mix format; the device normally supplies it.
        engine_config.noDevice = MA_TRUE;
        engine_config.channels = 2;
        engine_config.sampleRate = 48000;
    }
    const ma_result result = ma_engine_init(&engine_config, &impl_->engine);
    if (result != MA_SUCCESS) {
        return Result<void>::failure(
            audio_error("AUDIO-INIT-FAILED", "miniaudio engine initialization failed with code " +
                                                  std::to_string(static_cast<int>(result)),
                "Confirm an audio backend is available or use no_device mode for headless tests."));
    }
    impl_->initialized = true;
    set_master_volume(config.master_volume);
    return Result<void>::success();
}

void AudioEngine::shutdown() {
    if (!impl_ || !impl_->initialized) return;
    for (auto& sound : impl_->active_sounds) ma_sound_uninit(&sound);
    impl_->active_sounds.clear();
    ma_engine_uninit(&impl_->engine);
    impl_->initialized = false;
}

bool AudioEngine::is_initialized() const noexcept { return impl_ && impl_->initialized; }

void AudioEngine::set_project_root(const std::filesystem::path& project_root) noexcept {
    project_root_ = project_root;
}

void AudioEngine::set_master_volume(float volume) noexcept {
    if (!impl_) return;
    impl_->master_volume = std::clamp(volume, 0.0f, 1.0f);
    if (impl_->initialized) ma_engine_set_volume(&impl_->engine, impl_->master_volume);
}

float AudioEngine::master_volume() const noexcept { return impl_ ? impl_->master_volume : 0.0f; }

void AudioEngine::update_listener(const std::array<float, 3>& position, const std::array<float, 3>& forward) {
    if (!impl_ || !impl_->initialized) return;
    const auto normalized = normalize_or_default(forward);
    ma_engine_listener_set_position(&impl_->engine, 0, position[0], position[1], position[2]);
    ma_engine_listener_set_direction(&impl_->engine, 0, normalized[0], normalized[1], normalized[2]);
    ma_engine_listener_set_world_up(&impl_->engine, 0, 0.0f, 1.0f, 0.0f);
}

void AudioEngine::update(float /*dt_seconds*/) {
    if (!impl_ || !impl_->initialized) return;
    for (auto it = impl_->active_sounds.begin(); it != impl_->active_sounds.end();) {
        if (ma_sound_is_playing(&*it)) {
            ++it;
            continue;
        }
        ma_sound_uninit(&*it);
        it = impl_->active_sounds.erase(it);
    }
}

Result<std::filesystem::path> AudioEngine::resolve_project_path(const std::string& project_relative_path) const {
    if (project_relative_path.empty()) {
        return Result<std::filesystem::path>::failure(
            audio_error("AUDIO-PATH-EMPTY", "Audio path is empty.", "Pass a project-relative assets path."));
    }
    const std::filesystem::path relative = project_relative_path;
    if (relative.is_absolute()) {
        return Result<std::filesystem::path>::failure(
            audio_error("AUDIO-PATH-ABSOLUTE", "Audio path must be project-relative.", "Use assets/... paths."));
    }
    if (project_root_.empty()) {
        return Result<std::filesystem::path>::failure(
            audio_error("AUDIO-PROJECT-ROOT", "Audio project root is not set.", "Initialize the editor play session."));
    }
    const auto absolute = (project_root_ / relative).lexically_normal();
    const auto root = project_root_.lexically_normal();
    const auto root_prefix = root.string();
    const auto absolute_prefix = absolute.string();
    if (absolute_prefix.size() < root_prefix.size() ||
        absolute_prefix.compare(0, root_prefix.size(), root_prefix) != 0) {
        return Result<std::filesystem::path>::failure(audio_error("AUDIO-PATH-ESCAPE",
            "Audio path must not escape the project root.", "Use paths under assets/."));
    }
    return Result<std::filesystem::path>::success(absolute);
}

Result<void> AudioEngine::play_absolute(const std::filesystem::path& absolute_path, bool loop,
    std::optional<std::array<float, 3>> position) {
    if (!impl_ || !impl_->initialized) {
        return Result<void>::failure(
            audio_error("AUDIO-NOT-READY", "Audio engine is not initialized.", "Call initialize() before playback."));
    }
    if (!std::filesystem::exists(absolute_path)) {
        return Result<void>::failure(audio_error("AUDIO-FILE-MISSING", "Audio file not found: " + absolute_path.string(),
            "Confirm the asset exists under the project and the path is correct."));
    }

    // Spatialization is enabled by default; only disable it for non-positional playback.
    ma_uint32 flags = 0;
    if (!position.has_value()) flags = MA_SOUND_FLAG_NO_SPATIALIZATION;
    if (loop) flags |= MA_SOUND_FLAG_LOOPING;

    impl_->active_sounds.emplace_back();
    ma_sound& sound = impl_->active_sounds.back();
    const ma_result init_result =
        ma_sound_init_from_file(&impl_->engine, absolute_path.string().c_str(), flags, nullptr, nullptr, &sound);
    if (init_result != MA_SUCCESS) {
        impl_->active_sounds.pop_back();
        return Result<void>::failure(audio_error("AUDIO-LOAD-FAILED",
            "Failed to load audio file: " + absolute_path.string() + " (code " +
                std::to_string(static_cast<int>(init_result)) + ")",
            "Use a supported format such as WAV and verify the file is not corrupt."));
    }

    if (position.has_value()) {
        ma_sound_set_position(&sound, (*position)[0], (*position)[1], (*position)[2]);
        ma_sound_set_min_distance(&sound, 0.5f);
        ma_sound_set_max_distance(&sound, 40.0f);
        ma_sound_set_rolloff(&sound, 1.0f);
    }

    const ma_result start_result = ma_sound_start(&sound);
    if (start_result != MA_SUCCESS) {
        ma_sound_uninit(&sound);
        impl_->active_sounds.pop_back();
        return Result<void>::failure(audio_error("AUDIO-PLAY-FAILED",
            "Failed to start audio playback (code " + std::to_string(static_cast<int>(start_result)) + ")",
            "Retry after confirming the audio device is available."));
    }
    return Result<void>::success();
}

Result<void> AudioEngine::play_one_shot(const std::filesystem::path& path, bool loop) {
    return play_absolute(path, loop, std::nullopt);
}

Result<void> AudioEngine::play_spatial(const std::filesystem::path& path, float x, float y, float z, bool loop) {
    return play_absolute(path, loop, std::array<float, 3>{x, y, z});
}

Result<void> AudioEngine::play_project_sound(const std::string& project_relative_path, bool loop) {
    const auto resolved = resolve_project_path(project_relative_path);
    if (!resolved) return Result<void>::failure(resolved.error());
    return play_one_shot(resolved.value(), loop);
}

Result<void> AudioEngine::play_project_sound_at(const std::string& project_relative_path, float x, float y, float z,
    bool loop) {
    const auto resolved = resolve_project_path(project_relative_path);
    if (!resolved) return Result<void>::failure(resolved.error());
    return play_spatial(resolved.value(), x, y, z, loop);
}

Result<void> write_test_tone_wav(const std::filesystem::path& output_path, float frequency_hz, float duration_seconds) {
    if (frequency_hz <= 0.0f || duration_seconds <= 0.0f) {
        return Result<void>::failure(
            audio_error("AUDIO-WAV-ARGS", "Test tone frequency and duration must be positive.", "Use defaults."));
    }
    const std::uint32_t sample_rate = 22050;
    const std::uint16_t channels = 1;
    const std::uint16_t bits_per_sample = 16;
    const std::uint32_t sample_count =
        static_cast<std::uint32_t>(std::ceil(duration_seconds * static_cast<float>(sample_rate)));
    const std::uint32_t data_bytes = sample_count * channels * (bits_per_sample / 8);
    const std::uint32_t riff_size = 36 + data_bytes;

    std::error_code ec;
    std::filesystem::create_directories(output_path.parent_path(), ec);

    std::ofstream output(output_path, std::ios::binary);
    if (!output) {
        return Result<void>::failure(
            audio_error("AUDIO-WAV-IO", "Could not open WAV output: " + output_path.string(), "Check permissions."));
    }

    auto write_u32 = [&](std::uint32_t value) {
        output.put(static_cast<char>(value & 0xFF));
        output.put(static_cast<char>((value >> 8) & 0xFF));
        output.put(static_cast<char>((value >> 16) & 0xFF));
        output.put(static_cast<char>((value >> 24) & 0xFF));
    };
    auto write_u16 = [&](std::uint16_t value) {
        output.put(static_cast<char>(value & 0xFF));
        output.put(static_cast<char>((value >> 8) & 0xFF));
    };

    output.write("RIFF", 4);
    write_u32(riff_size);
    output.write("WAVE", 4);
    output.write("fmt ", 4);
    write_u32(16);
    write_u16(1);
    write_u16(channels);
    write_u32(sample_rate);
    write_u32(sample_rate * channels * (bits_per_sample / 8));
    write_u16(static_cast<std::uint16_t>(channels * (bits_per_sample / 8)));
    write_u16(bits_per_sample);
    output.write("data", 4);
    write_u32(data_bytes);

    for (std::uint32_t sample = 0; sample < sample_count; ++sample) {
        const float t = static_cast<float>(sample) / static_cast<float>(sample_rate);
        const float wave = std::sin(2.0f * 3.14159265f * frequency_hz * t);
        const float envelope = sample < sample_count / 10 ? static_cast<float>(sample) / (sample_count / 10.0f)
                                                          : (sample > sample_count * 9 / 10
                                                                    ? static_cast<float>(sample_count - sample) /
                                                                          (sample_count / 10.0f)
                                                                    : 1.0f);
        const auto pcm = static_cast<std::int16_t>(std::clamp(wave * envelope, -1.0f, 1.0f) * 3000.0f);
        write_u16(static_cast<std::uint16_t>(pcm));
    }

    if (!output) {
        return Result<void>::failure(
            audio_error("AUDIO-WAV-IO", "Failed while writing WAV: " + output_path.string(), "Retry."));
    }
    return Result<void>::success();
}

} // namespace engine
