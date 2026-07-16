#pragma once

#include "engine/core/result.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace engine {

struct PlaySessionSettings {
    std::uint32_t schema_version = 1;
    std::string character_asset = "assets/characters/player.character.json";
    std::string camera_asset = "assets/cameras/game.camera.json";

    [[nodiscard]] Result<void> validate() const;
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static Result<PlaySessionSettings> from_json(const std::string& text);
    [[nodiscard]] static Result<PlaySessionSettings> load(const std::filesystem::path& path);
    [[nodiscard]] Result<void> save(const std::filesystem::path& path) const;
};

[[nodiscard]] std::filesystem::path default_play_session_settings_path(const std::filesystem::path& project_root);

} // namespace engine
