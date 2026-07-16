#pragma once
#include "engine/world/world_partition.h"
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
namespace engine {
struct TerrainTileMetadata { CellCoord coordinate; std::uint32_t height_resolution=129; float minimum_height=0, maximum_height=0; std::string height_asset, foliage_asset; [[nodiscard]] Result<void> validate() const; };
[[nodiscard]] Result<std::set<CellCoord>> simulation_bubble(CellCoord center, std::uint32_t radius);
class CellStateStore final { public: void set(CellCoord,std::string,std::string); [[nodiscard]] std::optional<std::string> get(CellCoord,const std::string&) const; [[nodiscard]] std::string to_json() const; [[nodiscard]] static Result<CellStateStore> from_json(const std::string&); [[nodiscard]] static Result<CellStateStore> load(const std::filesystem::path&); [[nodiscard]] Result<void> save_atomic(const std::filesystem::path&) const; private: std::map<CellCoord,std::map<std::string,std::string>> states_; };
}
