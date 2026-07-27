#include "engine/world/foliage_scatter.h"

#include "engine/world/terrain.h"
#include "engine/world/water_store.h"

#include <DirectXMath.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace engine {
namespace {

using namespace DirectX;

std::uint32_t hash_mix(std::uint32_t value) noexcept {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

std::uint32_t hash_seed(CellCoord cell, std::uint32_t x, std::uint32_t z, std::uint8_t layer, std::uint32_t instance) noexcept {
    std::uint32_t seed = static_cast<std::uint32_t>(cell.x * 73856093) ^ static_cast<std::uint32_t>(cell.z * 19349663) ^
                         x * 83492791u ^ z * 50331653u ^ static_cast<std::uint32_t>(layer) * 25165843u ^ instance * 12582917u;
    return hash_mix(seed);
}

float hash_unit(std::uint32_t seed) noexcept {
    return static_cast<float>(hash_mix(seed)) / static_cast<float>(0xffffffffu);
}

float distance_falloff(const std::array<float, 3>& camera_position, float world_x, float /*world_y*/, float world_z,
    const FoliageScatterConfig& config) {
    // Horizontal distance only. Including camera Y made high Scene/Sculpt orbits bake near-zero
    // instance counts into newly streamed cells (foliage "vanishes" until a rebuild).
    const float dx = world_x - camera_position[0];
    const float dz = world_z - camera_position[2];
    const float distance = std::sqrt(dx * dx + dz * dz);
    if (distance <= config.distance_falloff_start) return 1.0f;
    if (distance >= config.distance_falloff_end) return 0.0f;
    const float range = config.distance_falloff_end - config.distance_falloff_start;
    return std::clamp(1.0f - (distance - config.distance_falloff_start) / range, 0.0f, 1.0f);
}

void store_matrix(std::array<float, 16>& out, float x, float y, float z, float yaw, float scale) {
    XMMATRIX matrix = XMMatrixScaling(scale, scale, scale) * XMMatrixRotationY(yaw) * XMMatrixTranslation(x, y, z);
    XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(out.data()), matrix);
}

struct DiscreteSpawn {
    std::uint32_t x = 0;
    std::uint32_t z = 0;
    std::uint8_t layer_index = 0;
    float sample_x = 0.0f;
    float sample_z = 0.0f;
};

struct GroundCoverSpawn {
    std::uint32_t x = 0;
    std::uint32_t z = 0;
    std::uint8_t layer_index = 0;
    float sample_x = 0.0f;
    float sample_z = 0.0f;
    float expected = 0.0f;
};

[[nodiscard]] float height_at_grid(const std::vector<float>& heights, std::uint32_t resolution, std::uint32_t x,
    std::uint32_t z) {
    return heights[static_cast<std::size_t>(z) * resolution + x];
}

[[nodiscard]] float slope_ratio_at_grid(const std::vector<float>& heights, std::uint32_t resolution, float step,
    std::uint32_t x, std::uint32_t z) {
    const float center = height_at_grid(heights, resolution, x, z);
    const float east = height_at_grid(heights, resolution, (std::min)(x + 1, resolution - 1), z);
    const float north = height_at_grid(heights, resolution, x, (std::min)(z + 1, resolution - 1));
    const float rise = (std::max)(std::abs(east - center), std::abs(north - center));
    return rise / (std::max)(step, 0.001f);
}

[[nodiscard]] float sample_height_bilinear(const std::vector<float>& heights, std::uint32_t resolution, float step,
    float origin_x, float origin_z, float world_x, float world_z) {
    const float fx = (world_x - origin_x) / step;
    const float fz = (world_z - origin_z) / step;
    const float clamped_x = std::clamp(fx, 0.0f, static_cast<float>(resolution - 1));
    const float clamped_z = std::clamp(fz, 0.0f, static_cast<float>(resolution - 1));
    const std::uint32_t x0 = static_cast<std::uint32_t>(clamped_x);
    const std::uint32_t z0 = static_cast<std::uint32_t>(clamped_z);
    const std::uint32_t x1 = (std::min)(x0 + 1, resolution - 1);
    const std::uint32_t z1 = (std::min)(z0 + 1, resolution - 1);
    const float tx = clamped_x - static_cast<float>(x0);
    const float tz = clamped_z - static_cast<float>(z0);
    const float h00 = height_at_grid(heights, resolution, x0, z0);
    const float h10 = height_at_grid(heights, resolution, x1, z0);
    const float h01 = height_at_grid(heights, resolution, x0, z1);
    const float h11 = height_at_grid(heights, resolution, x1, z1);
    const float h0 = h00 + (h10 - h00) * tx;
    const float h1 = h01 + (h11 - h01) * tx;
    return h0 + (h1 - h0) * tz;
}

} // namespace

std::vector<FoliageInstance> scatter_foliage_cell(CellCoord cell, const FoliageDensityStore& density,
    const FoliageLayerPalette& palette, const FoliageScatterConfig& config,
    const std::array<float, 3>& camera_position) {
    std::vector<FoliageInstance> instances;
    const auto* entry = density.find_cell(cell);
    if (!entry) return instances;
    constexpr std::uint32_t k_res = FoliageDensityStore::k_resolution;
    bool any_density = false;
    for (const std::uint8_t sample : entry->density) {
        if (sample != 0) {
            any_density = true;
            break;
        }
    }
    if (!any_density) return instances;
    const float origin_x = static_cast<float>(cell.x) * FoliageDensityStore::k_cell_size - FoliageDensityStore::k_cell_size * 0.5f;
    const float origin_z = static_cast<float>(cell.z) * FoliageDensityStore::k_cell_size - FoliageDensityStore::k_cell_size * 0.5f;
    const float step = FoliageDensityStore::k_cell_size / static_cast<float>(k_res - 1);

    // Sample each density texel once. The previous path called sample_terrain_height 4+ times per
    // painted texel (center + slope neighbors) and again during rejection sampling.
    std::vector<float> heights(static_cast<std::size_t>(k_res) * k_res);
    for (std::uint32_t z = 0; z < k_res; ++z) {
        for (std::uint32_t x = 0; x < k_res; ++x) {
            const float sample_x = origin_x + static_cast<float>(x) * step;
            const float sample_z = origin_z + static_cast<float>(z) * step;
            heights[static_cast<std::size_t>(z) * k_res + x] = sample_terrain_height(sample_x, sample_z);
        }
    }

    std::vector<DiscreteSpawn> discrete_spawns;
    std::vector<GroundCoverSpawn> ground_spawns;
    double ground_cover_expected = 0.0;
    ground_spawns.reserve(256);
    discrete_spawns.reserve(64);

    for (std::uint32_t z = 0; z < k_res; ++z) {
        for (std::uint32_t x = 0; x < k_res; ++x) {
            const std::size_t index = static_cast<std::size_t>(z) * k_res + x;
            const std::uint8_t sample_density = entry->density[index];
            const std::uint8_t layer_index = entry->layer[index];
            const auto* layer = palette.find_by_index(layer_index);
            if (!layer || sample_density == 0) continue;
            const float sample_x = origin_x + static_cast<float>(x) * step;
            const float sample_z = origin_z + static_cast<float>(z) * step;
            if (slope_ratio_at_grid(heights, k_res, step, x, z) > layer->max_slope_ratio) continue;
            const float ground_y = heights[index];
            if (is_underwater(sample_x, sample_z, ground_y + 0.05f)) continue;
            const float distance_scale = distance_falloff(camera_position, sample_x, ground_y, sample_z, config);
            if (distance_scale <= 0.0f) continue;

            if (layer->scatter_mode == FoliageScatterMode::Discrete) {
                if (sample_density >= layer->discrete_min_density) {
                    discrete_spawns.push_back(DiscreteSpawn{x, z, layer_index, sample_x, sample_z});
                }
                continue;
            }

            if (sample_density < FoliageScatterConfig::k_min_density) continue;
            const float expected = static_cast<float>(sample_density) * layer->density_multiplier * distance_scale;
            if (expected <= 0.0f) continue;
            ground_cover_expected += static_cast<double>(expected);
            ground_spawns.push_back(GroundCoverSpawn{x, z, layer_index, sample_x, sample_z, expected});
        }
    }

    instances.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(
        static_cast<std::uint64_t>(ground_cover_expected) + discrete_spawns.size(),
        static_cast<std::uint64_t>(config.k_max_instances_per_cell))));

    for (const auto& spawn : discrete_spawns) {
        if (instances.size() >= config.k_max_instances_per_cell) return instances;
        const auto* layer = palette.find_by_index(spawn.layer_index);
        if (!layer) continue;
        const std::uint32_t seed = hash_seed(cell, spawn.x, spawn.z, spawn.layer_index, 0u);
        const float jitter_x = (hash_unit(seed ^ 0x11u) - 0.5f) * step;
        const float jitter_z = (hash_unit(seed ^ 0x22u) - 0.5f) * step;
        const float world_x = spawn.sample_x + jitter_x;
        const float world_z = spawn.sample_z + jitter_z;
        const float world_y =
            sample_height_bilinear(heights, k_res, step, origin_x, origin_z, world_x, world_z);
        if (is_underwater(world_x, world_z, world_y + 0.05f)) continue;
        const float yaw = hash_unit(seed ^ 0x33u) * 6.2831853f;
        const float scale = layer->scale_min + (layer->scale_max - layer->scale_min) * hash_unit(seed ^ 0x44u);
        FoliageInstance instance;
        instance.layer_index = spawn.layer_index;
        store_matrix(instance.model, world_x, world_y + 0.04f, world_z, yaw, scale);
        instances.push_back(std::move(instance));
    }

    const std::size_t remaining_budget =
        config.k_max_instances_per_cell > instances.size() ? config.k_max_instances_per_cell - instances.size() : 0;
    if (remaining_budget == 0 || ground_spawns.empty() || ground_cover_expected <= 0.0) return instances;

    // Texel-driven ground cover (no rejection storm). Scale each texel's expected count so the cell
    // stays under budget while still covering the painted area instead of aborting mid-grid-row.
    const double scale =
        std::min(1.0, static_cast<double>(remaining_budget) / ground_cover_expected);
    double carry = 0.0;
    for (const auto& spawn : ground_spawns) {
        if (instances.size() >= config.k_max_instances_per_cell) break;
        const auto* layer = palette.find_by_index(spawn.layer_index);
        if (!layer) continue;
        carry += static_cast<double>(spawn.expected) * scale;
        int count = static_cast<int>(carry);
        if (count <= 0) continue;
        carry -= static_cast<double>(count);
        for (int i = 0; i < count; ++i) {
            if (instances.size() >= config.k_max_instances_per_cell) break;
            const std::uint32_t seed =
                hash_seed(cell, spawn.x, spawn.z, spawn.layer_index, static_cast<std::uint32_t>(i + 1));
            const float jitter_x = (hash_unit(seed ^ 0x11u) - 0.5f) * step;
            const float jitter_z = (hash_unit(seed ^ 0x22u) - 0.5f) * step;
            const float world_x = spawn.sample_x + jitter_x;
            const float world_z = spawn.sample_z + jitter_z;
            const float world_y =
                sample_height_bilinear(heights, k_res, step, origin_x, origin_z, world_x, world_z);
            if (is_underwater(world_x, world_z, world_y + 0.05f)) continue;
            const float yaw = hash_unit(seed ^ 0x33u) * 6.2831853f;
            const float instance_scale =
                layer->scale_min + (layer->scale_max - layer->scale_min) * hash_unit(seed ^ 0x44u);
            FoliageInstance instance;
            instance.layer_index = spawn.layer_index;
            store_matrix(instance.model, world_x, world_y + 0.04f, world_z, yaw, instance_scale);
            instances.push_back(std::move(instance));
        }
    }

    return instances;
}

std::map<std::string, std::vector<FoliageInstance>> scatter_foliage_cells(const std::set<CellCoord>& cells,
    const FoliageDensityStore& density, const FoliageLayerPalette& palette, const FoliageScatterConfig& config,
    const std::array<float, 3>& camera_position) {
    std::map<std::string, std::vector<FoliageInstance>> grouped;
    for (const auto& cell : cells) {
        const auto cell_instances = scatter_foliage_cell(cell, density, palette, config, camera_position);
        for (const auto& instance : cell_instances) {
            const auto mesh_key = palette.mesh_key_for_layer(instance.layer_index);
            if (mesh_key.empty()) continue;
            grouped[mesh_key].push_back(instance);
        }
    }
    return grouped;
}

} // namespace engine
