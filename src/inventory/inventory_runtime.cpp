#include "engine/inventory/inventory_runtime.h"

#include "engine/core/error.h"

#include <algorithm>

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

} // namespace

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
    selected_hotbar_ = 0;
    ui_selection_ = {};
    gold_ = 0;
    return Result<void>::success();
}

void InventoryRuntime::reset() noexcept {
    catalog_ = nullptr;
    bag_.clear();
    hotbar_ = {};
    equipped_ = {};
    ammo_.clear();
    quest_inventory_.clear();
    selected_hotbar_ = 0;
    ui_selection_ = {};
    gold_ = 0;
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

    return Result<void>::failure(
        inv_error("INV-MOVE-TO", "Unsupported destination region: " + to_region,
            "Use bag, hotbar, or equip."));
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
    if (ui_selection_.region != "bag" && ui_selection_.region != "hotbar") {
        return Result<void>::failure(
            inv_error("INV-EQUIP-SEL", "Select a bag or hotbar item first", "Click an item slot."));
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

InventoryStatusSnapshot InventoryRuntime::status() const {
    InventoryStatusSnapshot snap;
    snap.bag_capacity = bag_capacity_;
    snap.bag = bag_;
    snap.hotbar.assign(hotbar_.begin(), hotbar_.end());
    snap.equipped = equipped_;
    snap.ammo = ammo_;
    snap.quest_inventory = quest_inventory_;
    snap.selected_hotbar = selected_hotbar_;
    snap.ui_selection = ui_selection_;
    snap.gold = gold_;
    return snap;
}

} // namespace engine
