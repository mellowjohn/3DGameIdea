#pragma once

#include "engine/core/result.h"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine {

/// Weld from a character skin joint to the item's world mesh. See engine::BoneWeld for the math.
struct ItemHandAttach {
    /// Skin joint name (e.g. RightHand). Empty uses the engine default for one-handers.
    std::string joint;
    std::array<float, 3> grip_offset{0.0f, 0.0f, 0.0f};
    /// Authoring-friendly XYZ Euler degrees (roll Z, then pitch X, then yaw Y).
    std::array<float, 3> grip_euler_deg{0.0f, 0.0f, 0.0f};
    /// Extra scale on top of the character scale the joint chain already carries.
    std::array<float, 3> grip_scale{1.0f, 1.0f, 1.0f};
    /// Optional draw/pose clip on the held worldMesh (e.g. `bow_draw`). Empty falls back to
    /// sampling `bow_draw` when present on a skinned weapon mesh.
    std::string draw_clip;
};

struct ItemDef {
    std::string id;
    std::string display_name;
    std::string kind; // weapon | armor | trinket | consumable | material
    std::vector<std::string> tags;
    std::string icon;
    std::string world_mesh;
    bool icon_only = false;
    std::string source;
    std::string notes;
    ItemHandAttach hand_attach{};
    /// Project-relative JSON file this def was loaded from (for Save during play-test).
    std::string source_path;
};

struct ItemLootEntry {
    std::string item_id;
    int count = 1;
    int weight = 1;
};

struct ItemLootTable {
    std::string id;
    std::vector<std::string> container_kinds;
    std::vector<ItemLootEntry> entries;
};

struct ItemCatalogAsset {
    int schema_version = 1;
    std::string id;
    std::string notes;
    std::vector<ItemDef> entities;
    std::vector<ItemLootTable> loot_tables;
    std::unordered_map<std::string, std::size_t> by_id;

    [[nodiscard]] static Result<ItemCatalogAsset> load(const std::filesystem::path& path);
    [[nodiscard]] static Result<ItemCatalogAsset> parse(const std::string& text,
        const std::string& source_name = "items.json");
    [[nodiscard]] const ItemDef* find(const std::string& item_id) const;
    [[nodiscard]] ItemDef* find_mutable(const std::string& item_id);
    [[nodiscard]] const ItemLootTable* find_loot_table(const std::string& table_id) const;
};

/// Patch `handAttach` on one entity inside its source catalog JSON and write the file.
[[nodiscard]] Result<void> save_item_hand_attach(const std::filesystem::path& project_root, const ItemDef& def);

/// Load every `assets/items/*.json` under the project (merge entities by id; later files win).
[[nodiscard]] Result<ItemCatalogAsset> load_project_item_catalog(const std::filesystem::path& project_root);

} // namespace engine
