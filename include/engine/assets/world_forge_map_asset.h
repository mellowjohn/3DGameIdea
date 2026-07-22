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
enum class WorldForgeHydrologyKind : std::uint8_t { Lake, River, Sea };
enum class WorldForgeTravelRouteKind : std::uint8_t { Track, Road, Highway };

struct WorldForgeMapSoftGate {
    bool enabled = false;
    std::string notes;
};

struct WorldForgeWorldAnchor {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

/// Fixed Cartography backdrop AABB in world meters (optional). When set, the official map
/// plate uses this rect instead of fitting around marker content.
struct WorldForgeCartographyPlate {
    float center_x = 0.0f;
    float center_z = 0.0f;
    float width_meters = 4000.0f;
    float height_meters = 4000.0f;
};

struct WorldForgeMapPoint2 {
    float x = 0.0f;
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
    /// Campaign act membership (`act0`..`act4`). Empty = campaign-wide. See DEC-0036.
    std::vector<std::string> acts;
    std::vector<std::string> tags;
    WorldForgeMapSoftGate soft_gate;
    std::optional<WorldForgeWorldAnchor> anchor;
    /// Optional political / region outline in world XZ (open or closed polyline).
    std::vector<WorldForgeMapPoint2> border;
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
    /// Campaign act membership (`act0`..`act4`). Empty = campaign-wide. See DEC-0036.
    std::vector<std::string> acts;
    std::vector<std::string> tags;
    std::optional<WorldForgeWorldAnchor> anchor;
    std::vector<std::string> open_questions;
};

struct WorldForgeHydrologyRegion {
    std::string id;
    WorldForgeHydrologyKind kind = WorldForgeHydrologyKind::Lake;
    float min_x = 0.0f;
    float max_x = 0.0f;
    float min_z = 0.0f;
    float max_z = 0.0f;
    std::vector<std::string> acts;
    std::string summary;
};

struct WorldForgeFerryRoute {
    std::string id;
    std::string from_poi_id;
    std::string to_poi_id;
    std::vector<WorldForgeMapPoint2> points;
    /// Campaign act membership (`act0`..`act4`). Empty = campaign-wide. See DEC-0036.
    std::vector<std::string> acts;
    std::string summary;
};

/// Authored land travel geometry (track / road / highway). Narrative adjacency stays in `links[]`.
struct WorldForgeTravelRoute {
    std::string id;
    WorldForgeTravelRouteKind kind = WorldForgeTravelRouteKind::Road;
    std::string from_poi_id;
    std::string to_poi_id;
    std::vector<WorldForgeMapPoint2> points;
    /// Campaign act membership (`act0`..`act4`). Empty = campaign-wide. See DEC-0036.
    std::vector<std::string> acts;
    std::string summary;
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
    /// When set, Cartography official-map backdrop locks to this world-meter plate.
    std::optional<WorldForgeCartographyPlate> cartography_plate;
    std::vector<WorldForgeRegion> regions;
    std::vector<WorldForgePoi> pois;
    std::vector<WorldForgeMapLink> links;
    std::vector<WorldForgeHydrologyRegion> hydrology_regions;
    std::vector<WorldForgeFerryRoute> ferry_routes;
    std::vector<WorldForgeTravelRoute> travel_routes;

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
[[nodiscard]] const char* to_string(WorldForgeHydrologyKind value) noexcept;
[[nodiscard]] const char* to_string(WorldForgeTravelRouteKind value) noexcept;

[[nodiscard]] std::filesystem::path default_world_forge_map_path(const std::filesystem::path& project_root);

/// True when width/height are finite and positive.
[[nodiscard]] bool cartography_plate_valid(const WorldForgeCartographyPlate& plate) noexcept;

/// Uniformly scale all authored map XZ geometry about (center_x, center_z). Anchor Y unchanged.
void scale_map_xz_about(WorldForgeMapAsset& asset, float center_x, float center_z, float scale);

/// Content AABB of anchors / borders / hydrology / route points. Returns false if empty.
[[nodiscard]] bool compute_map_xz_content_bounds(const WorldForgeMapAsset& asset, float& out_min_x,
    float& out_max_x, float& out_min_z, float& out_max_z);

/// Build a plate centered on content (or origin), sized to `width_meters` × aspect-matched height,
/// then scale content so it fills the plate with `pad` (default 1.35 matches Cartography fit).
/// Writes `asset.cartography_plate`. Returns the applied uniform scale (1 if nothing to scale).
float apply_cartography_plate_and_rescale(WorldForgeMapAsset& asset, float width_meters,
    float map_aspect = 16.0f / 9.0f, float pad = 1.35f);

} // namespace engine
