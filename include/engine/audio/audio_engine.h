#pragma once

#include "engine/core/result.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace engine {

struct AudioEngineConfig {
    bool no_device = false;
    float master_volume = 1.0f;
};

/// miniaudio-backed playback: init/shutdown, one-shot/loop, spatial listener (TICKET-0107).
class AudioEngine final {
public:
    AudioEngine();
    ~AudioEngine();
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    [[nodiscard]] Result<void> initialize(const AudioEngineConfig& config = {});
    void shutdown();
    [[nodiscard]] bool is_initialized() const noexcept;

    void set_project_root(const std::filesystem::path& project_root) noexcept;
    [[nodiscard]] const std::filesystem::path& project_root() const noexcept { return project_root_; }

    void set_master_volume(float volume) noexcept;
    [[nodiscard]] float master_volume() const noexcept;

    void update_listener(const std::array<float, 3>& position, const std::array<float, 3>& forward);
    void update(float dt_seconds);

    [[nodiscard]] Result<void> play_one_shot(const std::filesystem::path& path, bool loop = false);
    [[nodiscard]] Result<void> play_spatial(const std::filesystem::path& path, float x, float y, float z,
        bool loop = false);

    /// Resolve project-relative path then play (non-spatial).
    [[nodiscard]] Result<void> play_project_sound(const std::string& project_relative_path, bool loop = false);
    /// Resolve project-relative path then play at world position.
    [[nodiscard]] Result<void> play_project_sound_at(const std::string& project_relative_path, float x, float y,
        float z, bool loop = false);

private:
    [[nodiscard]] Result<std::filesystem::path> resolve_project_path(const std::string& project_relative_path) const;
    [[nodiscard]] Result<void> play_absolute(const std::filesystem::path& absolute_path, bool loop,
        std::optional<std::array<float, 3>> position);

    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::filesystem::path project_root_;
};

/// Write a minimal mono PCM WAV for suite fixtures (440 Hz sine, 0.1 s).
[[nodiscard]] Result<void> write_test_tone_wav(const std::filesystem::path& output_path, float frequency_hz = 440.0f,
    float duration_seconds = 0.1f);

} // namespace engine
