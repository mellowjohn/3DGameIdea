#include "engine/world/foliage_scatter.h"

#include "engine/world/terrain.h"

#include <DirectXMath.h>

#include <algorithm>
#include <cmath>

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

float estimate_slope_ratio(float world_x, float world_z) {
    constexpr float sample_offset = 0.35f;
    const float center = sample_terrain_height(world_x, world_z);
    const float east = sample_terrain_height(world_x + sample_offset, world_z);
    const float north = sample_terrain_height(world_x, world_z + sample_offset);
    const float rise = std::max(std::abs(east - center), std::abs(north - center));
    return rise / sample_offset;
}

float distance_falloff(const std::array<float, 3>& camera_position, float world_x, float world_y, float world_z,
    const FoliageScatterConfig& config) {
    const float dx = world_x - camera_position[0];
    const float dy = world_y - camera_position[1];
    const float dz = world_z - camera_position[2];
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (distance <= config.distance_falloff_start) return 1.0f;
    if (distance >= config.distance_falloff_end) return 0.0f;
    const float range = config.distance_falloff_end - config.distance_falloff_start;
    return std::clamp(1.0f - (distance - config.distance_falloff_start) / range, 0.0f, 1.0f);
}

void store_matrix(std::array<float, 16>& out, float x, float y, float z, float yaw, float scale) {
    const float sin_yaw = std::sin(yaw);
    const float cos_yaw = std::cos(yaw);
    XMMATRIX matrix = XMMatrixScaling(scale, scale, scale) * XMMatrixRotationY(yaw) * XMMatrixTranslation(x, y, z);
    XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(out.data()), matrix);
}

} // namespace

std::vector<FoliageInstance> scatter_foliage_cell(CellCoord cell, const FoliageDensityStore& density,
    const FoliageLayerPalette& palette, const FoliageScatterConfig& config,
    const std::array<float, 3>& camera_position) {
    std::vector<FoliageInstance> instances;
    const auto* entry = density.find_cell(cell);
    if (!entry) return instances;
    const float origin_x = static_cast<float>(cell.x) * FoliageDensityStore::k_cell_size - FoliageDensityStore::k_cell_size * 0.5f;
    const float origin_z = static_cast<float>(cell.z) * FoliageDensityStore::k_cell_size - FoliageDensityStore::k_cell_size * 0.5f;
    const float step = FoliageDensityStore::k_cell_size / static_cast<float>(FoliageDensityStore::k_resolution - 1);
    instances.reserve(256);
    for (std::uint32_t z = 0; z < FoliageDensityStore::k_resolution; ++z) {
        for (std::uint32_t x = 0; x < FoliageDensityStore::k_resolution; ++x) {
            const std::size_t index = static_cast<std::size_t>(z) * FoliageDensityStore::k_resolution + x;
            const std::uint8_t sample_density = entry->density[index];
            const std::uint8_t layer_index = entry->layer[index];
            const auto* layer = palette.find_by_index(layer_index);
            if (!layer) continue;
            const float sample_x = origin_x + static_cast<float>(x) * step;
            const float sample_z = origin_z + static_cast<float>(z) * step;
            if (estimate_slope_ratio(sample_x, sample_z) > layer->max_slope_ratio) continue;
            const float ground_y = sample_terrain_height(sample_x, sample_z);
            const float distance_scale =
                distance_falloff(camera_position, sample_x, ground_y, sample_z, config);
            if (distance_scale <= 0.0f) continue;
            int target_count = 0;
            if (layer->scatter_mode == FoliageScatterMode::Discrete) {
                if (sample_density >= layer->discrete_min_density) target_count = 1;
            } else {
                if (sample_density < FoliageScatterConfig::k_min_density) continue;
                target_count = std::max(1, static_cast<int>(std::lround(
                    static_cast<float>(sample_density) * layer->density_multiplier * distance_scale)));
            }
            if (target_count <= 0) continue;
            for (int instance_index = 0; instance_index < target_count; ++instance_index) {
                if (instances.size() >= config.k_max_instances_per_cell) return instances;
                const std::uint32_t seed = hash_seed(cell, x, z, layer_index, static_cast<std::uint32_t>(instance_index));
                const float jitter_x = (hash_unit(seed ^ 0x11u) - 0.5f) * step * 0.85f;
                const float jitter_z = (hash_unit(seed ^ 0x22u) - 0.5f) * step * 0.85f;
                const float world_x = sample_x + jitter_x;
                const float world_z = sample_z + jitter_z;
                const float world_y = sample_terrain_height(world_x, world_z);
                const float yaw = hash_unit(seed ^ 0x33u) * 6.2831853f;
                const float scale = layer->scale_min +
                    (layer->scale_max - layer->scale_min) * hash_unit(seed ^ 0x44u);
                FoliageInstance instance;
                instance.layer_index = layer_index;
                store_matrix(instance.model, world_x, world_y, world_z, yaw, scale);
                instances.push_back(std::move(instance));
            }
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
