#include "engine/dialogue/dialogue_graph_edit.h"

#include "engine/core/error.h"
#include "engine/core/id_slug.h"

#include <algorithm>
#include <cctype>
#include <queue>
#include <unordered_set>

namespace engine {
namespace {

EngineError graph_error(std::string code, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Validation, "dialogue_graph_edit",
        std::move(message), ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

WorldForgeDialogueNode* find_mutable_node(WorldForgeDialogueTree& tree, const std::string& node_id) {
    for (auto& node : tree.nodes) {
        if (node.id == node_id) return &node;
    }
    return nullptr;
}

const WorldForgeDialogueNode* find_const_node(const WorldForgeDialogueTree& tree, const std::string& node_id) {
    for (const auto& node : tree.nodes) {
        if (node.id == node_id) return &node;
    }
    return nullptr;
}

bool choice_id_exists(const WorldForgeDialogueTree& tree, const std::string& choice_id) {
    for (const auto& node : tree.nodes) {
        for (const auto& choice : node.choices) {
            if (choice.id == choice_id) return true;
        }
    }
    return false;
}

std::string sanitize_id(std::string value) {
    auto slug = slugify_id(value);
    return slug.empty() ? "node" : slug;
}

} // namespace

std::string unique_dialogue_node_id(const WorldForgeDialogueTree& tree, const std::string& preferred) {
    return unique_slugify_id(
        preferred.empty() ? "node" : preferred,
        [&](const std::string& candidate) { return find_const_node(tree, candidate) != nullptr; }, "node");
}

std::string unique_dialogue_choice_id(const WorldForgeDialogueTree& tree, const std::string& preferred) {
    return unique_slugify_id(
        preferred.empty() ? "choice" : preferred,
        [&](const std::string& candidate) { return choice_id_exists(tree, candidate); }, "choice");
}

void layout_dialogue_graph(const WorldForgeDialogueTree& tree, DialogueGraphPositions& positions,
    bool preserve_existing) {
    if (!preserve_existing) positions.clear();

    std::unordered_map<std::string, int> depth;
    std::queue<std::string> queue;
    if (!tree.entry_node_id.empty() && find_const_node(tree, tree.entry_node_id)) {
        depth[tree.entry_node_id] = 0;
        queue.push(tree.entry_node_id);
    }
    while (!queue.empty()) {
        const auto id = queue.front();
        queue.pop();
        const auto* node = find_const_node(tree, id);
        if (!node) continue;
        for (const auto& choice : node->choices) {
            if (choice.next_node_id.empty()) continue;
            if (depth.count(choice.next_node_id)) continue;
            if (!find_const_node(tree, choice.next_node_id)) continue;
            depth[choice.next_node_id] = depth[id] + 1;
            queue.push(choice.next_node_id);
        }
    }

    // Orphans / unreachable sit after the deepest layer.
    int max_depth = 0;
    for (const auto& entry : depth) max_depth = (std::max)(max_depth, entry.second);
    for (const auto& node : tree.nodes) {
        if (!depth.count(node.id)) depth[node.id] = max_depth + 1;
    }

    std::unordered_map<int, std::vector<std::string>> layers;
    for (const auto& entry : depth) layers[entry.second].push_back(entry.first);
    for (auto& layer : layers) std::sort(layer.second.begin(), layer.second.end());

    constexpr float x_spacing = 300.0f;
    constexpr float y_spacing = 160.0f;
    for (const auto& layer : layers) {
        const float x = 40.0f + static_cast<float>(layer.first) * x_spacing;
        for (std::size_t i = 0; i < layer.second.size(); ++i) {
            const auto& id = layer.second[i];
            if (preserve_existing && positions.count(id)) continue;
            const float y = 40.0f + static_cast<float>(i) * y_spacing;
            positions[id] = {x, y};
        }
    }
}

Result<void> add_dialogue_node(WorldForgeDialogueTree& tree, std::string id, std::string speaker_id,
    std::string line) {
    id = sanitize_id(std::move(id));
    if (id.empty()) {
        return Result<void>::failure(
            graph_error("DIALOGUE-GRAPH-NODE-ID", "Dialogue node id is required", "Provide a non-empty id."));
    }
    if (find_const_node(tree, id)) {
        return Result<void>::failure(graph_error("DIALOGUE-GRAPH-NODE-DUP", "Dialogue node id already exists: " + id,
            "Choose a unique node id."));
    }
    WorldForgeDialogueNode node;
    node.id = std::move(id);
    node.speaker_id = std::move(speaker_id);
    node.line = std::move(line);
    tree.nodes.push_back(std::move(node));
    if (tree.entry_node_id.empty()) tree.entry_node_id = tree.nodes.back().id;
    return Result<void>::success();
}

Result<void> remove_dialogue_node(WorldForgeDialogueTree& tree, const std::string& node_id) {
    if (!find_const_node(tree, node_id)) {
        return Result<void>::failure(graph_error("DIALOGUE-GRAPH-NODE-MISSING", "Unknown dialogue node: " + node_id,
            "Select an existing node."));
    }
    if (tree.nodes.size() <= 1) {
        return Result<void>::failure(graph_error("DIALOGUE-GRAPH-NODE-LAST", "Cannot remove the last dialogue node",
            "Add another node before deleting this one."));
    }
    tree.nodes.erase(std::remove_if(tree.nodes.begin(), tree.nodes.end(),
                          [&](const WorldForgeDialogueNode& node) { return node.id == node_id; }),
        tree.nodes.end());
    for (auto& node : tree.nodes) {
        for (auto& choice : node.choices) {
            if (choice.next_node_id == node_id) choice.next_node_id.clear();
        }
    }
    if (tree.entry_node_id == node_id) tree.entry_node_id = tree.nodes.front().id;
    return Result<void>::success();
}

Result<void> add_dialogue_choice(WorldForgeDialogueTree& tree, const std::string& from_node_id, std::string choice_id,
    std::string text, std::string next_node_id) {
    auto* from = find_mutable_node(tree, from_node_id);
    if (!from) {
        return Result<void>::failure(graph_error("DIALOGUE-GRAPH-NODE-MISSING",
            "Unknown dialogue node: " + from_node_id, "Select an existing from-node."));
    }
    choice_id = sanitize_id(std::move(choice_id));
    if (choice_id.empty()) {
        return Result<void>::failure(
            graph_error("DIALOGUE-GRAPH-CHOICE-ID", "Choice id is required", "Provide a non-empty choice id."));
    }
    if (choice_id_exists(tree, choice_id)) {
        return Result<void>::failure(graph_error("DIALOGUE-GRAPH-CHOICE-DUP",
            "Choice id already exists: " + choice_id, "Choice ids must be unique within the tree."));
    }
    if (!next_node_id.empty() && !find_const_node(tree, next_node_id)) {
        return Result<void>::failure(graph_error("DIALOGUE-GRAPH-NEXT",
            "Unknown next node: " + next_node_id, "Link to an existing node or leave next empty to end."));
    }
    WorldForgeDialogueChoice choice;
    choice.id = std::move(choice_id);
    choice.text = std::move(text);
    choice.next_node_id = std::move(next_node_id);
    from->choices.push_back(std::move(choice));
    return Result<void>::success();
}

Result<void> remove_dialogue_choice(WorldForgeDialogueTree& tree, const std::string& from_node_id,
    const std::string& choice_id) {
    auto* from = find_mutable_node(tree, from_node_id);
    if (!from) {
        return Result<void>::failure(graph_error("DIALOGUE-GRAPH-NODE-MISSING",
            "Unknown dialogue node: " + from_node_id, "Select an existing from-node."));
    }
    const auto before = from->choices.size();
    from->choices.erase(std::remove_if(from->choices.begin(), from->choices.end(),
                            [&](const WorldForgeDialogueChoice& choice) { return choice.id == choice_id; }),
        from->choices.end());
    if (from->choices.size() == before) {
        return Result<void>::failure(graph_error("DIALOGUE-GRAPH-CHOICE-MISSING",
            "Unknown choice on node: " + choice_id, "Select an existing choice."));
    }
    return Result<void>::success();
}

std::vector<std::string> dialogue_reachable_node_ids(const WorldForgeDialogueTree& tree) {
    std::vector<std::string> ordered;
    std::unordered_set<std::string> seen;
    std::queue<std::string> queue;
    if (!tree.entry_node_id.empty() && find_const_node(tree, tree.entry_node_id)) {
        queue.push(tree.entry_node_id);
        seen.insert(tree.entry_node_id);
    }
    while (!queue.empty()) {
        const auto id = queue.front();
        queue.pop();
        ordered.push_back(id);
        const auto* node = find_const_node(tree, id);
        if (!node) continue;
        for (const auto& choice : node->choices) {
            if (choice.next_node_id.empty() || seen.count(choice.next_node_id)) continue;
            if (!find_const_node(tree, choice.next_node_id)) continue;
            seen.insert(choice.next_node_id);
            queue.push(choice.next_node_id);
        }
    }
    return ordered;
}

Result<std::string> duplicate_dialogue_node(WorldForgeDialogueTree& tree, const std::string& node_id) {
    const auto* src = find_const_node(tree, node_id);
    if (!src) {
        return Result<std::string>::failure(
            graph_error("DIALOGUE-GRAPH-NODE-MISSING", "Unknown node: " + node_id, "Select an existing node."));
    }
    const auto new_id = unique_dialogue_node_id(tree, node_id + "_copy");
    WorldForgeDialogueNode copy = *src;
    copy.id = new_id;
    for (auto& choice : copy.choices) {
        choice.id = unique_dialogue_choice_id(tree, choice.id + "_copy");
    }
    tree.nodes.push_back(std::move(copy));
    return Result<std::string>::success(new_id);
}

DialogueGraphNodeKind infer_dialogue_node_kind(const WorldForgeDialogueNode& node) noexcept {
    return node.choices.empty() ? DialogueGraphNodeKind::End : DialogueGraphNodeKind::Dialogue;
}

const char* to_string(DialogueGraphNodeKind kind) noexcept {
    switch (kind) {
    case DialogueGraphNodeKind::End:
        return "End";
    case DialogueGraphNodeKind::Dialogue:
    default:
        return "Dialogue";
    }
}

const char* to_string(DialogueGraphNodeDisplayMode mode) noexcept {
    switch (mode) {
    case DialogueGraphNodeDisplayMode::Compact:
        return "Compact";
    case DialogueGraphNodeDisplayMode::Expanded:
        return "Expanded";
    case DialogueGraphNodeDisplayMode::Standard:
    default:
        return "Standard";
    }
}

std::array<float, 2> dialogue_node_card_size(DialogueGraphNodeDisplayMode mode) noexcept {
    switch (mode) {
    case DialogueGraphNodeDisplayMode::Compact:
        return {140.0f, 48.0f};
    case DialogueGraphNodeDisplayMode::Expanded:
        return {240.0f, 120.0f};
    case DialogueGraphNodeDisplayMode::Standard:
    default:
        return {200.0f, 88.0f};
    }
}

namespace {

std::string to_lower_copy(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

bool contains_ci(const std::string& haystack, const std::string& needle_lower) {
    if (needle_lower.empty()) return true;
    return to_lower_copy(haystack).find(needle_lower) != std::string::npos;
}

} // namespace

bool dialogue_node_matches_query(const WorldForgeDialogueNode& node, const std::string& query) {
    const auto q = to_lower_copy(query);
    if (q.empty()) return true;
    if (contains_ci(node.id, q) || contains_ci(node.speaker_id, q) || contains_ci(node.line, q)) return true;
    for (const auto& choice : node.choices) {
        if (contains_ci(choice.text, q) || contains_ci(choice.id, q) || contains_ci(choice.next_node_id, q))
            return true;
        for (const auto& flag : choice.set_flags) {
            if (contains_ci(flag, q)) return true;
        }
    }
    return false;
}

std::vector<std::string> dialogue_search_node_ids(const WorldForgeDialogueTree& tree, const std::string& query) {
    std::vector<std::string> out;
    for (const auto& node : tree.nodes) {
        if (dialogue_node_matches_query(node, query)) out.push_back(node.id);
    }
    return out;
}

} // namespace engine
