#pragma once

#include "engine/core/result.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace engine {

/// Act Zero (or other act) MVP launch checklist status — authoritative Overview progress.
enum class WorldForgeMvpItemStatus : std::uint8_t { Todo, Wip, Done, Blocked };

/// Launch priority: P0 = do now for playable demo, P1 = next, P2 = later polish.
enum class WorldForgeMvpItemPriority : std::uint8_t { P0, P1, P2 };

/// Owner bounce lens for Act Zero MVP work.
enum class WorldForgeMvpWorkstream : std::uint8_t {
    Art,
    Effects,
    Coding,
    Project,
    Storyline,
    Gameplay,
    Combat,
    Archetype,
    /// In-game events / cinematics / theatrical sequences players watch rather than freely play.
    Cinematics,
    /// Player-facing UI / UX (HUD, dialogue presentation, menus, prompts, accessibility).
    UiUx
};

/// Optional soft references for display (not hard-validated in schema v1).
struct WorldForgeMvpItemRefs {
    std::string story_ref;
    std::string quest_id;
    std::string dialogue_id;
    std::string asset_path;
    std::string ticket_id;
};

struct WorldForgeMvpChecklistItem {
    std::string id;
    std::string title;
    WorldForgeMvpItemStatus status = WorldForgeMvpItemStatus::Todo;
    WorldForgeMvpItemPriority priority = WorldForgeMvpItemPriority::P1;
    WorldForgeMvpWorkstream workstream = WorldForgeMvpWorkstream::Coding;
    /// Short one-liner for table rows.
    std::string notes;
    /// Longer context / acceptance / how-to for the expand/detail pane.
    std::string description;
    /// Repo-relative PNG paths (e.g. `context/art/tier1-bush-variants-concept.png`).
    std::vector<std::string> image_paths;
    /// Soft prerequisite checklist item ids that should be done first.
    std::vector<std::string> depends_on;
    /// When true, this row is a verifiable Act MVP acceptance sink. Overview
    /// "next unblocker" ranks actionable work by progress toward open goals.
    bool goal = false;
    WorldForgeMvpItemRefs refs;
};

struct WorldForgeMvpCategory {
    std::string id;
    std::string title;
    std::vector<WorldForgeMvpChecklistItem> items;
};

/// Curated MVP readiness checklist for an act lens (TICKET Overview Act Zero).
struct WorldForgeMvpReadinessAsset {
    int schema_version = 1;
    std::string id;
    /// Canonical act id (`act0` … `act4`).
    std::string act_id = "act0";
    std::vector<WorldForgeMvpCategory> categories;

    [[nodiscard]] Result<void> validate() const;
    [[nodiscard]] int count_items() const;
    [[nodiscard]] int count_done() const;
    [[nodiscard]] float done_fraction() const;
    [[nodiscard]] WorldForgeMvpChecklistItem* find_item(const std::string& item_id);
    [[nodiscard]] const WorldForgeMvpChecklistItem* find_item(const std::string& item_id) const;
    [[nodiscard]] static Result<WorldForgeMvpReadinessAsset> load(const std::filesystem::path& path);
    [[nodiscard]] static Result<WorldForgeMvpReadinessAsset> parse(const std::string& text,
        const std::string& source_name = "act0_mvp_readiness.worldforge.json");
    [[nodiscard]] Result<void> save_atomic(const std::filesystem::path& path) const;
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static Result<void> validate_file(const std::filesystem::path& path);
};

[[nodiscard]] const char* to_string(WorldForgeMvpItemStatus value) noexcept;
[[nodiscard]] const char* to_string(WorldForgeMvpItemPriority value) noexcept;
[[nodiscard]] const char* to_string(WorldForgeMvpWorkstream value) noexcept;
[[nodiscard]] const char* world_forge_mvp_workstream_label(WorldForgeMvpWorkstream value) noexcept;
[[nodiscard]] const char* world_forge_mvp_priority_label(WorldForgeMvpItemPriority value) noexcept;

inline constexpr WorldForgeMvpWorkstream k_world_forge_mvp_workstreams[] = {
    WorldForgeMvpWorkstream::Art,
    WorldForgeMvpWorkstream::Effects,
    WorldForgeMvpWorkstream::Coding,
    WorldForgeMvpWorkstream::Project,
    WorldForgeMvpWorkstream::Storyline,
    WorldForgeMvpWorkstream::Gameplay,
    WorldForgeMvpWorkstream::Combat,
    WorldForgeMvpWorkstream::Archetype,
    WorldForgeMvpWorkstream::Cinematics,
    WorldForgeMvpWorkstream::UiUx,
};
inline constexpr int k_world_forge_mvp_workstream_count = 10;

[[nodiscard]] std::filesystem::path default_world_forge_mvp_readiness_path(
    const std::filesystem::path& project_root);

} // namespace engine
