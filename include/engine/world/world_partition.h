#pragma once

#include "engine/core/result.h"

#include <compare>
#include <cstdint>
#include <string>

namespace engine {

struct WorldPosition { double x = 0.0, y = 0.0, z = 0.0; };
struct LocalPosition { float x = 0.0f, y = 0.0f, z = 0.0f; };
struct CellCoord {
    std::int32_t x = 0, z = 0;
    auto operator<=>(const CellCoord&) const = default;
    [[nodiscard]] std::string str() const;
};

struct PartitionConfig {
    double cell_size = 128.0;
    double world_half_extent = 2000.0;
    double rebase_threshold = 512.0;
};

class WorldPartition final {
public:
    explicit WorldPartition(PartitionConfig config = {});
    [[nodiscard]] Result<CellCoord> cell_for(WorldPosition position) const;
    [[nodiscard]] WorldPosition cell_origin(CellCoord cell) const;
    [[nodiscard]] LocalPosition to_local(WorldPosition position) const;
    [[nodiscard]] bool rebase_if_needed(WorldPosition focus);
    [[nodiscard]] WorldPosition origin() const noexcept { return origin_; }
    [[nodiscard]] const PartitionConfig& config() const noexcept { return config_; }
private:
    PartitionConfig config_;
    WorldPosition origin_{};
};

} // namespace engine

