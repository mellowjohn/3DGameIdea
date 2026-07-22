#pragma once

#include "engine/core/result.h"
#include "engine/physics/character_controller.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace engine {

struct CharacterAsset {
    std::uint32_t schema_version = 1;
    std::string visual_prefab = "assets/prefabs/player.prefab.json";
    /** Optional project-relative *.rig.json (IK hooks + retarget roles). Empty = none. */
    std::string rig;
    float capsule_radius = 0.35f;
    float capsule_half_height = 0.85f;
    float max_slope_ratio = 0.45f;
    float step_height = 0.35f;
    float max_speed = 6.0f;
    float gravity = 9.81f;
    float jump_velocity = 5.0f;

    [[nodiscard]] Result<void> validate() const;
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static Result<CharacterAsset> from_json(const std::string& text);
    [[nodiscard]] static Result<CharacterAsset> load(const std::filesystem::path& path);
    [[nodiscard]] CharacterControllerConfig controller_config() const;
};

} // namespace engine
