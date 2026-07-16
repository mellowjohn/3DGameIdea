#pragma once

#include "engine/core/result.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace engine {

enum class WorldForgeMapCanonStatus : std::uint8_t { Established, Draft, Proposal, Open };
enum class WorldForgeRegionKind : std::uint8_t {
    Region,
    Fortress,
    City,
    Wilderness,
    Chaotic,
    Settlement,
    Other
};
enum class WorldForgePoiKind : std::uint8_t { Landmark, Settlement, Gate, Shrine, Camp, Other };
enum class WorldForgeMapLinkKind : std::uint8_t { Travel, SoftGate, StoryGate, Adjacency };
enum class WorldForgeMapEndpointKind : std::uint8_t { Region, Poi };

struct WorldForgeMapSoftGate {
    bool enabled = false;
    std::string notes;
};

struct WorldForgeWorldAnchor {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct WorldForgeRegion {
    std::string id;
    WorldForgeRegionKind kind = WorldForgeRegionKind::Region;
    std::string display_name;
    WorldForgeMapCanonStatus canon_status = WorldForgeMapCanonStatus::Draft;
    std::string summary;
    std::string story_ref;
    std::string parent_region_id;
    std::vector<std::string> faction_ids;
    std::vector<std::string> tags;
    WorldForgeMapSoftGate soft_gate;
    std::optional<WorldForgeWorldAnchor> anchor;
    std::vector<std::string> open_questions;
};

struct WorldForgePoi {
    std::string id;
    WorldForgePoiKind kind = WorldForgePoiKind::Landmark;
    std::string display_name;
    WorldForgeMapCanonStatus canon_status = WorldForgeMapCanonStatus::Draft;
    std::string region_id;
    std::string summary;
    std::string story_ref;
    std::string scene_entity_id;
    std::string prefab_id;
    std::vector<std::string> tags;
    std::optional<WorldForgeWorldAnchor> anchor;
    std::vector<std::string> open_questions;
};

struct WorldForgeMapLink {
    std::string id;
    WorldForgeMapLinkKind kind = WorldForgeMapLinkKind::Travel;
    WorldForgeMapEndpointKind from_kind = WorldForgeMapEndpointKind::Region;
    std::string from_id;
    WorldForgeMapEndpointKind to_kind = WorldForgeMapEndpointKind::Region;
    std::string to_id;
    WorldForgeMapCanonStatus canon_status = WorldForgeMapCanonStatus::Draft;
    bool bidirectional = true;
    WorldForgeMapSoftGate soft_gate;
    std::string summary;
    std::string story_ref;
    std::vector<std::string> open_questions;
};

struct WorldForgeMapAsset {
    int schema_version = 1;
    std::string id;
    std::vector<WorldForgeRegion> regions;
    std::vector<WorldForgePoi> pois;
    std::vector<WorldForgeMapLink> links;

    [[nodiscard]] Result<void> validate() const;
    [[nodiscard]] Result<void> validate_faction_refs(const std::unordered_set<std::string>& known_faction_ids) const;
    [[nodiscard]] static Result<WorldForgeMapAsset> load(const std::filesystem::path& path);
    [[nodiscard]] static Result<WorldForgeMapAsset> parse(const std::string& text,
        const std::string& source_name = "map.worldforge.json");
    [[nodiscard]] Result<void> save_atomic(const std::filesystem::path& path) const;
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static Result<void> validate_file(const std::filesystem::path& path);
    [[nodiscard]] static Result<void> validate_file(const std::filesystem::path& path,
        const std::unordered_set<std::string>& known_faction_ids);
};

[[nodiscard]] const char* to_string(WorldForgeMapCanonStatus value) noexcept;
[[nodiscard]] const char* to_string(WorldForgeRegionKind value) noexcept;
[[nodiscard]] const char* to_string(WorldForgePoiKind value) noexcept;
[[nodiscard]] const char* to_string(WorldForgeMapLinkKind value) noexcept;
[[nodiscard]] const char* to_string(WorldForgeMapEndpointKind value) noexcept;

[[nodiscard]] std::filesystem::path default_world_forge_map_path(const std::filesystem::path& project_root);

} // namespace engine
