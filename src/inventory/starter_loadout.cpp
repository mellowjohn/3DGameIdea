#include "engine/inventory/starter_loadout.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace engine {
namespace {

std::string lower_copy(std::string_view value) {
    std::string out(value);
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

} // namespace

std::string normalize_starter_archetype_id(std::string_view raw) {
    const auto key = lower_copy(raw);
    if (key.empty()) return kDefaultPlayTestStarterArchetypeId;
    if (key == "ashfell_blade" || key == "ashfell" || key == "blade" || key == "melee" || key == "squire" ||
        key == "fighter" || key == "warrior")
        return "ashfell_blade";
    if (key == "outrider" || key == "archer" || key == "ranged" || key == "bow" || key == "ranger")
        return "outrider";
    if (key == "runecaster" || key == "acolyte" || key == "magic" || key == "mage" || key == "caster" ||
        key == "rune")
        return "runecaster";
    return std::string(key);
}

std::string default_starter_weapon_item_id(std::string_view archetype_id) {
    const auto id = normalize_starter_archetype_id(archetype_id);
    if (id == "outrider") return "outrider_shortbow";
    if (id == "runecaster") return "guild_rune_focus";
    return "ashfell_arming_sword";
}

std::string resolve_starter_weapon_item_id(std::string_view archetype_id,
    const WorldForgeArchetypesAsset* archetypes) {
    const auto id = normalize_starter_archetype_id(archetype_id);
    if (archetypes) {
        if (const auto* entity = archetypes->find_entity(id)) {
            if (!entity->starter_weapon_item_id.empty()) return entity->starter_weapon_item_id;
        }
    }
    return default_starter_weapon_item_id(id);
}

} // namespace engine
