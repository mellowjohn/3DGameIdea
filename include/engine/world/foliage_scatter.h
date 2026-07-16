#pragma once

#include "engine/world/foliage_density.h"
#include "engine/world/foliage_layers.h"
#include "engine/world/world_partition.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace engine {

struct FoliageInstance {
    std::array<float, 16> model{};
    std::uint8_t layer_index = 0;
};

struct FoliageScatterConfig {
    static constexpr std::uint32_t k_max_instances_per_cell = 2048;
    static constexpr std::uint8_t k_min_density = 8;
    float distance_falloff_start = 120.0f;
    float distance_falloff_end = 220.0f;
};

[[nodiscard]] std::vector<FoliageInstance> scatter_foliage_cell(CellCoord cell, const FoliageDensityStore& density,
    const FoliageLayerPalette& palette, const FoliageScatterConfig& config = {},
    const std::array<float, 3>& camera_position = {0.0f, 0.0f, 0.0f});

[[nodiscard]] std::map<std::string, std::vector<FoliageInstance>> scatter_foliage_cells(
    const std::set<CellCoord>& cells, const FoliageDensityStore& density, const FoliageLayerPalette& palette,
    const FoliageScatterConfig& config = {}, const std::array<float, 3>& camera_position = {0.0f, 0.0f, 0.0f});

} // namespace engine
