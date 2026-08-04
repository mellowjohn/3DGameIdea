#pragma once

#include "engine/assets/world_forge_archetypes_asset.h"

#include <string>
#include <string_view>

namespace engine {

/// Normalize play-test / command archetype aliases to World Forge entity ids.
/// Accepts full ids (`ashfell_blade`, `outrider`, `runecaster`) plus lane shorts
/// (`ashfell`, `melee`, `bow`, `magic`, legacy Squire/Archer/Acolyte).
[[nodiscard]] std::string normalize_starter_archetype_id(std::string_view raw);

/// Default Act 0 starter weapon for an archetype id when World Forge has no override.
[[nodiscard]] std::string default_starter_weapon_item_id(std::string_view archetype_id);

/// Resolve starter weapon: optional World Forge `starterWeaponItemId`, else built-in map.
[[nodiscard]] std::string resolve_starter_weapon_item_id(std::string_view archetype_id,
    const WorldForgeArchetypesAsset* archetypes = nullptr);

/// Shared Act 0 consumable granted with every starter kit.
inline constexpr const char* kAct0StarterBandageItemId = "field_bandage";
inline constexpr int kAct0StarterBandageCount = 2;
inline constexpr const char* kDefaultPlayTestStarterArchetypeId = "ashfell_blade";

} // namespace engine
