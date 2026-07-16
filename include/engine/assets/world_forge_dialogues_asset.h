#pragma once

#include "engine/core/result.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace engine {

enum class WorldForgeDialogueCanonStatus : std::uint8_t { Established, Draft, Proposal, Open };
enum class WorldForgeDialogueSpeakerTarget : std::uint8_t { Node, Faction, Narrator };

struct WorldForgeDialogueChoice {
    std::string id;
    std::string text;
    /// Empty nextNodeId ends the conversation after this choice.
    std::string next_node_id;
    std::vector<std::string> set_flags;
};

struct WorldForgeDialogueNode {
    std::string id;
    std::string speaker_id;
    std::string line;
    std::vector<WorldForgeDialogueChoice> choices;
};

struct WorldForgeDialogueTree {
    std::string id;
    /// Optional; when set, this tree is a child of a quest stage (DEC-0026).
    std::string parent_quest_id;
    std::string display_name;
    WorldForgeDialogueCanonStatus canon_status = WorldForgeDialogueCanonStatus::Draft;
    std::string summary;
    std::string story_ref;
    std::string entry_node_id;
    std::vector<WorldForgeDialogueNode> nodes;
    std::vector<std::string> tags;
    std::vector<std::string> open_questions;
};

struct WorldForgeDialoguesAsset {
    int schema_version = 1;
    std::string id;
    std::vector<WorldForgeDialogueTree> trees;

    [[nodiscard]] Result<void> validate() const;
    /// When non-empty, every non-empty parentQuestId must be listed.
    [[nodiscard]] Result<void> validate_quest_refs(const std::unordered_set<std::string>& known_quest_ids) const;
    [[nodiscard]] const WorldForgeDialogueTree* find_tree(const std::string& tree_id) const;
    [[nodiscard]] WorldForgeDialogueTree* find_tree(const std::string& tree_id);
    [[nodiscard]] static Result<WorldForgeDialoguesAsset> load(const std::filesystem::path& path);
    [[nodiscard]] static Result<WorldForgeDialoguesAsset> parse(const std::string& text,
        const std::string& source_name = "dialogues.worldforge.json");
    [[nodiscard]] Result<void> save_atomic(const std::filesystem::path& path) const;
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static Result<void> validate_file(const std::filesystem::path& path);
    [[nodiscard]] static Result<void> validate_file(const std::filesystem::path& path,
        const std::unordered_set<std::string>& known_quest_ids);
};

[[nodiscard]] const char* to_string(WorldForgeDialogueCanonStatus value) noexcept;
[[nodiscard]] const char* to_string(WorldForgeDialogueSpeakerTarget value) noexcept;

[[nodiscard]] std::filesystem::path default_world_forge_dialogues_path(const std::filesystem::path& project_root);

} // namespace engine
