#pragma once

#include "engine/core/result.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace engine {

enum class WorldForgeRelationshipNodeKind : std::uint8_t { Person, Deity, Artifact, Organization };
enum class WorldForgeRelationshipCanonStatus : std::uint8_t { Established, Draft, Proposal, Open };
enum class WorldForgeRelationshipEndpointTarget : std::uint8_t { Node, Faction };
enum class WorldForgeRelationshipEdgeKind : std::uint8_t {
    Ally,
    Rival,
    MemberOf,
    Leads,
    Kin,
    Serves,
    Opposes,
    Influences,
    Related
};

struct WorldForgeRelationshipNode {
    std::string id;
    WorldForgeRelationshipNodeKind kind = WorldForgeRelationshipNodeKind::Person;
    std::string display_name;
    WorldForgeRelationshipCanonStatus canon_status = WorldForgeRelationshipCanonStatus::Draft;
    std::string summary;
    std::string story_ref;
    std::vector<std::string> tags;
    std::vector<std::string> open_questions;
};

struct WorldForgeRelationshipEndpoint {
    WorldForgeRelationshipEndpointTarget target = WorldForgeRelationshipEndpointTarget::Node;
    std::string id;
};

struct WorldForgeRelationshipEdge {
    std::string id;
    WorldForgeRelationshipEndpoint from;
    WorldForgeRelationshipEndpoint to;
    WorldForgeRelationshipEdgeKind kind = WorldForgeRelationshipEdgeKind::Related;
    WorldForgeRelationshipCanonStatus canon_status = WorldForgeRelationshipCanonStatus::Draft;
    bool bidirectional = false;
    std::string summary;
    std::string story_ref;
    std::vector<std::string> open_questions;
    /// Hostility fallout weight (DEC-0029). Primary +D applies -D * transfer to the other faction
    /// when both endpoints are factions and kind is rival/opposes. 0 = no fallout.
    double standing_transfer = 0.0;
};

struct WorldForgeRelationshipsAsset {
    int schema_version = 1;
    std::string id;
    std::vector<WorldForgeRelationshipNode> nodes;
    std::vector<WorldForgeRelationshipEdge> edges;

    [[nodiscard]] Result<void> validate() const;
    /// When `known_faction_ids` is non-empty, every edge endpoint with target `faction` must be listed.
    [[nodiscard]] Result<void> validate_faction_refs(const std::unordered_set<std::string>& known_faction_ids) const;
    [[nodiscard]] static Result<WorldForgeRelationshipsAsset> load(const std::filesystem::path& path);
    [[nodiscard]] static Result<WorldForgeRelationshipsAsset> parse(const std::string& text,
        const std::string& source_name = "relationships.worldforge.json");
    [[nodiscard]] Result<void> save_atomic(const std::filesystem::path& path) const;
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static Result<void> validate_file(const std::filesystem::path& path);
    [[nodiscard]] static Result<void> validate_file(const std::filesystem::path& path,
        const std::unordered_set<std::string>& known_faction_ids);
};

[[nodiscard]] const char* to_string(WorldForgeRelationshipNodeKind value) noexcept;
[[nodiscard]] const char* to_string(WorldForgeRelationshipCanonStatus value) noexcept;
[[nodiscard]] const char* to_string(WorldForgeRelationshipEndpointTarget value) noexcept;
[[nodiscard]] const char* to_string(WorldForgeRelationshipEdgeKind value) noexcept;

[[nodiscard]] std::filesystem::path default_world_forge_relationships_path(const std::filesystem::path& project_root);

} // namespace engine
