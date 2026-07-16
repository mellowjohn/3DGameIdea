#pragma once

#include "engine/assets/world_forge_factions_asset.h"
#include "engine/assets/world_forge_quests_asset.h"
#include "engine/assets/world_forge_relationships_asset.h"
#include "engine/core/result.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace engine {

struct StandingScoreEntry {
    std::string faction_id;
    double score = 0.0;
    std::string rank_id;
};

/// Session faction standing (TICKET-0181 / DEC-0029). Explicit adjust applies hostility transfers.
class StandingRuntime {
public:
    [[nodiscard]] Result<void> bind(const WorldForgeFactionsAsset* factions,
        const WorldForgeRelationshipsAsset* relationships);
    [[nodiscard]] Result<double> get(const std::string& faction_id) const;
    [[nodiscard]] Result<void> set(const std::string& faction_id, double score);
    [[nodiscard]] Result<void> adjust(const std::string& faction_id, double delta);
    [[nodiscard]] Result<std::string> rank(const std::string& faction_id) const;
    [[nodiscard]] Result<bool> meets_requirement(const WorldForgeQuestStandingRequirement& requirement) const;
    [[nodiscard]] Result<bool> meets_requirement(const std::string& faction_id, double min_score) const;
    [[nodiscard]] Result<bool> meets_requirement(const std::string& faction_id, const std::string& min_rank_id) const;
    /// First tracksPlayer faction whose score >= lockIn.threshold, or empty.
    [[nodiscard]] Result<std::string> lock_in_faction() const;
    [[nodiscard]] std::vector<StandingScoreEntry> list_tracked() const;
    void reset() noexcept;

    [[nodiscard]] bool is_bound() const noexcept { return factions_ != nullptr && relationships_ != nullptr; }

private:
    [[nodiscard]] const WorldForgeFactionEntity* find_tracked(const std::string& faction_id) const;
    [[nodiscard]] Result<void> ensure_score_slot(const std::string& faction_id);
    [[nodiscard]] static double clamp_score(const WorldForgeFactionStandingConfig& config, double score);
    [[nodiscard]] static std::string rank_for_score(const WorldForgeFactionStandingConfig& config, double score);
    void apply_hostility_transfers(const std::string& primary_faction_id, double primary_delta);

    const WorldForgeFactionsAsset* factions_ = nullptr;
    const WorldForgeRelationshipsAsset* relationships_ = nullptr;
    std::unordered_map<std::string, double> scores_;
};

} // namespace engine
