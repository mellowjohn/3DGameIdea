#pragma once

#include "engine/assets/world_forge_dialogues_asset.h"
#include "engine/core/result.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine {

/// Ephemeral layout positions keyed by dialogue node id (TICKET-0053).
using DialogueGraphPositions = std::unordered_map<std::string, std::array<float, 2>>;

[[nodiscard]] std::string unique_dialogue_node_id(const WorldForgeDialogueTree& tree, const std::string& preferred);
[[nodiscard]] std::string unique_dialogue_choice_id(const WorldForgeDialogueTree& tree, const std::string& preferred);

/// Layered BFS layout from `entry_node_id`. Existing positions are kept when `preserve_existing` is true.
void layout_dialogue_graph(const WorldForgeDialogueTree& tree, DialogueGraphPositions& positions,
    bool preserve_existing = false);

[[nodiscard]] Result<void> add_dialogue_node(WorldForgeDialogueTree& tree, std::string id, std::string speaker_id,
    std::string line);
/// Removes a node, clears dangling choice targets, and retargets entry when needed.
[[nodiscard]] Result<void> remove_dialogue_node(WorldForgeDialogueTree& tree, const std::string& node_id);

[[nodiscard]] Result<void> add_dialogue_choice(WorldForgeDialogueTree& tree, const std::string& from_node_id,
    std::string choice_id, std::string text, std::string next_node_id);
[[nodiscard]] Result<void> remove_dialogue_choice(WorldForgeDialogueTree& tree, const std::string& from_node_id,
    const std::string& choice_id);

/// Walk reachable nodes from entry via non-empty nextNodeId edges (headless reachability).
[[nodiscard]] std::vector<std::string> dialogue_reachable_node_ids(const WorldForgeDialogueTree& tree);

/// Duplicate a node (new unique id) and optionally deep-copy choices with retargeted ids.
[[nodiscard]] Result<std::string> duplicate_dialogue_node(WorldForgeDialogueTree& tree, const std::string& node_id);

enum class DialogueGraphNodeDisplayMode : std::uint8_t { Compact, Standard, Expanded };

enum class DialogueGraphNodeKind : std::uint8_t { Dialogue, End };

[[nodiscard]] DialogueGraphNodeKind infer_dialogue_node_kind(const WorldForgeDialogueNode& node) noexcept;
[[nodiscard]] const char* to_string(DialogueGraphNodeKind kind) noexcept;
[[nodiscard]] const char* to_string(DialogueGraphNodeDisplayMode mode) noexcept;

/// World-space card size for the given display mode (unscaled; multiply by zoom in UI).
[[nodiscard]] std::array<float, 2> dialogue_node_card_size(DialogueGraphNodeDisplayMode mode) noexcept;

/// Case-insensitive search across node id, speaker, line, and choice setFlags (TICKET-0167).
[[nodiscard]] bool dialogue_node_matches_query(const WorldForgeDialogueNode& node, const std::string& query);
[[nodiscard]] std::vector<std::string> dialogue_search_node_ids(const WorldForgeDialogueTree& tree,
    const std::string& query);

} // namespace engine
