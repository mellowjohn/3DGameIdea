#pragma once

#include "engine/assets/world_forge_quests_asset.h"
#include "engine/core/result.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace engine {

enum class QuestInstanceStatus : std::uint8_t { Inactive, Active, Completed, Abandoned };

enum class QuestDialogueStage : std::uint8_t { Start, CurrentObjective, Complete, Abandon };

struct QuestProgressStatus {
    std::string quest_id;
    QuestInstanceStatus status = QuestInstanceStatus::Inactive;
    std::string current_objective_id;
    std::string current_objective_summary;
    std::vector<std::string> completed_objective_ids;
};

[[nodiscard]] const char* to_string(QuestInstanceStatus value) noexcept;
[[nodiscard]] const char* to_string(QuestDialogueStage value) noexcept;

/// Headless quest progression walker (TICKET-0180 / DEC-0028). Explicit API only — no auto-advance from dialogue.
class QuestRuntime {
public:
    [[nodiscard]] Result<void> bind(const WorldForgeQuestsAsset* asset);
    [[nodiscard]] Result<void> start(const std::string& quest_id);
    [[nodiscard]] Result<void> complete_objective(const std::string& quest_id, const std::string& objective_id);
    [[nodiscard]] Result<void> abandon(const std::string& quest_id);
    [[nodiscard]] Result<QuestProgressStatus> status(const std::string& quest_id) const;
    [[nodiscard]] std::vector<QuestProgressStatus> list_active() const;
    /// Returns the hooked dialogue tree id for a stage (may be empty). Does not mutate progress.
    [[nodiscard]] Result<std::string> dialogue_for_stage(const std::string& quest_id, QuestDialogueStage stage) const;
    void reset() noexcept;

    [[nodiscard]] bool is_bound() const noexcept { return asset_ != nullptr; }
    /// Summary text for HUD bind `quest.objectiveText` (empty when no active current objective).
    [[nodiscard]] std::string primary_objective_text() const;

private:
    struct Instance {
        QuestInstanceStatus status = QuestInstanceStatus::Inactive;
        std::vector<std::string> completed_objective_ids;
    };

    [[nodiscard]] const WorldForgeQuest* find_quest(const std::string& quest_id) const;
    [[nodiscard]] Instance* find_instance(const std::string& quest_id);
    [[nodiscard]] const Instance* find_instance(const std::string& quest_id) const;
    [[nodiscard]] Result<QuestProgressStatus> build_status(const WorldForgeQuest& quest, const Instance* instance) const;
    [[nodiscard]] static std::string current_objective_id(const WorldForgeQuest& quest, const Instance& instance);

    const WorldForgeQuestsAsset* asset_ = nullptr;
    std::vector<std::pair<std::string, Instance>> instances_;
};

} // namespace engine
