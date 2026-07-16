#pragma once

#include "engine/core/result.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace engine {

enum class WorldForgeFactionKind : std::uint8_t { Faction, Culture, Clan, Warband };
enum class WorldForgeCanonStatus : std::uint8_t { Established, Draft, Proposal, Open };
enum class WorldForgePoliticalRole : std::uint8_t { Arena, Faction, Unknown };

/// Rank band for gates (DEC-0029). Highest matching minScore wins.
struct WorldForgeFactionStandingRank {
    std::string id;
    double min_score = 0.0;
    std::string display_name;
};

struct WorldForgeFactionStandingLockIn {
    double threshold = 0.0;
    std::vector<std::string> exclusive_faction_ids;
};

/// Optional player standing config on a faction entity (DEC-0029). Absent = not tracked.
struct WorldForgeFactionStandingConfig {
    bool tracks_player = false;
    double min_score = -100.0;
    double max_score = 100.0;
    std::vector<WorldForgeFactionStandingRank> ranks;
    std::optional<WorldForgeFactionStandingLockIn> lock_in;
};

struct WorldForgeFactionEntity {
    std::string id;
    WorldForgeFactionKind kind = WorldForgeFactionKind::Faction;
    std::string display_name;
    WorldForgeCanonStatus canon_status = WorldForgeCanonStatus::Draft;
    std::string summary;
    std::string story_ref;
    std::vector<std::string> tags;
    std::optional<WorldForgePoliticalRole> political_role;
    /// Empty means no parent. Non-empty must reference another entity id in the same asset.
    std::string parent_id;
    std::vector<std::string> open_questions;
    /// When set and tracks_player, StandingRuntime tracks a score for this faction.
    std::optional<WorldForgeFactionStandingConfig> standing;
};

struct WorldForgeFactionsAsset {
    int schema_version = 1;
    std::string id;
    std::vector<WorldForgeFactionEntity> entities;

    [[nodiscard]] Result<void> validate() const;
    [[nodiscard]] const WorldForgeFactionEntity* find_entity(const std::string& entity_id) const;
    [[nodiscard]] WorldForgeFactionEntity* find_entity(const std::string& entity_id);
    [[nodiscard]] static Result<WorldForgeFactionsAsset> load(const std::filesystem::path& path);
    [[nodiscard]] static Result<WorldForgeFactionsAsset> parse(const std::string& text,
        const std::string& source_name = "factions.worldforge.json");
    [[nodiscard]] Result<void> save_atomic(const std::filesystem::path& path) const;
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static Result<void> validate_file(const std::filesystem::path& path);
};

[[nodiscard]] const char* to_string(WorldForgeFactionKind value) noexcept;
[[nodiscard]] const char* to_string(WorldForgeCanonStatus value) noexcept;
[[nodiscard]] const char* to_string(WorldForgePoliticalRole value) noexcept;

[[nodiscard]] std::filesystem::path default_world_forge_factions_path(const std::filesystem::path& project_root);

} // namespace engine
