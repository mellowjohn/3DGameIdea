#include "engine/world/world_influence.h"

#include <cmath>
#include <limits>

namespace engine {

WorldInfluenceSource WorldInfluenceBus::dominant_at(float world_x, float world_z) const noexcept {
    WorldInfluenceSource best;
    float best_distance = std::numeric_limits<float>::max();
    for (const auto& source : sources_) {
        const float dx = source.position[0] - world_x;
        const float dz = source.position[2] - world_z;
        const float distance_sq = dx * dx + dz * dz;
        if (distance_sq < best_distance) {
            best_distance = distance_sq;
            best = source;
        }
    }
    return best;
}

} // namespace engine
