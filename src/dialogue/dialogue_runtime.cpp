#include "engine/dialogue/dialogue_runtime.h"

#include "engine/core/error.h"

namespace engine {
namespace {

EngineError rt_error(std::string code, ErrorCategory category, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, category, "dialogue_runtime", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

const WorldForgeDialogueNode* find_node(const WorldForgeDialogueTree& tree, const std::string& node_id) {
    for (const auto& node : tree.nodes) {
        if (node.id == node_id) return &node;
    }
    return nullptr;
}

} // namespace

Result<void> DialogueRuntime::bind(const WorldForgeDialoguesAsset* asset) {
    if (!asset) {
        return Result<void>::failure(rt_error("DIALOGUE-RT-BIND", ErrorCategory::Validation,
            "DialogueRuntime requires a World Forge dialogues asset", "Call bind with a loaded dialogues asset."));
    }
    asset_ = asset;
    reset();
    return Result<void>::success();
}

Result<void> DialogueRuntime::start(const std::string& tree_id) {
    if (!asset_) {
        return Result<void>::failure(rt_error("DIALOGUE-RT-STATE", ErrorCategory::Validation,
            "DialogueRuntime is not bound to a dialogues asset", "Call bind before start."));
    }
    const auto* tree = asset_->find_tree(tree_id);
    if (!tree) {
        return Result<void>::failure(rt_error("DIALOGUE-RT-START", ErrorCategory::Validation,
            "Unknown dialogue tree: " + tree_id, "Use a tree id from dialogues.worldforge.json."));
    }
    tree_id_ = tree_id;
    node_id_ = tree->entry_node_id;
    flags_set_.clear();
    complete_ = false;
    if (const auto* entry = find_node(*tree, node_id_); entry && entry->choices.empty()) complete_ = true;
    return Result<void>::success();
}

Result<DialoguePresent> DialogueRuntime::present() const {
    if (!asset_ || tree_id_.empty()) {
        return Result<DialoguePresent>::failure(rt_error("DIALOGUE-RT-STATE", ErrorCategory::Validation,
            "DialogueRuntime has no active tree", "Call bind and start before present."));
    }
    const auto* tree = asset_->find_tree(tree_id_);
    if (!tree) {
        return Result<DialoguePresent>::failure(rt_error("DIALOGUE-RT-STATE", ErrorCategory::Validation,
            "Active dialogue tree is missing: " + tree_id_, "Restart the dialogue session."));
    }
    const auto* node = find_node(*tree, node_id_);
    if (!node) {
        return Result<DialoguePresent>::failure(rt_error("DIALOGUE-RT-NODE", ErrorCategory::Validation,
            "Current dialogue node is missing: " + node_id_, "Restart the dialogue session."));
    }

    DialoguePresent present;
    present.tree_id = tree_id_;
    present.node_id = node_id_;
    present.speaker_id = node->speaker_id;
    present.line = node->line;
    present.complete = complete_;
    if (!complete_) present.choices = node->choices;
    return Result<DialoguePresent>::success(std::move(present));
}

Result<void> DialogueRuntime::choose(const std::string& choice_id) {
    if (!asset_ || tree_id_.empty()) {
        return Result<void>::failure(rt_error("DIALOGUE-RT-STATE", ErrorCategory::Validation,
            "DialogueRuntime has no active tree", "Call bind and start before choose."));
    }
    if (complete_) {
        return Result<void>::failure(rt_error("DIALOGUE-RT-CHOOSE", ErrorCategory::Validation,
            "Dialogue is already complete", "Call start to begin a new conversation."));
    }
    const auto* tree = asset_->find_tree(tree_id_);
    if (!tree) {
        return Result<void>::failure(rt_error("DIALOGUE-RT-STATE", ErrorCategory::Validation,
            "Active dialogue tree is missing: " + tree_id_, "Restart the dialogue session."));
    }
    const auto* node = find_node(*tree, node_id_);
    if (!node) {
        return Result<void>::failure(rt_error("DIALOGUE-RT-NODE", ErrorCategory::Validation,
            "Current dialogue node is missing: " + node_id_, "Restart the dialogue session."));
    }

    const WorldForgeDialogueChoice* choice = nullptr;
    for (const auto& candidate : node->choices) {
        if (candidate.id == choice_id) {
            choice = &candidate;
            break;
        }
    }
    if (!choice) {
        return Result<void>::failure(rt_error("DIALOGUE-RT-CHOOSE", ErrorCategory::Validation,
            "Unknown choice '" + choice_id + "' on node '" + node_id_ + "'", "Use a choice id from the current node."));
    }

    for (const auto& flag : choice->set_flags) flags_set_.push_back(flag);
    if (choice->next_node_id.empty()) {
        complete_ = true;
    } else {
        node_id_ = choice->next_node_id;
        if (const auto* next = find_node(*tree, node_id_); next && next->choices.empty()) complete_ = true;
    }
    return Result<void>::success();
}

void DialogueRuntime::reset() noexcept {
    tree_id_.clear();
    node_id_.clear();
    flags_set_.clear();
    complete_ = false;
}

} // namespace engine
