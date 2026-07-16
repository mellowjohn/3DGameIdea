#include "engine/standing/standing_runtime.h"

#include "engine/core/error.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace engine {
namespace {

EngineError rt_error(std::string code, ErrorCategory category, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, category, "standing_runtime", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

bool is_hostility_kind(WorldForgeRelationshipEdgeKind kind) {
    return kind == WorldForgeRelationshipEdgeKind::Rival || kind == WorldForgeRelationshipEdgeKind::Opposes;
}

} // namespace

Result<void> StandingRuntime::bind(const WorldForgeFactionsAsset* factions,
    const WorldForgeRelationshipsAsset* relationships) {
    if (!factions || !relationships) {
        return Result<void>::failure(rt_error("STANDING-RUNTIME-BIND", ErrorCategory::Validation,
            "StandingRuntime requires factions and relationships assets", "Call bind with both assets."));
    }
    factions_ = factions;
    relationships_ = relationships;
    reset();
    return Result<void>::success();
}

const WorldForgeFactionEntity* StandingRuntime::find_tracked(const std::string& faction_id) const {
    if (!factions_) return nullptr;
    const auto* entity = factions_->find_entity(faction_id);
    if (!entity || !entity->standing || !entity->standing->tracks_player) return nullptr;
    return entity;
}

double StandingRuntime::clamp_score(const WorldForgeFactionStandingConfig& config, double score) {
    return (std::clamp)(score, config.min_score, config.max_score);
}

std::string StandingRuntime::rank_for_score(const WorldForgeFactionStandingConfig& config, double score) {
    std::string best_id;
    double best_min = -std::numeric_limits<double>::infinity();
    for (const auto& rank : config.ranks) {
        if (score + 1e-9 >= rank.min_score && rank.min_score >= best_min) {
            best_min = rank.min_score;
            best_id = rank.id;
        }
    }
    return best_id;
}

Result<void> StandingRuntime::ensure_score_slot(const std::string& faction_id) {
    if (!find_tracked(faction_id)) {
        return Result<void>::failure(rt_error("STANDING-RUNTIME-UNKNOWN", ErrorCategory::Validation,
            "Faction is not tracked for standing: " + faction_id,
            "Set standing.tracksPlayer on the faction entity."));
    }
    if (scores_.find(faction_id) == scores_.end()) scores_[faction_id] = 0.0;
    return Result<void>::success();
}

Result<double> StandingRuntime::get(const std::string& faction_id) const {
    if (!is_bound()) {
        return Result<double>::failure(rt_error("STANDING-RUNTIME-STATE", ErrorCategory::Validation,
            "StandingRuntime is not bound", "Call bind before get."));
    }
    if (!find_tracked(faction_id)) {
        return Result<double>::failure(rt_error("STANDING-RUNTIME-UNKNOWN", ErrorCategory::Validation,
            "Faction is not tracked for standing: " + faction_id,
            "Set standing.tracksPlayer on the faction entity."));
    }
    const auto it = scores_.find(faction_id);
    return Result<double>::success(it == scores_.end() ? 0.0 : it->second);
}

Result<void> StandingRuntime::set(const std::string& faction_id, double score) {
    if (!is_bound()) {
        return Result<void>::failure(rt_error("STANDING-RUNTIME-STATE", ErrorCategory::Validation,
            "StandingRuntime is not bound", "Call bind before set."));
    }
    const auto* entity = find_tracked(faction_id);
    if (!entity) {
        return Result<void>::failure(rt_error("STANDING-RUNTIME-UNKNOWN", ErrorCategory::Validation,
            "Faction is not tracked for standing: " + faction_id,
            "Set standing.tracksPlayer on the faction entity."));
    }
    scores_[faction_id] = clamp_score(*entity->standing, score);
    return Result<void>::success();
}

void StandingRuntime::apply_hostility_transfers(const std::string& primary_faction_id, double primary_delta) {
    if (!relationships_ || primary_delta == 0.0) return;
    for (const auto& edge : relationships_->edges) {
        if (!is_hostility_kind(edge.kind) || edge.standing_transfer <= 0.0) continue;
        if (edge.from.target != WorldForgeRelationshipEndpointTarget::Faction ||
            edge.to.target != WorldForgeRelationshipEndpointTarget::Faction)
            continue;
        std::string other;
        if (edge.from.id == primary_faction_id) other = edge.to.id;
        else if (edge.to.id == primary_faction_id) other = edge.from.id;
        else continue;
        const auto* other_entity = find_tracked(other);
        if (!other_entity) continue;
        const double fallout = -primary_delta * edge.standing_transfer;
        auto& score = scores_[other];
        score = clamp_score(*other_entity->standing, score + fallout);
    }
}

Result<void> StandingRuntime::adjust(const std::string& faction_id, double delta) {
    if (!is_bound()) {
        return Result<void>::failure(rt_error("STANDING-RUNTIME-STATE", ErrorCategory::Validation,
            "StandingRuntime is not bound", "Call bind before adjust."));
    }
    if (const auto ensured = ensure_score_slot(faction_id); !ensured) return ensured;
    const auto* entity = find_tracked(faction_id);
    auto& score = scores_[faction_id];
    score = clamp_score(*entity->standing, score + delta);
    apply_hostility_transfers(faction_id, delta);
    return Result<void>::success();
}

Result<std::string> StandingRuntime::rank(const std::string& faction_id) const {
    const auto score = get(faction_id);
    if (!score) return Result<std::string>::failure(score.error());
    const auto* entity = find_tracked(faction_id);
    return Result<std::string>::success(rank_for_score(*entity->standing, score.value()));
}

Result<bool> StandingRuntime::meets_requirement(const std::string& faction_id, double min_score) const {
    const auto score = get(faction_id);
    if (!score) return Result<bool>::failure(score.error());
    return Result<bool>::success(score.value() + 1e-9 >= min_score);
}

Result<bool> StandingRuntime::meets_requirement(const std::string& faction_id, const std::string& min_rank_id) const {
    if (min_rank_id.empty()) {
        return Result<bool>::failure(rt_error("STANDING-RUNTIME-REQ", ErrorCategory::Validation,
            "minRankId is empty", "Provide a rank id."));
    }
    const auto* entity = find_tracked(faction_id);
    if (!entity) {
        return Result<bool>::failure(rt_error("STANDING-RUNTIME-UNKNOWN", ErrorCategory::Validation,
            "Faction is not tracked for standing: " + faction_id,
            "Set standing.tracksPlayer on the faction entity."));
    }
    const auto* required = static_cast<const WorldForgeFactionStandingRank*>(nullptr);
    for (const auto& rank : entity->standing->ranks) {
        if (rank.id == min_rank_id) {
            required = &rank;
            break;
        }
    }
    if (!required) {
        return Result<bool>::failure(rt_error("STANDING-RUNTIME-RANK", ErrorCategory::Validation,
            "Unknown rank '" + min_rank_id + "' on faction '" + faction_id + "'",
            "Use a rank id from the faction standing config."));
    }
    return meets_requirement(faction_id, required->min_score);
}

Result<bool> StandingRuntime::meets_requirement(const WorldForgeQuestStandingRequirement& requirement) const {
    if (!requirement.min_rank_id.empty()) {
        const auto by_rank = meets_requirement(requirement.faction_id, requirement.min_rank_id);
        if (!by_rank) return by_rank;
        if (!by_rank.value()) return by_rank;
    }
    if (requirement.min_score) {
        return meets_requirement(requirement.faction_id, *requirement.min_score);
    }
    if (requirement.min_rank_id.empty()) {
        return Result<bool>::failure(rt_error("STANDING-RUNTIME-REQ", ErrorCategory::Validation,
            "Requirement needs minScore and/or minRankId", "Fill at least one gate field."));
    }
    return Result<bool>::success(true);
}

Result<std::string> StandingRuntime::lock_in_faction() const {
    if (!is_bound()) {
        return Result<std::string>::failure(rt_error("STANDING-RUNTIME-STATE", ErrorCategory::Validation,
            "StandingRuntime is not bound", "Call bind before lock_in_faction."));
    }
    for (const auto& entity : factions_->entities) {
        if (!entity.standing || !entity.standing->tracks_player || !entity.standing->lock_in) continue;
        const auto it = scores_.find(entity.id);
        const double score = it == scores_.end() ? 0.0 : it->second;
        if (score + 1e-9 >= entity.standing->lock_in->threshold) {
            return Result<std::string>::success(entity.id);
        }
    }
    return Result<std::string>::success(std::string{});
}

std::vector<StandingScoreEntry> StandingRuntime::list_tracked() const {
    std::vector<StandingScoreEntry> out;
    if (!factions_) return out;
    for (const auto& entity : factions_->entities) {
        if (!entity.standing || !entity.standing->tracks_player) continue;
        StandingScoreEntry entry;
        entry.faction_id = entity.id;
        const auto it = scores_.find(entity.id);
        entry.score = it == scores_.end() ? 0.0 : it->second;
        entry.rank_id = rank_for_score(*entity.standing, entry.score);
        out.push_back(std::move(entry));
    }
    return out;
}

void StandingRuntime::reset() noexcept {
    scores_.clear();
}

} // namespace engine
