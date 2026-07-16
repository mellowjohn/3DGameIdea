#include "engine/world/world_partition.h"

#include <cmath>
#include <limits>
#include <sstream>

namespace engine {
namespace {
EngineError partition_error(std::string code, std::string message) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Validation, "world-partition", std::move(message),
                       ENGINE_SOURCE_CONTEXT, {}, "Use finite coordinates inside the configured world bounds.", make_correlation_id()};
}
}

std::string CellCoord::str() const { return std::to_string(x) + "," + std::to_string(z); }

WorldPartition::WorldPartition(PartitionConfig config) : config_(config) {
    if (!(config_.cell_size > 0.0)) config_.cell_size = 128.0;
    if (!(config_.world_half_extent > 0.0)) config_.world_half_extent = 2000.0;
    if (!(config_.rebase_threshold > 0.0)) config_.rebase_threshold = 512.0;
}

Result<CellCoord> WorldPartition::cell_for(WorldPosition position) const {
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
        return Result<CellCoord>::failure(partition_error("WORLD-POSITION-NONFINITE", "World position contains NaN or infinity"));
    if (std::abs(position.x) > config_.world_half_extent || std::abs(position.z) > config_.world_half_extent)
        return Result<CellCoord>::failure(partition_error("WORLD-POSITION-OUTSIDE", "World position is outside the configured extent"));
    return Result<CellCoord>::success(CellCoord{static_cast<std::int32_t>(std::floor(position.x / config_.cell_size)),
                                                static_cast<std::int32_t>(std::floor(position.z / config_.cell_size))});
}

WorldPosition WorldPartition::cell_origin(CellCoord cell) const {
    return {static_cast<double>(cell.x) * config_.cell_size, 0.0, static_cast<double>(cell.z) * config_.cell_size};
}

LocalPosition WorldPartition::to_local(WorldPosition position) const {
    return {static_cast<float>(position.x - origin_.x), static_cast<float>(position.y - origin_.y), static_cast<float>(position.z - origin_.z)};
}

bool WorldPartition::rebase_if_needed(WorldPosition focus) {
    const double dx = focus.x - origin_.x, dz = focus.z - origin_.z;
    if (dx * dx + dz * dz < config_.rebase_threshold * config_.rebase_threshold) return false;
    const auto cell = cell_for(focus);
    if (!cell) return false;
    origin_ = cell_origin(cell.value());
    origin_.y = focus.y;
    return true;
}

} // namespace engine

