#include "engine/assets/world_forge_map_asset.h"
#include "engine/assets/world_forge_acts.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace engine {
namespace {

EngineError map_error(std::string code, ErrorCategory category, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, category, "world_forge_map", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

Result<WorldForgeMapCanonStatus> parse_canon_status(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "established") return Result<WorldForgeMapCanonStatus>::success(WorldForgeMapCanonStatus::Established);
    if (key == "draft") return Result<WorldForgeMapCanonStatus>::success(WorldForgeMapCanonStatus::Draft);
    if (key == "proposal") return Result<WorldForgeMapCanonStatus>::success(WorldForgeMapCanonStatus::Proposal);
    if (key == "open") return Result<WorldForgeMapCanonStatus>::success(WorldForgeMapCanonStatus::Open);
    return Result<WorldForgeMapCanonStatus>::failure(map_error("WORLD-FORGE-MAP-CANON", ErrorCategory::Validation,
        "Unsupported canonStatus: " + raw, "Use established, draft, proposal, or open."));
}

Result<WorldForgeRegionKind> parse_region_kind(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "region") return Result<WorldForgeRegionKind>::success(WorldForgeRegionKind::Region);
    if (key == "fortress") return Result<WorldForgeRegionKind>::success(WorldForgeRegionKind::Fortress);
    if (key == "city") return Result<WorldForgeRegionKind>::success(WorldForgeRegionKind::City);
    if (key == "wilderness") return Result<WorldForgeRegionKind>::success(WorldForgeRegionKind::Wilderness);
    if (key == "chaotic") return Result<WorldForgeRegionKind>::success(WorldForgeRegionKind::Chaotic);
    if (key == "settlement") return Result<WorldForgeRegionKind>::success(WorldForgeRegionKind::Settlement);
    if (key == "other") return Result<WorldForgeRegionKind>::success(WorldForgeRegionKind::Other);
    return Result<WorldForgeRegionKind>::failure(map_error("WORLD-FORGE-MAP-REGION-KIND", ErrorCategory::Validation,
        "Unsupported region kind: " + raw,
        "Use region, fortress, city, wilderness, chaotic, settlement, or other."));
}

Result<WorldForgePoiKind> parse_poi_kind(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "landmark") return Result<WorldForgePoiKind>::success(WorldForgePoiKind::Landmark);
    if (key == "settlement") return Result<WorldForgePoiKind>::success(WorldForgePoiKind::Settlement);
    if (key == "gate") return Result<WorldForgePoiKind>::success(WorldForgePoiKind::Gate);
    if (key == "shrine") return Result<WorldForgePoiKind>::success(WorldForgePoiKind::Shrine);
    if (key == "camp") return Result<WorldForgePoiKind>::success(WorldForgePoiKind::Camp);
    if (key == "other") return Result<WorldForgePoiKind>::success(WorldForgePoiKind::Other);
    return Result<WorldForgePoiKind>::failure(map_error("WORLD-FORGE-MAP-POI-KIND", ErrorCategory::Validation,
        "Unsupported POI kind: " + raw, "Use landmark, settlement, gate, shrine, camp, or other."));
}

Result<WorldForgeMapLinkKind> parse_link_kind(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "travel") return Result<WorldForgeMapLinkKind>::success(WorldForgeMapLinkKind::Travel);
    if (key == "soft_gate" || key == "softgate")
        return Result<WorldForgeMapLinkKind>::success(WorldForgeMapLinkKind::SoftGate);
    if (key == "story_gate" || key == "storygate")
        return Result<WorldForgeMapLinkKind>::success(WorldForgeMapLinkKind::StoryGate);
    if (key == "adjacency") return Result<WorldForgeMapLinkKind>::success(WorldForgeMapLinkKind::Adjacency);
    return Result<WorldForgeMapLinkKind>::failure(map_error("WORLD-FORGE-MAP-LINK-KIND", ErrorCategory::Validation,
        "Unsupported map link kind: " + raw, "Use travel, soft_gate, story_gate, or adjacency."));
}

Result<WorldForgeMapEndpointKind> parse_endpoint_kind(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "region") return Result<WorldForgeMapEndpointKind>::success(WorldForgeMapEndpointKind::Region);
    if (key == "poi") return Result<WorldForgeMapEndpointKind>::success(WorldForgeMapEndpointKind::Poi);
    return Result<WorldForgeMapEndpointKind>::failure(map_error("WORLD-FORGE-MAP-ENDPOINT", ErrorCategory::Validation,
        "Unsupported map endpoint kind: " + raw, "Use region or poi."));
}

Result<WorldForgeHydrologyKind> parse_hydrology_kind(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "lake") return Result<WorldForgeHydrologyKind>::success(WorldForgeHydrologyKind::Lake);
    if (key == "river") return Result<WorldForgeHydrologyKind>::success(WorldForgeHydrologyKind::River);
    if (key == "sea") return Result<WorldForgeHydrologyKind>::success(WorldForgeHydrologyKind::Sea);
    return Result<WorldForgeHydrologyKind>::failure(map_error("WORLD-FORGE-MAP-HYDRO-KIND", ErrorCategory::Validation,
        "Unsupported hydrology kind: " + raw, "Use lake, river, or sea."));
}

std::vector<std::string> read_string_array(const nlohmann::json& node) {
    std::vector<std::string> out;
    if (!node.is_array()) return out;
    for (const auto& entry : node) {
        if (entry.is_string()) out.push_back(entry.get<std::string>());
    }
    return out;
}

nlohmann::json write_string_array(const std::vector<std::string>& values) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& value : values) out.push_back(value);
    return out;
}

WorldForgeMapSoftGate read_soft_gate(const nlohmann::json& node) {
    WorldForgeMapSoftGate gate;
    if (!node.is_object()) return gate;
    gate.enabled = node.value("enabled", false);
    gate.notes = node.value("notes", std::string{});
    return gate;
}

nlohmann::json write_soft_gate(const WorldForgeMapSoftGate& gate) {
    return nlohmann::ordered_json{{"enabled", gate.enabled}, {"notes", gate.notes}};
}

std::optional<WorldForgeWorldAnchor> read_anchor(const nlohmann::json& node) {
    if (!node.is_object()) return std::nullopt;
    WorldForgeWorldAnchor anchor;
    anchor.x = node.value("x", 0.0f);
    anchor.y = node.value("y", 0.0f);
    anchor.z = node.value("z", 0.0f);
    return anchor;
}

nlohmann::json write_anchor(const WorldForgeWorldAnchor& anchor) {
    return nlohmann::ordered_json{{"x", anchor.x}, {"y", anchor.y}, {"z", anchor.z}};
}

bool endpoint_exists(WorldForgeMapEndpointKind kind, const std::string& id,
    const std::unordered_set<std::string>& region_ids, const std::unordered_set<std::string>& poi_ids) {
    if (kind == WorldForgeMapEndpointKind::Region) return region_ids.find(id) != region_ids.end();
    return poi_ids.find(id) != poi_ids.end();
}

} // namespace

const char* to_string(WorldForgeMapCanonStatus value) noexcept {
    switch (value) {
    case WorldForgeMapCanonStatus::Established: return "established";
    case WorldForgeMapCanonStatus::Draft: return "draft";
    case WorldForgeMapCanonStatus::Proposal: return "proposal";
    case WorldForgeMapCanonStatus::Open: return "open";
    }
    return "draft";
}

const char* to_string(WorldForgeRegionKind value) noexcept {
    switch (value) {
    case WorldForgeRegionKind::Region: return "region";
    case WorldForgeRegionKind::Fortress: return "fortress";
    case WorldForgeRegionKind::City: return "city";
    case WorldForgeRegionKind::Wilderness: return "wilderness";
    case WorldForgeRegionKind::Chaotic: return "chaotic";
    case WorldForgeRegionKind::Settlement: return "settlement";
    case WorldForgeRegionKind::Other: return "other";
    }
    return "region";
}

const char* to_string(WorldForgePoiKind value) noexcept {
    switch (value) {
    case WorldForgePoiKind::Landmark: return "landmark";
    case WorldForgePoiKind::Settlement: return "settlement";
    case WorldForgePoiKind::Gate: return "gate";
    case WorldForgePoiKind::Shrine: return "shrine";
    case WorldForgePoiKind::Camp: return "camp";
    case WorldForgePoiKind::Other: return "other";
    }
    return "landmark";
}

const char* to_string(WorldForgeMapLinkKind value) noexcept {
    switch (value) {
    case WorldForgeMapLinkKind::Travel: return "travel";
    case WorldForgeMapLinkKind::SoftGate: return "soft_gate";
    case WorldForgeMapLinkKind::StoryGate: return "story_gate";
    case WorldForgeMapLinkKind::Adjacency: return "adjacency";
    }
    return "travel";
}

const char* to_string(WorldForgeMapEndpointKind value) noexcept {
    switch (value) {
    case WorldForgeMapEndpointKind::Region: return "region";
    case WorldForgeMapEndpointKind::Poi: return "poi";
    }
    return "region";
}

const char* to_string(WorldForgeHydrologyKind value) noexcept {
    switch (value) {
    case WorldForgeHydrologyKind::Lake: return "lake";
    case WorldForgeHydrologyKind::River: return "river";
    case WorldForgeHydrologyKind::Sea: return "sea";
    }
    return "lake";
}

std::filesystem::path default_world_forge_map_path(const std::filesystem::path& project_root) {
    return project_root / "assets" / "world-forge" / "map.worldforge.json";
}

Result<void> WorldForgeMapAsset::validate() const {
    if (schema_version != 1) {
        return Result<void>::failure(map_error("WORLD-FORGE-MAP-SCHEMA", ErrorCategory::Validation,
            "Only World Forge map schemaVersion 1 is supported", "Use schemaVersion 1."));
    }
    std::unordered_set<std::string> region_ids;
    region_ids.reserve(regions.size());
    for (const auto& region : regions) {
        if (region.id.empty()) {
            return Result<void>::failure(map_error("WORLD-FORGE-MAP-REGION-ID", ErrorCategory::Validation,
                "Region id is required", "Set a unique non-empty id for each region."));
        }
        if (!region_ids.insert(region.id).second) {
            return Result<void>::failure(map_error("WORLD-FORGE-MAP-REGION-ID-DUP", ErrorCategory::Validation,
                "Duplicate region id: " + region.id, "Ensure every region id is unique."));
        }
        if (const auto acts_ok = validate_world_forge_acts(region.acts, "region", region.id); !acts_ok) {
            return Result<void>::failure(acts_ok.error());
        }
    }
    for (const auto& region : regions) {
        if (region.parent_region_id.empty()) continue;
        if (region.parent_region_id == region.id) {
            return Result<void>::failure(map_error("WORLD-FORGE-MAP-PARENT", ErrorCategory::Validation,
                "Region parentRegionId cannot reference itself: " + region.id,
                "Point parentRegionId at a different region or leave it empty."));
        }
        if (region_ids.find(region.parent_region_id) == region_ids.end()) {
            return Result<void>::failure(map_error("WORLD-FORGE-MAP-PARENT", ErrorCategory::Validation,
                "Unknown parentRegionId '" + region.parent_region_id + "' on region '" + region.id + "'",
                "parentRegionId must match another region id in the same file, or be empty."));
        }
    }
    std::unordered_set<std::string> poi_ids;
    poi_ids.reserve(pois.size());
    for (const auto& poi : pois) {
        if (poi.id.empty()) {
            return Result<void>::failure(map_error("WORLD-FORGE-MAP-POI-ID", ErrorCategory::Validation,
                "POI id is required", "Set a unique non-empty id for each POI."));
        }
        if (!poi_ids.insert(poi.id).second) {
            return Result<void>::failure(map_error("WORLD-FORGE-MAP-POI-ID-DUP", ErrorCategory::Validation,
                "Duplicate POI id: " + poi.id, "Ensure every POI id is unique."));
        }
        if (poi.region_id.empty() || region_ids.find(poi.region_id) == region_ids.end()) {
            return Result<void>::failure(map_error("WORLD-FORGE-MAP-POI-REGION", ErrorCategory::Validation,
                "POI '" + poi.id + "' regionId must reference a region in this file",
                "Set regionId to an existing region id."));
        }
        if (const auto acts_ok = validate_world_forge_acts(poi.acts, "poi", poi.id); !acts_ok) {
            return Result<void>::failure(acts_ok.error());
        }
    }
    std::unordered_set<std::string> link_ids;
    link_ids.reserve(links.size());
    for (const auto& link : links) {
        if (link.id.empty()) {
            return Result<void>::failure(map_error("WORLD-FORGE-MAP-LINK-ID", ErrorCategory::Validation,
                "Map link id is required", "Set a unique non-empty id for each link."));
        }
        if (!link_ids.insert(link.id).second) {
            return Result<void>::failure(map_error("WORLD-FORGE-MAP-LINK-ID-DUP", ErrorCategory::Validation,
                "Duplicate map link id: " + link.id, "Ensure every link id is unique."));
        }
        if (link.from_id.empty() || link.to_id.empty()) {
            return Result<void>::failure(map_error("WORLD-FORGE-MAP-ENDPOINT", ErrorCategory::Validation,
                "Link endpoints require non-empty ids: " + link.id, "Set fromId and toId."));
        }
        if (!endpoint_exists(link.from_kind, link.from_id, region_ids, poi_ids)) {
            return Result<void>::failure(map_error("WORLD-FORGE-MAP-REF", ErrorCategory::Validation,
                "Unknown from endpoint '" + link.from_id + "' on link '" + link.id + "'",
                "fromId must match a region or POI id according to fromKind."));
        }
        if (!endpoint_exists(link.to_kind, link.to_id, region_ids, poi_ids)) {
            return Result<void>::failure(map_error("WORLD-FORGE-MAP-REF", ErrorCategory::Validation,
                "Unknown to endpoint '" + link.to_id + "' on link '" + link.id + "'",
                "toId must match a region or POI id according to toKind."));
        }
        if (link.from_kind == link.to_kind && link.from_id == link.to_id) {
            return Result<void>::failure(map_error("WORLD-FORGE-MAP-SELF", ErrorCategory::Validation,
                "Map link cannot connect an endpoint to itself: " + link.id, "Use two distinct endpoints."));
        }
    }
    return Result<void>::success();
}

Result<void> WorldForgeMapAsset::validate_faction_refs(const std::unordered_set<std::string>& known_faction_ids) const {
    if (known_faction_ids.empty()) return Result<void>::success();
    for (const auto& region : regions) {
        for (const auto& faction_id : region.faction_ids) {
            if (known_faction_ids.find(faction_id) == known_faction_ids.end()) {
                return Result<void>::failure(map_error("WORLD-FORGE-MAP-FACTION-REF", ErrorCategory::Validation,
                    "Unknown factionId '" + faction_id + "' on region '" + region.id + "'",
                    "Faction ids must match entities in factions.worldforge.json."));
            }
        }
    }
    return Result<void>::success();
}

Result<WorldForgeMapAsset> WorldForgeMapAsset::parse(const std::string& text, const std::string& source_name) {
    try {
        const auto json = nlohmann::json::parse(text);
        if (!json.is_object()) {
            return Result<WorldForgeMapAsset>::failure(map_error("WORLD-FORGE-MAP-ROOT", ErrorCategory::Serialization,
                source_name + " must be a JSON object",
                "Wrap regions/pois/links in an object with schemaVersion and id."));
        }
        WorldForgeMapAsset asset;
        asset.schema_version = json.value("schemaVersion", 0);
        if (asset.schema_version != 1) {
            return Result<WorldForgeMapAsset>::failure(map_error("WORLD-FORGE-MAP-SCHEMA", ErrorCategory::Validation,
                "Unsupported World Forge map schemaVersion", "Use schemaVersion 1."));
        }
        asset.id = json.value("id", std::string{});

        const auto regions = json.value("regions", nlohmann::json::array());
        if (!regions.is_array()) {
            return Result<WorldForgeMapAsset>::failure(map_error("WORLD-FORGE-MAP-REGIONS", ErrorCategory::Validation,
                "regions must be an array", "Provide a regions array."));
        }
        for (const auto& node : regions) {
            if (!node.is_object()) {
                return Result<WorldForgeMapAsset>::failure(map_error("WORLD-FORGE-MAP-REGION", ErrorCategory::Validation,
                    "Each region must be an object", "Fix region entries."));
            }
            WorldForgeRegion region;
            region.id = node.value("id", std::string{});
            if (region.id.empty()) {
                return Result<WorldForgeMapAsset>::failure(map_error("WORLD-FORGE-MAP-REGION-ID",
                    ErrorCategory::Validation, "Region id is required",
                    "Set a unique non-empty id for each region."));
            }
            const auto kind = parse_region_kind(node.value("kind", std::string{}));
            if (!kind) return Result<WorldForgeMapAsset>::failure(kind.error());
            region.kind = kind.value();
            region.display_name = node.value("displayName", std::string{});
            const auto canon = parse_canon_status(node.value("canonStatus", std::string{}));
            if (!canon) return Result<WorldForgeMapAsset>::failure(canon.error());
            region.canon_status = canon.value();
            region.summary = node.value("summary", std::string{});
            region.story_ref = node.value("storyRef", std::string{});
            region.parent_region_id = node.value("parentRegionId", std::string{});
            region.faction_ids = read_string_array(node.value("factionIds", nlohmann::json::array()));
            region.acts = read_string_array(node.value("acts", nlohmann::json::array()));
            region.tags = read_string_array(node.value("tags", nlohmann::json::array()));
            region.soft_gate = read_soft_gate(node.value("softGate", nlohmann::json::object()));
            if (node.contains("anchor") && !node["anchor"].is_null()) region.anchor = read_anchor(node["anchor"]);
            region.open_questions = read_string_array(node.value("openQuestions", nlohmann::json::array()));
            asset.regions.push_back(std::move(region));
        }

        const auto pois = json.value("pois", nlohmann::json::array());
        if (!pois.is_array()) {
            return Result<WorldForgeMapAsset>::failure(map_error("WORLD-FORGE-MAP-POIS", ErrorCategory::Validation,
                "pois must be an array", "Provide a pois array."));
        }
        for (const auto& node : pois) {
            if (!node.is_object()) {
                return Result<WorldForgeMapAsset>::failure(map_error("WORLD-FORGE-MAP-POI", ErrorCategory::Validation,
                    "Each POI must be an object", "Fix POI entries."));
            }
            WorldForgePoi poi;
            poi.id = node.value("id", std::string{});
            if (poi.id.empty()) {
                return Result<WorldForgeMapAsset>::failure(map_error("WORLD-FORGE-MAP-POI-ID", ErrorCategory::Validation,
                    "POI id is required", "Set a unique non-empty id for each POI."));
            }
            const auto kind = parse_poi_kind(node.value("kind", std::string{}));
            if (!kind) return Result<WorldForgeMapAsset>::failure(kind.error());
            poi.kind = kind.value();
            poi.display_name = node.value("displayName", std::string{});
            const auto canon = parse_canon_status(node.value("canonStatus", std::string{}));
            if (!canon) return Result<WorldForgeMapAsset>::failure(canon.error());
            poi.canon_status = canon.value();
            poi.region_id = node.value("regionId", std::string{});
            poi.summary = node.value("summary", std::string{});
            poi.story_ref = node.value("storyRef", std::string{});
            poi.scene_entity_id = node.value("sceneEntityId", std::string{});
            poi.prefab_id = node.value("prefabId", std::string{});
            poi.acts = read_string_array(node.value("acts", nlohmann::json::array()));
            poi.tags = read_string_array(node.value("tags", nlohmann::json::array()));
            if (node.contains("anchor") && !node["anchor"].is_null()) poi.anchor = read_anchor(node["anchor"]);
            poi.open_questions = read_string_array(node.value("openQuestions", nlohmann::json::array()));
            asset.pois.push_back(std::move(poi));
        }

        const auto links = json.value("links", nlohmann::json::array());
        if (!links.is_array()) {
            return Result<WorldForgeMapAsset>::failure(map_error("WORLD-FORGE-MAP-LINKS", ErrorCategory::Validation,
                "links must be an array", "Provide a links array."));
        }
        for (const auto& node : links) {
            if (!node.is_object()) {
                return Result<WorldForgeMapAsset>::failure(map_error("WORLD-FORGE-MAP-LINK", ErrorCategory::Validation,
                    "Each link must be an object", "Fix link entries."));
            }
            WorldForgeMapLink link;
            link.id = node.value("id", std::string{});
            if (link.id.empty()) {
                return Result<WorldForgeMapAsset>::failure(map_error("WORLD-FORGE-MAP-LINK-ID", ErrorCategory::Validation,
                    "Map link id is required", "Set a unique non-empty id for each link."));
            }
            const auto kind = parse_link_kind(node.value("kind", std::string{}));
            if (!kind) return Result<WorldForgeMapAsset>::failure(kind.error());
            link.kind = kind.value();
            const auto from_kind = parse_endpoint_kind(node.value("fromKind", std::string{}));
            if (!from_kind) return Result<WorldForgeMapAsset>::failure(from_kind.error());
            link.from_kind = from_kind.value();
            link.from_id = node.value("fromId", std::string{});
            const auto to_kind = parse_endpoint_kind(node.value("toKind", std::string{}));
            if (!to_kind) return Result<WorldForgeMapAsset>::failure(to_kind.error());
            link.to_kind = to_kind.value();
            link.to_id = node.value("toId", std::string{});
            const auto canon = parse_canon_status(node.value("canonStatus", std::string{}));
            if (!canon) return Result<WorldForgeMapAsset>::failure(canon.error());
            link.canon_status = canon.value();
            link.bidirectional = node.value("bidirectional", true);
            link.soft_gate = read_soft_gate(node.value("softGate", nlohmann::json::object()));
            link.summary = node.value("summary", std::string{});
            link.story_ref = node.value("storyRef", std::string{});
            link.open_questions = read_string_array(node.value("openQuestions", nlohmann::json::array()));
            asset.links.push_back(std::move(link));
        }

        if (json.contains("hydrologyRegions")) {
            const auto hydrology = json.at("hydrologyRegions");
            if (!hydrology.is_array()) {
                return Result<WorldForgeMapAsset>::failure(map_error("WORLD-FORGE-MAP-HYDRO", ErrorCategory::Validation,
                    "hydrologyRegions must be an array", "Provide a hydrologyRegions array."));
            }
            for (const auto& node : hydrology) {
                if (!node.is_object()) continue;
                WorldForgeHydrologyRegion region;
                region.id = node.value("id", std::string{});
                const auto kind = parse_hydrology_kind(node.value("kind", std::string{"lake"}));
                if (!kind) return Result<WorldForgeMapAsset>::failure(kind.error());
                region.kind = kind.value();
                region.min_x = node.value("minX", 0.0f);
                region.max_x = node.value("maxX", 0.0f);
                region.min_z = node.value("minZ", 0.0f);
                region.max_z = node.value("maxZ", 0.0f);
                region.acts = read_string_array(node.value("acts", nlohmann::json::array()));
                region.summary = node.value("summary", std::string{});
                asset.hydrology_regions.push_back(std::move(region));
            }
        }

        if (json.contains("ferryRoutes")) {
            const auto routes = json.at("ferryRoutes");
            if (!routes.is_array()) {
                return Result<WorldForgeMapAsset>::failure(map_error("WORLD-FORGE-MAP-FERRY", ErrorCategory::Validation,
                    "ferryRoutes must be an array", "Provide a ferryRoutes array."));
            }
            for (const auto& node : routes) {
                if (!node.is_object()) continue;
                WorldForgeFerryRoute route;
                route.id = node.value("id", std::string{});
                route.from_poi_id = node.value("fromPoiId", std::string{});
                route.to_poi_id = node.value("toPoiId", std::string{});
                route.summary = node.value("summary", std::string{});
                if (node.contains("points") && node.at("points").is_array()) {
                    for (const auto& point : node.at("points")) {
                        if (!point.is_object()) continue;
                        WorldForgeFerryRoutePoint entry;
                        entry.x = point.value("x", 0.0f);
                        entry.z = point.value("z", 0.0f);
                        route.points.push_back(entry);
                    }
                }
                asset.ferry_routes.push_back(std::move(route));
            }
        }

        if (const auto valid = asset.validate(); !valid) {
            return Result<WorldForgeMapAsset>::failure(valid.error());
        }
        return Result<WorldForgeMapAsset>::success(std::move(asset));
    } catch (const std::exception& exception) {
        auto error = map_error("WORLD-FORGE-MAP-PARSE", ErrorCategory::Serialization,
            "Failed to parse " + source_name, "Fix JSON syntax.");
        error.causes.push_back(exception.what());
        return Result<WorldForgeMapAsset>::failure(std::move(error));
    }
}

Result<WorldForgeMapAsset> WorldForgeMapAsset::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Result<WorldForgeMapAsset>::failure(map_error("WORLD-FORGE-MAP-READ", ErrorCategory::Io,
            "Could not read World Forge map: " + path.generic_string(), "Check the path and file permissions."));
    }
    std::ostringstream text;
    text << input.rdbuf();
    return parse(text.str(), path.filename().string());
}

std::string WorldForgeMapAsset::to_json() const {
    nlohmann::ordered_json json;
    json["schemaVersion"] = schema_version;
    json["id"] = id;
    auto regions_json = nlohmann::ordered_json::array();
    for (const auto& region : regions) {
        nlohmann::ordered_json node;
        node["id"] = region.id;
        node["kind"] = to_string(region.kind);
        node["displayName"] = region.display_name;
        node["canonStatus"] = to_string(region.canon_status);
        node["summary"] = region.summary;
        node["storyRef"] = region.story_ref;
        node["parentRegionId"] = region.parent_region_id;
        node["factionIds"] = write_string_array(region.faction_ids);
        node["acts"] = write_string_array(region.acts);
        node["tags"] = write_string_array(region.tags);
        node["softGate"] = write_soft_gate(region.soft_gate);
        if (region.anchor) node["anchor"] = write_anchor(*region.anchor);
        node["openQuestions"] = write_string_array(region.open_questions);
        regions_json.push_back(std::move(node));
    }
    json["regions"] = std::move(regions_json);
    auto pois_json = nlohmann::ordered_json::array();
    for (const auto& poi : pois) {
        nlohmann::ordered_json node;
        node["id"] = poi.id;
        node["kind"] = to_string(poi.kind);
        node["displayName"] = poi.display_name;
        node["canonStatus"] = to_string(poi.canon_status);
        node["regionId"] = poi.region_id;
        node["summary"] = poi.summary;
        node["storyRef"] = poi.story_ref;
        node["sceneEntityId"] = poi.scene_entity_id;
        node["prefabId"] = poi.prefab_id;
        node["acts"] = write_string_array(poi.acts);
        node["tags"] = write_string_array(poi.tags);
        if (poi.anchor) node["anchor"] = write_anchor(*poi.anchor);
        node["openQuestions"] = write_string_array(poi.open_questions);
        pois_json.push_back(std::move(node));
    }
    json["pois"] = std::move(pois_json);
    auto links_json = nlohmann::ordered_json::array();
    for (const auto& link : links) {
        nlohmann::ordered_json node;
        node["id"] = link.id;
        node["kind"] = to_string(link.kind);
        node["fromKind"] = to_string(link.from_kind);
        node["fromId"] = link.from_id;
        node["toKind"] = to_string(link.to_kind);
        node["toId"] = link.to_id;
        node["canonStatus"] = to_string(link.canon_status);
        node["bidirectional"] = link.bidirectional;
        node["softGate"] = write_soft_gate(link.soft_gate);
        node["summary"] = link.summary;
        node["storyRef"] = link.story_ref;
        node["openQuestions"] = write_string_array(link.open_questions);
        links_json.push_back(std::move(node));
    }
    json["links"] = std::move(links_json);
    auto hydrology_json = nlohmann::ordered_json::array();
    for (const auto& region : hydrology_regions) {
        nlohmann::ordered_json node;
        node["id"] = region.id;
        node["kind"] = to_string(region.kind);
        node["minX"] = region.min_x;
        node["maxX"] = region.max_x;
        node["minZ"] = region.min_z;
        node["maxZ"] = region.max_z;
        node["acts"] = write_string_array(region.acts);
        node["summary"] = region.summary;
        hydrology_json.push_back(std::move(node));
    }
    json["hydrologyRegions"] = std::move(hydrology_json);
    auto ferry_json = nlohmann::ordered_json::array();
    for (const auto& route : ferry_routes) {
        nlohmann::ordered_json node;
        node["id"] = route.id;
        node["fromPoiId"] = route.from_poi_id;
        node["toPoiId"] = route.to_poi_id;
        node["summary"] = route.summary;
        auto points_json = nlohmann::ordered_json::array();
        for (const auto& point : route.points) points_json.push_back({{"x", point.x}, {"z", point.z}});
        node["points"] = std::move(points_json);
        ferry_json.push_back(std::move(node));
    }
    json["ferryRoutes"] = std::move(ferry_json);
    return json.dump(2) + "\n";
}

Result<void> WorldForgeMapAsset::save_atomic(const std::filesystem::path& path) const {
    const auto valid = validate();
    if (!valid) return Result<void>::failure(valid.error());
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    const auto temporary = path.string() + ".tmp";
    const auto backup = path.string() + ".bak";
    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) {
            return Result<void>::failure(map_error("WORLD-FORGE-MAP-IO", ErrorCategory::Io,
                "Could not write World Forge map: " + path.generic_string(),
                "Check file permissions and disk space."));
        }
        output << to_json();
    }
    std::error_code ignored;
    if (std::filesystem::exists(path)) {
        std::filesystem::copy_file(path, backup, std::filesystem::copy_options::overwrite_existing, ignored);
    }
    std::filesystem::rename(temporary, path, ignored);
    if (ignored) {
        return Result<void>::failure(map_error("WORLD-FORGE-MAP-IO", ErrorCategory::Io,
            "Could not replace World Forge map: " + path.generic_string(),
            "Check file permissions and disk space."));
    }
    if (std::filesystem::exists(backup)) std::filesystem::remove(backup, ignored);
    return Result<void>::success();
}

Result<void> WorldForgeMapAsset::validate_file(const std::filesystem::path& path) {
    return validate_file(path, {});
}

Result<void> WorldForgeMapAsset::validate_file(const std::filesystem::path& path,
    const std::unordered_set<std::string>& known_faction_ids) {
    if (!std::filesystem::exists(path)) return Result<void>::success();
    const auto loaded = load(path);
    if (!loaded) return Result<void>::failure(loaded.error());
    return loaded.value().validate_faction_refs(known_faction_ids);
}

} // namespace engine
