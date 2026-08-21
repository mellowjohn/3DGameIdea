#include "engine/inventory/inventory_runtime.h"

#include "engine/core/error.h"
#include "engine/inventory/starter_loadout.h"
#include "engine/ui/hud_runtime.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace engine {
namespace {

EngineError inv_error(std::string code, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Validation, "inventory",
        std::move(message), ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

bool has_tag(const ItemDef& def, const std::string& tag) {
    return std::find(def.tags.begin(), def.tags.end(), tag) != def.tags.end();
}

bool is_ammo(const ItemDef& def) {
    return has_tag(def, "ammo") || def.kind == "ammo";
}

void refill_marbles(std::vector<std::uint8_t>& bag, int crit, int hit, std::uint32_t& seed) {
    bag.clear();
    bag.reserve(static_cast<std::size_t>(std::max(0, crit) + std::max(0, hit)));
    for (int i = 0; i < crit; ++i) bag.push_back(1);
    for (int i = 0; i < hit; ++i) bag.push_back(0);
    if (bag.size() <= 1) return;
    std::mt19937 rng(seed++);
    std::shuffle(bag.begin(), bag.end(), rng);
}

void refill_damage_marbles(std::vector<int>& bag, int lo, int hi, std::uint32_t& seed) {
    bag.clear();
    if (lo > hi) std::swap(lo, hi);
    bag.reserve(static_cast<std::size_t>(hi - lo + 1));
    for (int v = lo; v <= hi; ++v) bag.push_back(v);
    if (bag.size() <= 1) return;
    std::mt19937 rng(seed++);
    std::shuffle(bag.begin(), bag.end(), rng);
}

} // namespace

WeaponSchool weapon_school_from_tags(const std::vector<std::string>& tags) noexcept {
    bool melee = false;
    bool ranged = false;
    bool magic = false;
    for (const auto& tag : tags) {
        if (tag == "melee") melee = true;
        else if (tag == "ranged") ranged = true;
        else if (tag == "magic") magic = true;
    }
    if (magic) return WeaponSchool::Magic;
    if (ranged) return WeaponSchool::Ranged;
    if (melee) return WeaponSchool::Melee;
    return WeaponSchool::None;
}

const char* weapon_school_id(WeaponSchool school) noexcept {
    switch (school) {
    case WeaponSchool::Melee: return "melee";
    case WeaponSchool::Ranged: return "ranged";
    case WeaponSchool::Magic: return "magic";
    case WeaponSchool::None: return "none";
    }
    return "none";
}

float attribute_damage_multiplier(float attribute) noexcept {
    if (!(attribute > 0.0f) || !std::isfinite(attribute)) return 1.0f;
    return 1.0f + kDamagePerAttribute * attribute;
}

CritMarbleMix crit_marbles_from_chance(float percent) noexcept {
    CritMarbleMix mix;
    if (!(percent > 0.0f) || !std::isfinite(percent)) return mix;
    const float clamped = std::clamp(percent, 0.0f, 100.0f);
    mix.crit = static_cast<int>(std::lround(clamped * static_cast<float>(kCritBagSize) / 100.0f));
    mix.crit = std::clamp(mix.crit, 0, kCritBagSize);
    mix.hit = kCritBagSize - mix.crit;
    if (mix.crit <= 0) {
        mix.hit = 0;
    }
    return mix;
}

float resist_mitigation(float incoming, float resist) noexcept {
    if (!(incoming > 0.0f) || !std::isfinite(incoming)) return 0.0f;
    if (!(resist > 0.0f) || !std::isfinite(resist)) return incoming;
    return incoming * 100.0f / (100.0f + resist);
}

Result<void> InventoryRuntime::bind(const ItemCatalogAsset* catalog) {
    if (!catalog) {
        return Result<void>::failure(
            inv_error("INV-CATALOG", "InventoryRuntime requires an item catalog", "Call bind with a loaded catalog."));
    }
    catalog_ = catalog;
    bag_.assign(static_cast<std::size_t>(bag_capacity_), InventoryStack{});
    hotbar_ = {};
    equipped_ = {};
    ammo_.clear();
    quest_inventory_.clear();
    containers_.clear();
    open_container_id_.clear();
    selected_hotbar_ = 0;
    ui_selection_ = {};
    gold_ = 0;
    crit_remaining_.clear();
    crit_bag_crit_ = 0;
    crit_bag_hit_ = 0;
    damage_remaining_.clear();
    damage_bag_lo_ = 0;
    damage_bag_hi_ = 0;
    return Result<void>::success();
}

void InventoryRuntime::reset() noexcept {
    catalog_ = nullptr;
    bag_.clear();
    hotbar_ = {};
    equipped_ = {};
    ammo_.clear();
    quest_inventory_.clear();
    containers_.clear();
    open_container_id_.clear();
    selected_hotbar_ = 0;
    ui_selection_ = {};
    gold_ = 0;
    crit_remaining_.clear();
    crit_bag_crit_ = 0;
    crit_bag_hit_ = 0;
    damage_remaining_.clear();
    damage_bag_lo_ = 0;
    damage_bag_hi_ = 0;
}

Result<void> InventoryRuntime::require_bound() const {
    if (!catalog_) {
        return Result<void>::failure(
            inv_error("INV-UNBOUND", "InventoryRuntime is not bound", "Call bind before inventory ops."));
    }
    return Result<void>::success();
}

Result<const ItemDef*> InventoryRuntime::require_def(const std::string& item_id) const {
    if (const auto bound = require_bound(); !bound) return Result<const ItemDef*>::failure(bound.error());
    const ItemDef* def = catalog_->find(item_id);
    if (!def) {
        return Result<const ItemDef*>::failure(
            inv_error("INV-UNKNOWN-ITEM", "Unknown item id: " + item_id, "Use an authored item catalog id."));
    }
    return Result<const ItemDef*>::success(def);
}

const ItemDef* InventoryRuntime::find_def(const std::string& item_id) const {
    return catalog_ ? catalog_->find(item_id) : nullptr;
}

int InventoryRuntime::max_stack_for(const ItemDef& def) const {
    if (is_ammo(def)) return kInventoryAmmoMaxStack;
    if (def.kind == "weapon" || def.kind == "armor" || def.kind == "trinket") return 1;
    return kInventoryMaxStack;
}

bool InventoryRuntime::kind_fits_hotbar(const ItemDef& def) const {
    return def.kind == "weapon" || def.kind == "consumable" || def.kind == "material" || has_tag(def, "utility");
}

bool InventoryRuntime::kind_fits_equip(const ItemDef& def, const std::string& equip_slot) const {
    if (equip_slot == "head" || equip_slot == "chest" || equip_slot == "legs")
        return def.kind == "armor";
    if (equip_slot.rfind("trinket", 0) == 0) return def.kind == "trinket";
    return false;
}

int InventoryRuntime::first_empty_bag() const {
    for (int i = 0; i < static_cast<int>(bag_.size()); ++i) {
        if (bag_[static_cast<std::size_t>(i)].empty()) return i;
    }
    return -1;
}

int InventoryRuntime::first_empty_hotbar() const {
    for (int i = 0; i < kInventoryHotbarSlots; ++i) {
        if (hotbar_[static_cast<std::size_t>(i)].empty()) return i;
    }
    return -1;
}

int InventoryRuntime::first_empty_trinket() const {
    for (int i = 0; i < kInventoryTrinketSlots; ++i) {
        if (equipped_.trinkets[static_cast<std::size_t>(i)].empty()) return i;
    }
    return -1;
}

int InventoryRuntime::first_empty_container() const {
    const auto* slots = open_container_slots();
    if (!slots) return -1;
    for (int i = 0; i < static_cast<int>(slots->size()); ++i) {
        if ((*slots)[static_cast<std::size_t>(i)].empty()) return i;
    }
    return -1;
}

std::vector<InventoryStack>* InventoryRuntime::open_container_slots() {
    if (open_container_id_.empty()) return nullptr;
    auto it = containers_.find(open_container_id_);
    if (it == containers_.end()) return nullptr;
    return &it->second;
}

const std::vector<InventoryStack>* InventoryRuntime::open_container_slots() const {
    return const_cast<InventoryRuntime*>(this)->open_container_slots();
}

InventoryStack* InventoryRuntime::stack_at(const std::string& region, int index, const std::string& equip_slot) {
    if (region == "bag") {
        if (index < 0 || index >= static_cast<int>(bag_.size())) return nullptr;
        return &bag_[static_cast<std::size_t>(index)];
    }
    if (region == "hotbar") {
        if (index < 0 || index >= kInventoryHotbarSlots) return nullptr;
        return &hotbar_[static_cast<std::size_t>(index)];
    }
    if (region == "ammo") {
        if (index < 0 || index >= static_cast<int>(ammo_.size())) return nullptr;
        return &ammo_[static_cast<std::size_t>(index)];
    }
    if (region == "container") {
        auto* slots = open_container_slots();
        if (!slots) return nullptr;
        if (index < 0 || index >= static_cast<int>(slots->size())) return nullptr;
        return &(*slots)[static_cast<std::size_t>(index)];
    }
    if (region == "equip") {
        if (equip_slot == "head") return &equipped_.head;
        if (equip_slot == "chest") return &equipped_.chest;
        if (equip_slot == "legs") return &equipped_.legs;
        if (equip_slot.rfind("trinket", 0) == 0 && equip_slot.size() == 8) {
            const int t = equip_slot[7] - '0';
            if (t >= 0 && t < kInventoryTrinketSlots) return &equipped_.trinkets[static_cast<std::size_t>(t)];
        }
    }
    return nullptr;
}

const InventoryStack* InventoryRuntime::stack_at(const std::string& region, int index,
    const std::string& equip_slot) const {
    return const_cast<InventoryRuntime*>(this)->stack_at(region, index, equip_slot);
}

Result<void> InventoryRuntime::add_to_bag_or_ammo(const ItemDef& def, int count) {
    if (count <= 0) {
        return Result<void>::failure(
            inv_error("INV-COUNT", "Grant count must be positive", "Pass count >= 1."));
    }
    int remaining = count;
    const int max_stack = max_stack_for(def);

    if (is_ammo(def)) {
        for (auto& stack : ammo_) {
            if (stack.item_id == def.id && stack.count < max_stack) {
                const int space = max_stack - stack.count;
                const int add = std::min(space, remaining);
                stack.count += add;
                remaining -= add;
                if (remaining <= 0) return Result<void>::success();
            }
        }
        while (remaining > 0) {
            const int add = std::min(max_stack, remaining);
            ammo_.push_back(InventoryStack{def.id, add});
            remaining -= add;
        }
        return Result<void>::success();
    }

    for (auto& stack : bag_) {
        if (!stack.empty() && stack.item_id == def.id && stack.count < max_stack) {
            const int space = max_stack - stack.count;
            const int add = std::min(space, remaining);
            stack.count += add;
            remaining -= add;
            if (remaining <= 0) return Result<void>::success();
        }
    }
    while (remaining > 0) {
        const int slot = first_empty_bag();
        if (slot < 0) {
            return Result<void>::failure(
                inv_error("INV-BAG-FULL", "Bag is full", "Free a bag slot or increase capacity."));
        }
        const int add = std::min(max_stack, remaining);
        bag_[static_cast<std::size_t>(slot)] = InventoryStack{def.id, add};
        remaining -= add;
    }
    return Result<void>::success();
}

Result<void> InventoryRuntime::grant(const std::string& item_id, int count) {
    const auto def = require_def(item_id);
    if (!def) return Result<void>::failure(def.error());
    return add_to_bag_or_ammo(*def.value(), count);
}

Result<void> InventoryRuntime::remove(const std::string& item_id, int count) {
    if (const auto bound = require_bound(); !bound) return bound;
    if (count <= 0) {
        return Result<void>::failure(
            inv_error("INV-COUNT", "Remove count must be positive", "Pass count >= 1."));
    }
    if (count_item(item_id) < count) {
        return Result<void>::failure(
            inv_error("INV-REMOVE", "Not enough of item to remove: " + item_id, "Grant the item first."));
    }
    int remaining = count;
    auto consume = [&](std::vector<InventoryStack>& stacks) {
        for (auto& stack : stacks) {
            if (stack.item_id != item_id || stack.empty()) continue;
            const int take = std::min(stack.count, remaining);
            stack.count -= take;
            remaining -= take;
            if (stack.count <= 0) stack = {};
            if (remaining <= 0) return;
        }
    };
    consume(bag_);
    if (remaining > 0) consume(ammo_);
    if (remaining > 0) {
        for (auto& stack : hotbar_) {
            if (stack.item_id != item_id || stack.empty()) continue;
            const int take = std::min(stack.count, remaining);
            stack.count -= take;
            remaining -= take;
            if (stack.count <= 0) stack = {};
            if (remaining <= 0) break;
        }
    }
    if (remaining > 0) {
        return Result<void>::failure(
            inv_error("INV-REMOVE", "Not enough of item to remove: " + item_id, "Grant the item first."));
    }
    return Result<void>::success();
}

int InventoryRuntime::count_item(const std::string& item_id) const {
    if (item_id.empty() || !catalog_) return 0;
    int total = 0;
    auto accumulate = [&](const std::vector<InventoryStack>& stacks) {
        for (const auto& stack : stacks) {
            if (stack.item_id == item_id && !stack.empty()) total += stack.count;
        }
    };
    accumulate(bag_);
    accumulate(ammo_);
    for (const auto& stack : hotbar_) {
        if (stack.item_id == item_id && !stack.empty()) total += stack.count;
    }
    return total;
}

Result<void> InventoryRuntime::set_hotbar(int slot, const std::string& item_id, int count) {
    const auto def = require_def(item_id);
    if (!def) return Result<void>::failure(def.error());
    if (slot < 0 || slot >= kInventoryHotbarSlots) {
        return Result<void>::failure(
            inv_error("INV-HOTBAR-SLOT", "Hotbar slot out of range", "Use slots 0..7."));
    }
    if (!kind_fits_hotbar(*def.value())) {
        return Result<void>::failure(
            inv_error("INV-HOTBAR-KIND", "Item kind cannot go on hotbar: " + def.value()->kind,
                "Use weapon/consumable/utility items on the hotbar."));
    }
    if (count <= 0) count = 1;
    hotbar_[static_cast<std::size_t>(slot)] = InventoryStack{item_id, std::min(count, max_stack_for(*def.value()))};
    return Result<void>::success();
}

Result<void> InventoryRuntime::clear_hotbar(int slot) {
    if (const auto bound = require_bound(); !bound) return bound;
    if (slot < 0 || slot >= kInventoryHotbarSlots) {
        return Result<void>::failure(
            inv_error("INV-HOTBAR-SLOT", "Hotbar slot out of range", "Use slots 0..7."));
    }
    hotbar_[static_cast<std::size_t>(slot)] = {};
    return Result<void>::success();
}

Result<void> InventoryRuntime::set_equip(const std::string& equip_slot, const std::string& item_id, int count) {
    const auto def = require_def(item_id);
    if (!def) return Result<void>::failure(def.error());
    if (!kind_fits_equip(*def.value(), equip_slot)) {
        return Result<void>::failure(
            inv_error("INV-EQUIP-KIND", "Item kind does not fit equip slot " + equip_slot,
                "Armor → head/chest/legs; trinkets → trinket0..3."));
    }
    InventoryStack* dest = stack_at("equip", -1, equip_slot);
    if (!dest) {
        return Result<void>::failure(
            inv_error("INV-EQUIP-SLOT", "Unknown equip slot: " + equip_slot,
                "Use head, chest, legs, or trinket0..trinket3."));
    }
    if (count <= 0) count = 1;
    *dest = InventoryStack{item_id, 1};
    return Result<void>::success();
}

Result<void> InventoryRuntime::clear_equip(const std::string& equip_slot) {
    if (const auto bound = require_bound(); !bound) return bound;
    InventoryStack* dest = stack_at("equip", -1, equip_slot);
    if (!dest) {
        return Result<void>::failure(
            inv_error("INV-EQUIP-SLOT", "Unknown equip slot: " + equip_slot,
                "Use head, chest, legs, or trinket0..trinket3."));
    }
    *dest = {};
    return Result<void>::success();
}

Result<void> InventoryRuntime::move_to(const std::string& from_region, int from_index,
    const std::string& from_equip_slot, const std::string& to_region, int to_index,
    const std::string& to_equip_slot) {
    if (const auto bound = require_bound(); !bound) return bound;
    InventoryStack* from = stack_at(from_region, from_index, from_equip_slot);
    if (!from || from->empty()) {
        return Result<void>::failure(
            inv_error("INV-MOVE-FROM", "Source inventory slot is empty", "Select a filled slot."));
    }
    const auto def = require_def(from->item_id);
    if (!def) return Result<void>::failure(def.error());

    if (to_region == "hotbar") {
        int slot = to_index;
        if (slot < 0) slot = first_empty_hotbar();
        if (slot < 0) {
            return Result<void>::failure(
                inv_error("INV-HOTBAR-FULL", "Hotbar is full", "Clear a hotbar slot first."));
        }
        if (!kind_fits_hotbar(*def.value())) {
            return Result<void>::failure(
                inv_error("INV-HOTBAR-KIND", "Item kind cannot go on hotbar",
                    "Use weapon/consumable/utility items."));
        }
        InventoryStack& dest = hotbar_[static_cast<std::size_t>(slot)];
        if (!dest.empty() && dest.item_id != from->item_id) {
            InventoryStack swap = dest;
            dest = *from;
            *from = swap;
        } else if (!dest.empty() && dest.item_id == from->item_id) {
            const int max_stack = max_stack_for(*def.value());
            const int space = max_stack - dest.count;
            const int add = std::min(space, from->count);
            dest.count += add;
            from->count -= add;
            if (from->count <= 0) *from = {};
        } else {
            dest = *from;
            *from = {};
        }
        return Result<void>::success();
    }

    if (to_region == "equip") {
        std::string slot = to_equip_slot;
        if (slot.empty()) {
            if (def.value()->kind == "trinket") {
                const int t = first_empty_trinket();
                if (t < 0) {
                    return Result<void>::failure(
                        inv_error("INV-TRINKET-FULL", "No free trinket slot", "Unequip a trinket first."));
                }
                slot = "trinket" + std::to_string(t);
            } else if (def.value()->kind == "armor") {
                return Result<void>::failure(
                    inv_error("INV-EQUIP-SLOT", "Armor equip requires head/chest/legs",
                        "Pass to_equip_slot."));
            } else {
                return Result<void>::failure(
                    inv_error("INV-EQUIP-KIND", "Item cannot be equipped on the strip",
                        "Use armor or trinket items."));
            }
        }
        if (!kind_fits_equip(*def.value(), slot)) {
            return Result<void>::failure(
                inv_error("INV-EQUIP-KIND", "Item kind does not fit equip slot",
                    "Armor → head/chest/legs; trinkets → trinket0..3."));
        }
        InventoryStack* dest = stack_at("equip", -1, slot);
        if (!dest) {
            return Result<void>::failure(
                inv_error("INV-EQUIP-SLOT", "Unknown equip slot", "Use head/chest/legs/trinketN."));
        }
        InventoryStack swap = *dest;
        *dest = InventoryStack{from->item_id, 1};
        if (from->count > 1) {
            from->count -= 1;
            if (!swap.empty()) {
                if (const auto back = add_to_bag_or_ammo(*require_def(swap.item_id).value(), swap.count); !back)
                    return back;
            }
        } else {
            *from = swap;
        }
        return Result<void>::success();
    }

    if (to_region == "bag") {
        if (is_ammo(*def.value())) {
            const auto added = add_to_bag_or_ammo(*def.value(), from->count);
            if (!added) return added;
            *from = {};
            return Result<void>::success();
        }
        int slot = to_index;
        if (slot < 0) slot = first_empty_bag();
        if (slot < 0) {
            return Result<void>::failure(
                inv_error("INV-BAG-FULL", "Bag is full", "Free a bag slot."));
        }
        InventoryStack& dest = bag_[static_cast<std::size_t>(slot)];
        if (!dest.empty() && dest.item_id != from->item_id) {
            InventoryStack swap = dest;
            dest = *from;
            *from = swap;
        } else if (!dest.empty() && dest.item_id == from->item_id) {
            const int max_stack = max_stack_for(*def.value());
            const int space = max_stack - dest.count;
            const int add = std::min(space, from->count);
            dest.count += add;
            from->count -= add;
            if (from->count <= 0) *from = {};
        } else {
            dest = *from;
            *from = {};
        }
        return Result<void>::success();
    }

    if (to_region == "container") {
        auto* slots = open_container_slots();
        if (!slots) {
            return Result<void>::failure(
                inv_error("INV-CONTAINER-CLOSED", "No container is open",
                    "Open a crate or chest before moving items into it."));
        }
        int slot = to_index;
        if (slot < 0) slot = first_empty_container();
        if (slot < 0) {
            return Result<void>::failure(
                inv_error("INV-CONTAINER-FULL", "Container is full", "Take an item out first."));
        }
        InventoryStack& dest = (*slots)[static_cast<std::size_t>(slot)];
        if (!dest.empty() && dest.item_id != from->item_id) {
            InventoryStack swap = dest;
            dest = *from;
            *from = swap;
        } else if (!dest.empty() && dest.item_id == from->item_id) {
            const int max_stack = max_stack_for(*def.value());
            const int space = max_stack - dest.count;
            const int add = std::min(space, from->count);
            dest.count += add;
            from->count -= add;
            if (from->count <= 0) *from = {};
        } else {
            dest = *from;
            *from = {};
        }
        return Result<void>::success();
    }

    return Result<void>::failure(
        inv_error("INV-MOVE-TO", "Unsupported destination region: " + to_region,
            "Use bag, hotbar, equip, or container."));
}

Result<void> InventoryRuntime::open_container(const std::string& container_id) {
    if (const auto bound = require_bound(); !bound) return bound;
    if (container_id.empty()) {
        close_container();
        return Result<void>::success();
    }
    auto& slots = containers_[container_id];
    if (slots.size() != static_cast<std::size_t>(kInventoryContainerSlots)) {
        slots.assign(static_cast<std::size_t>(kInventoryContainerSlots), InventoryStack{});
    }
    open_container_id_ = container_id;
    return Result<void>::success();
}

void InventoryRuntime::close_container() noexcept {
    open_container_id_.clear();
}

Result<void> InventoryRuntime::grant_container(const std::string& item_id, int count) {
    const auto def = require_def(item_id);
    if (!def) return Result<void>::failure(def.error());
    auto* slots = open_container_slots();
    if (!slots) {
        return Result<void>::failure(
            inv_error("INV-CONTAINER-CLOSED", "No container is open",
                "Call open_container before grant_container."));
    }
    if (count <= 0) {
        return Result<void>::failure(
            inv_error("INV-COUNT", "Grant count must be positive", "Pass count >= 1."));
    }
    int remaining = count;
    const int max_stack = max_stack_for(*def.value());
    for (auto& stack : *slots) {
        if (!stack.empty() && stack.item_id == def.value()->id && stack.count < max_stack) {
            const int space = max_stack - stack.count;
            const int add = std::min(space, remaining);
            stack.count += add;
            remaining -= add;
            if (remaining <= 0) return Result<void>::success();
        }
    }
    while (remaining > 0) {
        const int slot = first_empty_container();
        if (slot < 0) {
            return Result<void>::failure(
                inv_error("INV-CONTAINER-FULL", "Container is full", "Take an item out first."));
        }
        const int add = std::min(max_stack, remaining);
        (*slots)[static_cast<std::size_t>(slot)] = InventoryStack{def.value()->id, add};
        remaining -= add;
    }
    return Result<void>::success();
}

Result<void> InventoryRuntime::select_hotbar(int slot) {
    if (const auto bound = require_bound(); !bound) return bound;
    if (slot < 0 || slot >= kInventoryHotbarSlots) {
        return Result<void>::failure(
            inv_error("INV-HOTBAR-SLOT", "Hotbar slot out of range", "Use slots 0..7."));
    }
    selected_hotbar_ = slot;
    ui_selection_.region = "hotbar";
    ui_selection_.index = slot;
    ui_selection_.equip_slot.clear();
    return Result<void>::success();
}

Result<void> InventoryRuntime::select_ui(const std::string& region, int index, const std::string& equip_slot) {
    if (const auto bound = require_bound(); !bound) return bound;
    if (region == "none" || region.empty()) {
        ui_selection_ = {};
        return Result<void>::success();
    }
    if (region == "hotbar") return select_hotbar(index);
    const InventoryStack* stack = stack_at(region, index, equip_slot);
    if (!stack) {
        return Result<void>::failure(
            inv_error("INV-SELECT", "Invalid selection target", "Pass a valid region/index/equip slot."));
    }
    ui_selection_.region = region;
    ui_selection_.index = index;
    ui_selection_.equip_slot = equip_slot;
    return Result<void>::success();
}

Result<void> InventoryRuntime::equip_selected() {
    if (const auto bound = require_bound(); !bound) return bound;
    if (ui_selection_.region != "bag" && ui_selection_.region != "hotbar" &&
        ui_selection_.region != "container") {
        return Result<void>::failure(
            inv_error("INV-EQUIP-SEL", "Select a bag, hotbar, or crate item first", "Click an item slot."));
    }
    const InventoryStack* stack = stack_at(ui_selection_.region, ui_selection_.index, {});
    if (!stack || stack->empty()) {
        return Result<void>::failure(
            inv_error("INV-EQUIP-SEL", "Selected slot is empty", "Select a filled item."));
    }
    const auto def = require_def(stack->item_id);
    if (!def) return Result<void>::failure(def.error());

    if (def.value()->kind == "weapon" || def.value()->kind == "consumable" || has_tag(*def.value(), "utility")) {
        return move_to(ui_selection_.region, ui_selection_.index, {}, "hotbar", -1, {});
    }
    if (def.value()->kind == "trinket") {
        return move_to(ui_selection_.region, ui_selection_.index, {}, "equip", -1, {});
    }
    if (def.value()->kind == "armor") {
        return Result<void>::failure(
            inv_error("INV-EQUIP-ARMOR", "Armor requires a named slot",
                "Select head/chest/legs destination explicitly."));
    }
    return move_to(ui_selection_.region, ui_selection_.index, {}, "hotbar", -1, {});
}

Result<void> InventoryRuntime::unequip_selected() {
    if (const auto bound = require_bound(); !bound) return bound;
    if (ui_selection_.region == "hotbar") {
        return move_to("hotbar", ui_selection_.index, {}, "bag", -1, {});
    }
    if (ui_selection_.region == "equip") {
        return move_to("equip", -1, ui_selection_.equip_slot, "bag", -1, {});
    }
    return Result<void>::failure(
        inv_error("INV-UNEQUIP", "Select a hotbar or equip slot to unequip", "Click an equipped item."));
}

InventoryStack InventoryRuntime::active_hotbar_item() const {
    if (selected_hotbar_ < 0 || selected_hotbar_ >= kInventoryHotbarSlots) return {};
    return hotbar_[static_cast<std::size_t>(selected_hotbar_)];
}

float mitigate_damage_with_armor(float incoming, float armor) noexcept {
    return resist_mitigation(incoming, armor);
}

void apply_player_stats_to_hud(HudRuntime& hud, const PlayerStatTotals& totals) {
    const double max_health = totals.max_health > 1.0f ? static_cast<double>(totals.max_health) : 1.0;
    const double max_stamina = totals.max_stamina > 1.0f ? static_cast<double>(totals.max_stamina) : 1.0;
    const double health = hud.get_number("player.health").value_or(max_health);
    hud.set_health(health, max_health);
    const double stamina = hud.get_number("player.resource").value_or(max_stamina);
    hud.set_resource(stamina, max_stamina);
    hud.set_number("player.armor", static_cast<double>(totals.armor));
    hud.set_number("player.strength", static_cast<double>(totals.strength));
    hud.set_number("player.agility", static_cast<double>(totals.agility));
    hud.set_number("player.intellect", static_cast<double>(totals.intellect));
    hud.set_number("player.weaponDps", static_cast<double>(totals.dps));
    hud.set_number("player.damageMin", static_cast<double>(totals.damage_min));
    hud.set_number("player.damageMax", static_cast<double>(totals.damage_max));
    hud.set_number("player.critChance", static_cast<double>(totals.crit_chance));
    hud.set_bool("player.holdingMagicWeapon", totals.holding_magic_weapon);
    hud.set_visible("player_magicka", false);
    hud.set_visible("player_magicka_text", false);
    hud.set_visible("player_magicka_label", false);
    hud.set_visible("player_magicka_frame", false);
    if (totals.holding_magic_weapon) {
        if (hud.rune_charges_max() <= 0)
            hud.set_rune_charges(kAct0RuneChargesMax, kAct0RuneChargesMax);
    } else {
        hud.set_rune_charges(0, 0);
    }
}

PlayerStatTotals InventoryRuntime::compute_player_stats() const {
    PlayerStatTotals totals;
    if (!catalog_) return totals;

    auto add_modifiers = [&](const InventoryStack& stack) {
        if (stack.empty()) return;
        const ItemDef* def = catalog_->find(stack.item_id);
        if (!def) return;
        for (const auto& mod : def->stats.modifiers) {
            if (mod.id == "maxHealth") {
                totals.max_health += mod.value;
            } else if (mod.id == "maxStamina" || mod.id == "stamina") {
                totals.max_stamina += mod.value;
            } else if (mod.id == "maxMagicka" || mod.id == "magicka") {
                totals.max_magicka += mod.value;
            } else if (mod.id == "armor") {
                totals.armor += mod.value;
            } else if (mod.id == "strength") {
                totals.strength += mod.value;
            } else if (mod.id == "agility") {
                totals.agility += mod.value;
            } else if (mod.id == "intellect" || mod.id == "intelligence") {
                totals.intellect += mod.value;
            } else if (mod.id == "magicResist" || mod.id == "resistMagic") {
                totals.magic_resist += mod.value;
            } else if (mod.id == "poisonResist" || mod.id == "resistPoison") {
                totals.poison_resist += mod.value;
            } else if (mod.id == "blightResist" || mod.id == "resistBlight") {
                totals.blight_resist += mod.value;
            } else if (mod.id == "holyResist" || mod.id == "resistHoly") {
                totals.holy_resist += mod.value;
            } else if (mod.id == "shadowResist" || mod.id == "resistShadow") {
                totals.shadow_resist += mod.value;
            } else if (mod.id == "critChance" || mod.id == "crit") {
                totals.crit_chance += mod.value;
            }
        }
    };

    add_modifiers(equipped_.head);
    add_modifiers(equipped_.chest);
    add_modifiers(equipped_.legs);
    for (const auto& trinket : equipped_.trinkets) add_modifiers(trinket);

    const InventoryStack held = active_hotbar_item();
    add_modifiers(held);
    totals.max_health += kHealthPerStrength * totals.strength;
    totals.crit_chance += kCritChancePerAgility * totals.agility;
    if (!held.empty()) {
        if (const ItemDef* weapon = catalog_->find(held.item_id)) {
            totals.weapon_school = weapon_school_from_tags(weapon->tags);
            totals.holding_magic_weapon = totals.weapon_school == WeaponSchool::Magic;
            if (weapon->stats.has_weapon_damage()) {
                float attribute = 0.0f;
                if (totals.weapon_school == WeaponSchool::Melee) attribute = totals.strength;
                else if (totals.weapon_school == WeaponSchool::Ranged) attribute = totals.agility;
                else if (totals.weapon_school == WeaponSchool::Magic) attribute = totals.intellect;
                totals.damage_multiplier = attribute_damage_multiplier(attribute);
                totals.damage_min = weapon->stats.damage_min * totals.damage_multiplier;
                totals.damage_max = weapon->stats.damage_max * totals.damage_multiplier;
                totals.attacks_per_second = weapon->stats.attacks_per_second;
                totals.dps = weapon->stats.dps() * totals.damage_multiplier;
            }
        }
    }
    const auto mix = crit_marbles_from_chance(totals.crit_chance);
    totals.crit_marbles = mix.crit;
    totals.hit_marbles = mix.hit;
    totals.crit_chance = mix.displayed_chance();
    return totals;
}

bool InventoryRuntime::draw_crit_marble() {
    const auto totals = compute_player_stats();
    if (totals.crit_marbles <= 0) return false;
    if (crit_remaining_.empty() || crit_bag_crit_ != totals.crit_marbles ||
        crit_bag_hit_ != totals.hit_marbles) {
        crit_bag_crit_ = totals.crit_marbles;
        crit_bag_hit_ = totals.hit_marbles;
        refill_marbles(crit_remaining_, crit_bag_crit_, crit_bag_hit_, crit_bag_seed_);
    }
    if (crit_remaining_.empty()) return false;
    const bool crit = crit_remaining_.back() != 0;
    crit_remaining_.pop_back();
    if (crit_remaining_.empty()) {
        refill_marbles(crit_remaining_, crit_bag_crit_, crit_bag_hit_, crit_bag_seed_);
    }
    return crit;
}

int InventoryRuntime::draw_damage_marble(int lo, int hi) {
    if (lo > hi) std::swap(lo, hi);
    if (damage_remaining_.empty() || damage_bag_lo_ != lo || damage_bag_hi_ != hi) {
        damage_bag_lo_ = lo;
        damage_bag_hi_ = hi;
        refill_damage_marbles(damage_remaining_, lo, hi, damage_bag_seed_);
    }
    if (damage_remaining_.empty()) return lo;
    const int value = damage_remaining_.back();
    damage_remaining_.pop_back();
    if (damage_remaining_.empty()) {
        refill_damage_marbles(damage_remaining_, damage_bag_lo_, damage_bag_hi_, damage_bag_seed_);
    }
    return value;
}

PlayerAttackRoll InventoryRuntime::roll_player_attack(int combo_step) {
    PlayerAttackRoll roll;
    roll.combo_step = combo_step;
    const auto totals = compute_player_stats();
    roll.school = totals.weapon_school;
    if (!(totals.damage_min > 0.0f || totals.damage_max > 0.0f)) return roll;
    const float lo_f = totals.damage_min > 0.0f ? totals.damage_min : totals.damage_max;
    const float hi_f = totals.damage_max > 0.0f ? totals.damage_max : totals.damage_min;
    int lo = static_cast<int>(std::lround(std::min(lo_f, hi_f)));
    int hi = static_cast<int>(std::lround(std::max(lo_f, hi_f)));
    if (lo < 1) lo = 1;
    if (hi < lo) hi = lo;
    const int mid = lo + (hi - lo) / 2;
    if (combo_step == 1) {
        roll.damage = static_cast<float>(lo);
    } else if (combo_step == 2) {
        roll.damage = static_cast<float>(mid);
    } else if (combo_step == 3) {
        roll.damage = static_cast<float>(hi);
    } else {
        roll.damage = static_cast<float>(draw_damage_marble(lo, hi));
    }
    roll.crit = draw_crit_marble();
    if (roll.crit) roll.damage *= kCritHitMultiplier;
    return roll;
}

InventoryStatusSnapshot InventoryRuntime::status() const {
    InventoryStatusSnapshot snap;
    snap.bag_capacity = bag_capacity_;
    snap.bag = bag_;
    snap.hotbar.assign(hotbar_.begin(), hotbar_.end());
    snap.equipped = equipped_;
    snap.ammo = ammo_;
    snap.quest_inventory = quest_inventory_;
    if (const auto* slots = open_container_slots()) {
        snap.container = *slots;
        snap.container_id = open_container_id_;
        snap.container_capacity = static_cast<int>(slots->size());
    }
    snap.selected_hotbar = selected_hotbar_;
    snap.ui_selection = ui_selection_;
    snap.gold = gold_;
    snap.player_stats = compute_player_stats();
    return snap;
}

} // namespace engine
