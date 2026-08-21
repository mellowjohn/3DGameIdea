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

/// Bind-space note: skinned armor draws with the player visual transform and a
/// per-piece bone palette (player animation locals × the shell's inverse binds).
/// Do not apply these values as a post-skin instance TRS (origin scale pulls
/// shells off the skeleton). Fit in Blockbench, rebake with matchPlayerBake.
struct ItemArmorAttach {
    std::array<float, 3> local_offset{0.0f, 0.0f, 0.0f};
    std::array<float, 3> local_euler_deg{0.0f, 0.0f, 0.0f};
    std::array<float, 3> local_scale{1.0f, 1.0f, 1.0f};
};

struct ItemStatModifier {
    std::string id;
    float value = 0.0f;
};

/// Applied to the hurt target when this weapon scores a hit (DoT / status).
struct ItemOnHitStatus {
    std::string status; // poison | bleed
    float damage_per_tick = 1.0f;
    float duration_seconds = 6.0f;
    float tick_interval_seconds = 1.0f;
};

/// Authored combat / equipment numbers. Tooltips show the block; equipped armor/trinkets
/// and the active hotbar weapon also sum `modifiers` into live player totals.
/// Damage range + attacksPerSecond yield DPS; modifiers are flat stat grants while equipped.
struct ItemStats {
    float damage_min = 0.0f;
    float damage_max = 0.0f;
    float attacks_per_second = 0.0f;
    float heal = 0.0f;
    std::vector<ItemStatModifier> modifiers;
    std::vector<ItemOnHitStatus> on_hit;

    [[nodiscard]] bool empty() const noexcept {
        return !(damage_max > 0.0f || damage_min > 0.0f || attacks_per_second > 0.0f || heal > 0.0f)
            && modifiers.empty() && on_hit.empty();
    }
    [[nodiscard]] bool has_weapon_damage() const noexcept {
        return damage_min > 0.0f || damage_max > 0.0f;
    }
    [[nodiscard]] float dps() const noexcept {
        if (!has_weapon_damage() || !(attacks_per_second > 0.0f)) return 0.0f;
        const float lo = damage_min > 0.0f ? damage_min : damage_max;
        const float hi = damage_max > 0.0f ? damage_max : damage_min;
        return 0.5f * (lo + hi) * attacks_per_second;
    }
};

/// Spell flavor on `magic` weapons. `magic_fire` / `magic_frost` / `magic_lightning`
/// tags select a school; otherwise the held focus is Arcane (starter Guild Rune Focus).
enum class MagicElement { Arcane, Fire, Frost, Lightning };

[[nodiscard]] MagicElement magic_element_from_tags(const std::vector<std::string>& tags) noexcept;
[[nodiscard]] const char* magic_element_id(MagicElement element) noexcept;
[[nodiscard]] const char* magic_element_display_name(MagicElement element) noexcept;

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
    ItemArmorAttach armor_attach{};
    ItemStats stats{};
    /// Project-relative JSON file this def was loaded from (for Save during play-test).
    std::string source_path;
};

[[nodiscard]] std::string item_stat_display_name(const std::string& id);
[[nodiscard]] std::vector<std::string> format_item_stat_lines(const ItemDef& def);
[[nodiscard]] std::string format_item_stat_text(const ItemDef& def);

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
/// Patch `armorAttach` on one entity inside its source catalog JSON and write the file.
[[nodiscard]] Result<void> save_item_armor_attach(const std::filesystem::path& project_root, const ItemDef& def);

/// Load every `assets/items/*.json` under the project (merge entities by id; later files win).
[[nodiscard]] Result<ItemCatalogAsset> load_project_item_catalog(const std::filesystem::path& project_root);

} // namespace engine
