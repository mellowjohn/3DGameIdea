#pragma once

#include "engine/core/result.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace engine {

enum class WorldForgeQuestKind : std::uint8_t { Main, Side, Faction };
enum class WorldForgeQuestCanonStatus : std::uint8_t { Established, Draft, Proposal, Open };

/// Ordered objective / stage. Optional `dialogue_id` selects the tree for this stage (DEC-0026).
struct WorldForgeQuestObjective {
    std::string id;
    std::string summary;
    std::string dialogue_id;
};

/// Meaningful branch with mutually exclusive outcome flags. Optional dialogue for the fork beat.
struct WorldForgeQuestFork {
    std::string id;
    std::string summary;
    std::vector<std::string> outcome_flags;
    std::string dialogue_id;
};

/// Quest-level dialogue hooks (in addition to per-objective / per-fork ids).
struct WorldForgeQuestDialogueHooks {
    std::string start_id;
    std::string complete_id;
    std::string abandon_id;
};

/// Quest gate / reward for faction standing (DEC-0029).
struct WorldForgeQuestStandingRequirement {
    std::string faction_id;
    /// Soft: either min_score and/or min_rank_id may be set.
    std::optional<double> min_score;
    std::string min_rank_id;
};

struct WorldForgeQuestStandingReward {
    std::string faction_id;
    double delta = 0.0;
};

struct WorldForgeQuest {
    std::string id;
    WorldForgeQuestKind kind = WorldForgeQuestKind::Side;
    std::string display_name;
    WorldForgeQuestCanonStatus canon_status = WorldForgeQuestCanonStatus::Draft;
    std::string summary;
    std::string story_ref;
    bool consequential = false;
    /// Optional map region id (soft-checked when a known set is provided).
    std::string region_id;
    std::string starts;
    WorldForgeQuestDialogueHooks dialogue;
    std::vector<WorldForgeQuestObjective> objectives;
    std::vector<WorldForgeQuestFork> forks;
    std::vector<WorldForgeQuestStandingRequirement> standing_requirements;
    std::vector<WorldForgeQuestStandingReward> standing_rewards;
    std::vector<std::string> tags;
    std::vector<std::string> open_questions;
};

struct WorldForgeQuestsAsset {
    int schema_version = 1;
    std::string id;
    std::vector<WorldForgeQuest> quests;

    [[nodiscard]] Result<void> validate() const;
    /// When `known_region_ids` is non-empty, non-empty `regionId` values must be listed.
    [[nodiscard]] Result<void> validate_region_refs(const std::unordered_set<std::string>& known_region_ids) const;
    [[nodiscard]] static Result<WorldForgeQuestsAsset> load(const std::filesystem::path& path);
    [[nodiscard]] static Result<WorldForgeQuestsAsset> parse(const std::string& text,
        const std::string& source_name = "quests.worldforge.json");
    [[nodiscard]] Result<void> save_atomic(const std::filesystem::path& path) const;
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static Result<void> validate_file(const std::filesystem::path& path);
    [[nodiscard]] static Result<void> validate_file(const std::filesystem::path& path,
        const std::unordered_set<std::string>& known_region_ids);
};

[[nodiscard]] const char* to_string(WorldForgeQuestKind value) noexcept;
[[nodiscard]] const char* to_string(WorldForgeQuestCanonStatus value) noexcept;

[[nodiscard]] std::filesystem::path default_world_forge_quests_path(const std::filesystem::path& project_root);

} // namespace engine
