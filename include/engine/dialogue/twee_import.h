#pragma once

#include "engine/assets/world_forge_dialogues_asset.h"
#include "engine/core/result.h"

#include <filesystem>
#include <string>

namespace engine {

struct TweeImportOptions {
    /// Required dialogue tree id to create or replace.
    std::string tree_id;
    std::string display_name;
    std::string parent_quest_id;
    std::string story_ref;
    /// When empty, prefer "prologue" if present, else first node.
    std::string entry_node_id;
    /// Relative or absolute path recorded into storyRef when story_ref is empty.
    std::string source_label;
};

/// Parse Harlowe-style Twine `.twee` text into one World Forge dialogue tree.
[[nodiscard]] Result<WorldForgeDialogueTree> import_twee_dialogue_tree(const std::string& twee_text,
    const TweeImportOptions& options);

[[nodiscard]] Result<WorldForgeDialogueTree> import_twee_dialogue_tree_file(const std::filesystem::path& twee_path,
    const TweeImportOptions& options);

/// Insert or replace `tree` inside `asset` by tree id, then validate.
[[nodiscard]] Result<void> upsert_dialogue_tree(WorldForgeDialoguesAsset& asset, WorldForgeDialogueTree tree);

} // namespace engine
