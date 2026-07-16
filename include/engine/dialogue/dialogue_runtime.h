#pragma once

#include "engine/assets/world_forge_dialogues_asset.h"
#include "engine/core/result.h"

#include <string>
#include <vector>

namespace engine {

struct DialoguePresent {
    std::string tree_id;
    std::string node_id;
    std::string speaker_id;
    std::string line;
    std::vector<WorldForgeDialogueChoice> choices;
    bool complete = false;
};

/// Headless branching dialogue walker (TICKET-0052). Applies choice setFlags into session history.
class DialogueRuntime {
public:
    [[nodiscard]] Result<void> bind(const WorldForgeDialoguesAsset* asset);
    [[nodiscard]] Result<void> start(const std::string& tree_id);
    [[nodiscard]] Result<DialoguePresent> present() const;
    [[nodiscard]] Result<void> choose(const std::string& choice_id);
    [[nodiscard]] bool is_complete() const noexcept { return complete_; }
    [[nodiscard]] const std::string& tree_id() const noexcept { return tree_id_; }
    [[nodiscard]] const std::string& current_node_id() const noexcept { return node_id_; }
    [[nodiscard]] const std::vector<std::string>& flags_set() const noexcept { return flags_set_; }
    void reset() noexcept;

private:
    const WorldForgeDialoguesAsset* asset_ = nullptr;
    std::string tree_id_;
    std::string node_id_;
    std::vector<std::string> flags_set_;
    bool complete_ = false;
};

} // namespace engine
