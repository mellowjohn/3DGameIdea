#pragma once

#include <array>
#include <string>
#include <vector>

namespace engine {

struct WorldInfluenceSource {
    std::array<float, 3> position{};
    std::array<float, 3> velocity{};
    float radius = 1.2f;
    float strength = 1.0f;
    std::string kind;
};

class WorldInfluenceBus final {
public:
    void clear() noexcept { sources_.clear(); }
    void add(WorldInfluenceSource source) { sources_.push_back(std::move(source)); }

    [[nodiscard]] const std::vector<WorldInfluenceSource>& sources() const noexcept { return sources_; }
    [[nodiscard]] bool empty() const noexcept { return sources_.empty(); }

    [[nodiscard]] WorldInfluenceSource dominant_at(float world_x, float world_z) const noexcept;

private:
    std::vector<WorldInfluenceSource> sources_;
};

} // namespace engine
