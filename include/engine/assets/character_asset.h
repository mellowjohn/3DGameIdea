#pragma once

#include "engine/core/result.h"
#include "engine/physics/character_controller.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace engine {

struct CharacterAppearance {
    std::string hair_mesh;
    std::array<float, 3> hair_tint{1.0f, 1.0f, 1.0f};
    std::array<float, 3> skin_tint{1.0f, 1.0f, 1.0f};
    std::array<float, 3> eye_tint{1.0f, 1.0f, 1.0f};

    [[nodiscard]] bool has_hair() const { return !hair_mesh.empty(); }
};

struct CharacterAsset {
    std::uint32_t schema_version = 1;
    std::string visual_prefab = "assets/prefabs/player.prefab.json";
    /** Optional project-relative *.rig.json (IK hooks + retarget roles). Empty = none. */
    std::string rig;
    CharacterAppearance appearance;
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

struct AppearanceOption {
    const char* id = "";
    const char* display = "";
};

[[nodiscard]] const std::vector<AppearanceOption>& appearance_hair_options();
[[nodiscard]] const std::vector<AppearanceOption>& appearance_skin_options();
[[nodiscard]] const std::vector<AppearanceOption>& appearance_eye_options();
[[nodiscard]] CharacterAppearance appearance_from_option_ids(std::string_view hair, std::string_view skin,
                                                             std::string_view eyes);
[[nodiscard]] std::string appearance_option_id(const std::vector<AppearanceOption>& options, std::string_view value);

} // namespace engine
