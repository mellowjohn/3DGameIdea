#pragma once

#include "engine/core/result.h"
#include "engine/flag/flag_runtime.h"
#include "engine/party/party_runtime.h"
#include "engine/quest/quest_runtime.h"
#include "engine/session/game_session.h"
#include "engine/standing/standing_runtime.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace engine {

struct RpgSavePlayerProfile {
    int player_slot = 0;
    std::string display_name;
    std::string archetype_id;
};

struct RpgSaveWorldAnchor {
    std::string scene_id;
    std::string region_id;
    std::array<double, 3> position{0.0, 0.0, 0.0};
    double yaw_degrees = 0.0;
};

struct RpgSaveQuestInstance {
    std::string quest_id;
    QuestInstanceStatus status = QuestInstanceStatus::Inactive;
    std::vector<std::string> completed_objective_ids;
};

struct RpgSaveStandingScore {
    std::string faction_id;
    double score = 0.0;
};

struct RpgSaveSharedCampaign {
    std::vector<RpgSaveQuestInstance> quest_instances;
    std::vector<std::string> outcome_flags;
    std::vector<RpgSaveStandingScore> standing_scores;
    std::string lock_in_faction_id;
    double morality_score = 0.0;
    std::string morality_band_id = "neutral";
    std::vector<std::string> unlocked_archetype_ids;
    std::vector<std::string> active_companion_ids;
    double gold = 0.0;
};

/// In-memory `*.save.json` document (TICKET-0114 / DEC-0042).
struct RpgSaveDocument {
    static constexpr int k_schema_version = 1;

    int schema_version = k_schema_version;
    std::string save_id;
    std::string display_name;
    SessionMode session_mode = SessionMode::Solo;
    std::string difficulty = "normal";
    std::uint64_t play_time_seconds = 0;
    RpgSaveWorldAnchor world_anchor;
    RpgSavePlayerProfile host_profile;
    std::optional<RpgSavePlayerProfile> guest_profile;
    RpgSaveSharedCampaign shared_campaign;

    [[nodiscard]] Result<void> validate() const;
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static Result<RpgSaveDocument> from_json(const std::string& text);
    [[nodiscard]] static Result<RpgSaveDocument> load(const std::filesystem::path& path);
    [[nodiscard]] Result<void> save(const std::filesystem::path& path) const;

    /// Capture runtime state into sharedCampaign (+ optional party).
    [[nodiscard]] Result<void> capture_from(const QuestRuntime& quests, const StandingRuntime& standing,
        const FlagRuntime& flags, const PartyRuntime* party = nullptr);
    /// Apply sharedCampaign into bound runtimes. Does not start GameSession playing.
    [[nodiscard]] Result<void> hydrate_into(QuestRuntime& quests, StandingRuntime& standing, FlagRuntime& flags,
        PartyRuntime* party = nullptr) const;
};

/// Apply schema migrations; currently identity for v1. Unsupported versions fail closed.
[[nodiscard]] Result<std::string> migrate_rpg_save_json(std::string json_text, int* out_from_version = nullptr);

/// Apply loaded save to a GameSession: solo → playing path; coop without guest → lobby only.
[[nodiscard]] Result<void> apply_rpg_save_to_session(const RpgSaveDocument& doc, GameSession& session,
    bool guest_connected);

} // namespace engine
