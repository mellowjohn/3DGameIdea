#include "engine/quest/quest_runtime.h"

#include "engine/core/error.h"
#include "engine/flag/flag_runtime.h"

#include <algorithm>
#include <utility>

namespace engine {
namespace {

EngineError rt_error(std::string code, ErrorCategory category, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, category, "quest_runtime", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

} // namespace

const char* to_string(QuestInstanceStatus value) noexcept {
    switch (value) {
    case QuestInstanceStatus::Inactive: return "inactive";
    case QuestInstanceStatus::Active: return "active";
    case QuestInstanceStatus::Completed: return "completed";
    case QuestInstanceStatus::Abandoned: return "abandoned";
    }
    return "inactive";
}

const char* to_string(QuestDialogueStage value) noexcept {
    switch (value) {
    case QuestDialogueStage::Start: return "start";
    case QuestDialogueStage::CurrentObjective: return "currentObjective";
    case QuestDialogueStage::Complete: return "complete";
    case QuestDialogueStage::Abandon: return "abandon";
    }
    return "start";
}

Result<void> QuestRuntime::bind(const WorldForgeQuestsAsset* asset) {
    if (!asset) {
        return Result<void>::failure(rt_error("QUEST-RUNTIME-BIND", ErrorCategory::Validation,
            "QuestRuntime requires a World Forge quests asset", "Call bind with a loaded quests asset."));
    }
    asset_ = asset;
    reset();
    return Result<void>::success();
}

const WorldForgeQuest* QuestRuntime::find_quest(const std::string& quest_id) const {
    if (!asset_) return nullptr;
    for (const auto& quest : asset_->quests) {
        if (quest.id == quest_id) return &quest;
    }
    return nullptr;
}

QuestRuntime::Instance* QuestRuntime::find_instance(const std::string& quest_id) {
    for (auto& entry : instances_) {
        if (entry.first == quest_id) return &entry.second;
    }
    return nullptr;
}

const QuestRuntime::Instance* QuestRuntime::find_instance(const std::string& quest_id) const {
    for (const auto& entry : instances_) {
        if (entry.first == quest_id) return &entry.second;
    }
    return nullptr;
}

std::string QuestRuntime::current_objective_id(const WorldForgeQuest& quest, const Instance& instance) {
    for (const auto& objective : quest.objectives) {
        const bool done = std::find(instance.completed_objective_ids.begin(), instance.completed_objective_ids.end(),
                             objective.id) != instance.completed_objective_ids.end();
        if (!done) return objective.id;
    }
    return {};
}

Result<QuestProgressStatus> QuestRuntime::build_status(const WorldForgeQuest& quest, const Instance* instance) const {
    QuestProgressStatus out;
    out.quest_id = quest.id;
    if (!instance) {
        out.status = QuestInstanceStatus::Inactive;
        return Result<QuestProgressStatus>::success(std::move(out));
    }
    out.status = instance->status;
    out.completed_objective_ids = instance->completed_objective_ids;
    if (instance->status == QuestInstanceStatus::Active) {
        out.current_objective_id = current_objective_id(quest, *instance);
        for (const auto& objective : quest.objectives) {
            if (objective.id == out.current_objective_id) {
                out.current_objective_summary = objective.summary;
                break;
            }
        }
    }
    return Result<QuestProgressStatus>::success(std::move(out));
}

Result<void> QuestRuntime::start(const std::string& quest_id) {
    if (!asset_) {
        return Result<void>::failure(rt_error("QUEST-RUNTIME-STATE", ErrorCategory::Validation,
            "QuestRuntime is not bound to a quests asset", "Call bind before start."));
    }
    const auto* quest = find_quest(quest_id);
    if (!quest) {
        return Result<void>::failure(rt_error("QUEST-RUNTIME-UNKNOWN", ErrorCategory::Validation,
            "Unknown quest: " + quest_id, "Use a quest id from quests.worldforge.json."));
    }
    auto* existing = find_instance(quest_id);
    if (existing && existing->status == QuestInstanceStatus::Active) {
        return Result<void>::failure(rt_error("QUEST-RUNTIME-ACTIVE", ErrorCategory::Validation,
            "Quest is already active: " + quest_id, "Complete or abandon before restarting."));
    }
    Instance instance;
    instance.status = quest->objectives.empty() ? QuestInstanceStatus::Completed : QuestInstanceStatus::Active;
    if (existing) {
        *existing = std::move(instance);
    } else {
        instances_.emplace_back(quest_id, std::move(instance));
    }
    return Result<void>::success();
}

Result<void> QuestRuntime::complete_objective(const std::string& quest_id, const std::string& objective_id) {
    if (!asset_) {
        return Result<void>::failure(rt_error("QUEST-RUNTIME-STATE", ErrorCategory::Validation,
            "QuestRuntime is not bound to a quests asset", "Call bind before complete_objective."));
    }
    const auto* quest = find_quest(quest_id);
    if (!quest) {
        return Result<void>::failure(rt_error("QUEST-RUNTIME-UNKNOWN", ErrorCategory::Validation,
            "Unknown quest: " + quest_id, "Use a quest id from quests.worldforge.json."));
    }
    auto* instance = find_instance(quest_id);
    if (!instance || instance->status != QuestInstanceStatus::Active) {
        return Result<void>::failure(rt_error("QUEST-RUNTIME-NOT-ACTIVE", ErrorCategory::Validation,
            "Quest is not active: " + quest_id, "Call start before completing an objective."));
    }
    const auto current = current_objective_id(*quest, *instance);
    if (current.empty()) {
        return Result<void>::failure(rt_error("QUEST-RUNTIME-NO-OBJECTIVE", ErrorCategory::Validation,
            "Quest has no remaining objectives: " + quest_id, "Quest should already be completed."));
    }
    if (objective_id != current) {
        return Result<void>::failure(rt_error("QUEST-RUNTIME-WRONG-OBJECTIVE", ErrorCategory::Validation,
            "Objective '" + objective_id + "' is not current (expected '" + current + "')",
            "Complete objectives in catalog order."));
    }
    bool known = false;
    for (const auto& objective : quest->objectives) {
        if (objective.id == objective_id) {
            known = true;
            break;
        }
    }
    if (!known) {
        return Result<void>::failure(rt_error("QUEST-RUNTIME-UNKNOWN-OBJECTIVE", ErrorCategory::Validation,
            "Unknown objective '" + objective_id + "' on quest '" + quest_id + "'",
            "Use an objective id from the quest asset."));
    }
    instance->completed_objective_ids.push_back(objective_id);
    if (current_objective_id(*quest, *instance).empty()) {
        instance->status = QuestInstanceStatus::Completed;
    }
    return Result<void>::success();
}

Result<void> QuestRuntime::abandon(const std::string& quest_id) {
    if (!asset_) {
        return Result<void>::failure(rt_error("QUEST-RUNTIME-STATE", ErrorCategory::Validation,
            "QuestRuntime is not bound to a quests asset", "Call bind before abandon."));
    }
    if (!find_quest(quest_id)) {
        return Result<void>::failure(rt_error("QUEST-RUNTIME-UNKNOWN", ErrorCategory::Validation,
            "Unknown quest: " + quest_id, "Use a quest id from quests.worldforge.json."));
    }
    auto* instance = find_instance(quest_id);
    if (!instance || instance->status != QuestInstanceStatus::Active) {
        return Result<void>::failure(rt_error("QUEST-RUNTIME-NOT-ACTIVE", ErrorCategory::Validation,
            "Quest is not active: " + quest_id, "Only active quests can be abandoned."));
    }
    instance->status = QuestInstanceStatus::Abandoned;
    return Result<void>::success();
}

Result<void> QuestRuntime::resolve_fork(const std::string& quest_id, const std::string& fork_id,
    const std::string& outcome_flag, FlagRuntime& flags) {
    if (!asset_) {
        return Result<void>::failure(rt_error("QUEST-RUNTIME-STATE", ErrorCategory::Validation,
            "QuestRuntime is not bound to a quests asset", "Call bind before resolve_fork."));
    }
    if (outcome_flag.empty()) {
        return Result<void>::failure(rt_error("QUEST-RUNTIME-FORK-FLAG", ErrorCategory::Validation,
            "outcome_flag must be non-empty", "Pass one outcomeFlags entry from the quest fork."));
    }
    const auto* quest = find_quest(quest_id);
    if (!quest) {
        return Result<void>::failure(rt_error("QUEST-RUNTIME-UNKNOWN", ErrorCategory::Validation,
            "Unknown quest: " + quest_id, "Use a quest id from quests.worldforge.json."));
    }
    const auto* instance = find_instance(quest_id);
    if (!instance ||
        (instance->status != QuestInstanceStatus::Active && instance->status != QuestInstanceStatus::Completed)) {
        return Result<void>::failure(rt_error("QUEST-RUNTIME-NOT-ACTIVE", ErrorCategory::Validation,
            "Quest must be active or completed to resolve a fork: " + quest_id,
            "Call start before resolve_fork."));
    }
    const WorldForgeQuestFork* fork = nullptr;
    for (const auto& candidate : quest->forks) {
        if (candidate.id == fork_id) {
            fork = &candidate;
            break;
        }
    }
    if (!fork) {
        return Result<void>::failure(rt_error("QUEST-RUNTIME-UNKNOWN-FORK", ErrorCategory::Validation,
            "Unknown fork '" + fork_id + "' on quest '" + quest_id + "'",
            "Use a fork id from the quest asset."));
    }
    const bool known = std::find(fork->outcome_flags.begin(), fork->outcome_flags.end(), outcome_flag) !=
        fork->outcome_flags.end();
    if (!known) {
        return Result<void>::failure(rt_error("QUEST-RUNTIME-FORK-FLAG", ErrorCategory::Validation,
            "Flag '" + outcome_flag + "' is not an outcomeFlags entry on fork '" + fork_id + "'",
            "Pass one of the fork's authored outcomeFlags."));
    }
    for (const auto& sibling : fork->outcome_flags) {
        if (sibling == outcome_flag) continue;
        if (const auto cleared = flags.clear(sibling); !cleared) return cleared;
    }
    return flags.set(outcome_flag);
}

Result<QuestProgressStatus> QuestRuntime::status(const std::string& quest_id) const {
    if (!asset_) {
        return Result<QuestProgressStatus>::failure(rt_error("QUEST-RUNTIME-STATE", ErrorCategory::Validation,
            "QuestRuntime is not bound to a quests asset", "Call bind before status."));
    }
    const auto* quest = find_quest(quest_id);
    if (!quest) {
        return Result<QuestProgressStatus>::failure(rt_error("QUEST-RUNTIME-UNKNOWN", ErrorCategory::Validation,
            "Unknown quest: " + quest_id, "Use a quest id from quests.worldforge.json."));
    }
    return build_status(*quest, find_instance(quest_id));
}

std::vector<QuestProgressStatus> QuestRuntime::list_active() const {
    std::vector<QuestProgressStatus> out;
    if (!asset_) return out;
    for (const auto& [quest_id, instance] : instances_) {
        if (instance.status != QuestInstanceStatus::Active) continue;
        const auto* quest = find_quest(quest_id);
        if (!quest) continue;
        if (auto built = build_status(*quest, &instance); built) out.push_back(std::move(built.value()));
    }
    return out;
}

Result<std::string> QuestRuntime::dialogue_for_stage(const std::string& quest_id, QuestDialogueStage stage) const {
    if (!asset_) {
        return Result<std::string>::failure(rt_error("QUEST-RUNTIME-STATE", ErrorCategory::Validation,
            "QuestRuntime is not bound to a quests asset", "Call bind before dialogue_for_stage."));
    }
    const auto* quest = find_quest(quest_id);
    if (!quest) {
        return Result<std::string>::failure(rt_error("QUEST-RUNTIME-UNKNOWN", ErrorCategory::Validation,
            "Unknown quest: " + quest_id, "Use a quest id from quests.worldforge.json."));
    }
    switch (stage) {
    case QuestDialogueStage::Start:
        return Result<std::string>::success(quest->dialogue.start_id);
    case QuestDialogueStage::Complete:
        return Result<std::string>::success(quest->dialogue.complete_id);
    case QuestDialogueStage::Abandon:
        return Result<std::string>::success(quest->dialogue.abandon_id);
    case QuestDialogueStage::CurrentObjective: {
        const auto* instance = find_instance(quest_id);
        if (!instance || instance->status != QuestInstanceStatus::Active) {
            return Result<std::string>::success(std::string{});
        }
        const auto current = current_objective_id(*quest, *instance);
        for (const auto& objective : quest->objectives) {
            if (objective.id == current) return Result<std::string>::success(objective.dialogue_id);
        }
        return Result<std::string>::success(std::string{});
    }
    }
    return Result<std::string>::success(std::string{});
}

void QuestRuntime::reset() noexcept {
    instances_.clear();
}

std::string QuestRuntime::primary_objective_text() const {
    const auto active = list_active();
    if (active.empty()) return {};
    if (!active.front().current_objective_summary.empty()) return active.front().current_objective_summary;
    return active.front().current_objective_id;
}

std::vector<QuestProgressStatus> QuestRuntime::list_instances() const {
    std::vector<QuestProgressStatus> out;
    if (!asset_) return out;
    for (const auto& [quest_id, instance] : instances_) {
        if (instance.status == QuestInstanceStatus::Inactive) continue;
        const auto* quest = find_quest(quest_id);
        if (!quest) continue;
        if (auto built = build_status(*quest, &instance); built) out.push_back(std::move(built.value()));
    }
    return out;
}

Result<void> QuestRuntime::restore_instance(const std::string& quest_id, QuestInstanceStatus status,
    std::vector<std::string> completed_objective_ids) {
    if (!asset_) {
        return Result<void>::failure(rt_error("QUEST-RUNTIME-STATE", ErrorCategory::Validation,
            "QuestRuntime is not bound to a quests asset", "Call bind before restore_instance."));
    }
    if (status == QuestInstanceStatus::Inactive) {
        return Result<void>::failure(rt_error("QUEST-RUNTIME-RESTORE", ErrorCategory::Validation,
            "Cannot restore inactive quest instance", "Omit inactive quests from the save blob."));
    }
    const auto* quest = find_quest(quest_id);
    if (!quest) {
        return Result<void>::failure(rt_error("QUEST-RUNTIME-UNKNOWN", ErrorCategory::Validation,
            "Unknown quest: " + quest_id, "Use a quest id from quests.worldforge.json."));
    }
    for (const auto& objective_id : completed_objective_ids) {
        bool known = false;
        for (const auto& objective : quest->objectives) {
            if (objective.id == objective_id) {
                known = true;
                break;
            }
        }
        if (!known) {
            return Result<void>::failure(rt_error("QUEST-RUNTIME-UNKNOWN-OBJECTIVE", ErrorCategory::Validation,
                "Unknown objective '" + objective_id + "' on quest '" + quest_id + "'",
                "Fix sharedCampaign.quests.instances completedObjectiveIds."));
        }
    }
    Instance instance;
    instance.status = status;
    instance.completed_objective_ids = std::move(completed_objective_ids);
    if (auto* existing = find_instance(quest_id)) {
        *existing = std::move(instance);
    } else {
        instances_.emplace_back(quest_id, std::move(instance));
    }
    return Result<void>::success();
}

} // namespace engine
