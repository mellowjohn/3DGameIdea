#pragma once

#include "engine/assets/mesh_asset.h"
#include "engine/core/result.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace engine {

enum class FoliageScatterMode {
    GroundCover,
    Discrete,
};

struct FoliageLayerDefinition {
    std::string id;
    std::string label;
    std::string mesh_kind;
    std::array<float, 3> color{0.14f, 0.22f, 0.10f};
    float scale_min = 0.6f;
    float scale_max = 1.1f;
    float density_multiplier = 0.04f;
    float max_slope_ratio = 0.55f;
    float bend_strength = 0.35f;
    float bend_radius = 1.2f;
    float blade_height = 0.55f;
    std::string disturb_vfx_id;
    FoliageScatterMode scatter_mode = FoliageScatterMode::GroundCover;
    std::uint8_t discrete_min_density = 64;
};

struct FoliageLayerPalette {
    std::uint32_t schema_version = 1;
    std::vector<FoliageLayerDefinition> layers;

    [[nodiscard]] Result<void> validate() const;
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static Result<FoliageLayerPalette> from_json(const std::string& text);
    [[nodiscard]] static Result<FoliageLayerPalette> load(const std::filesystem::path& path);
    [[nodiscard]] static Result<void> validate_file(const std::filesystem::path& path);
    [[nodiscard]] const FoliageLayerDefinition* find_by_index(std::uint8_t index) const;
    [[nodiscard]] std::string mesh_key_for_layer(std::uint8_t index) const;
};

[[nodiscard]] std::filesystem::path default_foliage_layers_path(const std::filesystem::path& project_root);
[[nodiscard]] std::vector<std::pair<std::string, ImportedMesh>> build_foliage_layer_meshes(const FoliageLayerPalette& palette);

} // namespace engine
