#pragma once

#include "engine/assets/item_catalog_asset.h"
#include "engine/core/result.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace engine {

constexpr int kInventoryBagCapacity = 20;
constexpr int kInventoryHotbarSlots = 8;
constexpr int kInventoryTrinketSlots = 4;
constexpr int kInventoryMaxStack = 99;
constexpr int kInventoryAmmoMaxStack = 1000;

struct InventoryStack {
    std::string item_id;
    int count = 0;
    [[nodiscard]] bool empty() const noexcept { return item_id.empty() || count <= 0; }
};

struct InventoryEquipped {
    InventoryStack head;
    InventoryStack chest;
    InventoryStack legs;
    std::array<InventoryStack, kInventoryTrinketSlots> trinkets{};
};

struct InventorySelection {
    std::string region; // bag | hotbar | equip | ammo | none
    int index = -1;     // bag/hotbar/ammo index, or equip ordinal
    std::string equip_slot; // head|chest|legs|trinket0..3 when region=equip
};

struct InventoryStatusSnapshot {
    int bag_capacity = kInventoryBagCapacity;
    std::vector<InventoryStack> bag;
    std::vector<InventoryStack> hotbar;
    InventoryEquipped equipped;
    std::vector<InventoryStack> ammo;
    std::vector<InventoryStack> quest_inventory;
    int selected_hotbar = 0;
    InventorySelection ui_selection{};
    int gold = 0;
};

/// Thin session inventory (TICKET-0237 / DEC-0050). No hard class locks.
class InventoryRuntime {
public:
    [[nodiscard]] Result<void> bind(const ItemCatalogAsset* catalog);
    void reset() noexcept;

    [[nodiscard]] Result<void> grant(const std::string& item_id, int count = 1);
    [[nodiscard]] Result<void> remove(const std::string& item_id, int count = 1);
    [[nodiscard]] Result<void> set_hotbar(int slot, const std::string& item_id, int count = 1);
    [[nodiscard]] Result<void> clear_hotbar(int slot);
    [[nodiscard]] Result<void> set_equip(const std::string& equip_slot, const std::string& item_id, int count = 1);
    [[nodiscard]] Result<void> clear_equip(const std::string& equip_slot);
    /// Move one stack between bag / hotbar / equip. `to_index` ignored for named equip slots.
    [[nodiscard]] Result<void> move_to(const std::string& from_region, int from_index,
        const std::string& from_equip_slot, const std::string& to_region, int to_index,
        const std::string& to_equip_slot);
    [[nodiscard]] Result<void> select_hotbar(int slot);
    [[nodiscard]] Result<void> select_ui(const std::string& region, int index = -1,
        const std::string& equip_slot = {});
    /// Equip/put currently UI-selected bag item onto hotbar or equip strip (smart default by kind).
    [[nodiscard]] Result<void> equip_selected();
    [[nodiscard]] Result<void> unequip_selected();

    void set_gold(int gold) noexcept { gold_ = gold < 0 ? 0 : gold; }
    [[nodiscard]] int gold() const noexcept { return gold_; }
    [[nodiscard]] int selected_hotbar() const noexcept { return selected_hotbar_; }
    [[nodiscard]] InventoryStack active_hotbar_item() const;
    [[nodiscard]] InventoryStatusSnapshot status() const;
    [[nodiscard]] const ItemCatalogAsset* catalog() const noexcept { return catalog_; }
    [[nodiscard]] bool is_bound() const noexcept { return catalog_ != nullptr; }
    [[nodiscard]] const ItemDef* find_def(const std::string& item_id) const;

private:
    [[nodiscard]] Result<void> require_bound() const;
    [[nodiscard]] Result<const ItemDef*> require_def(const std::string& item_id) const;
    [[nodiscard]] InventoryStack* stack_at(const std::string& region, int index, const std::string& equip_slot);
    [[nodiscard]] const InventoryStack* stack_at(const std::string& region, int index,
        const std::string& equip_slot) const;
    [[nodiscard]] int max_stack_for(const ItemDef& def) const;
    [[nodiscard]] bool kind_fits_equip(const ItemDef& def, const std::string& equip_slot) const;
    [[nodiscard]] bool kind_fits_hotbar(const ItemDef& def) const;
    [[nodiscard]] int first_empty_bag() const;
    [[nodiscard]] int first_empty_hotbar() const;
    [[nodiscard]] int first_empty_trinket() const;
    [[nodiscard]] Result<void> add_to_bag_or_ammo(const ItemDef& def, int count);

    const ItemCatalogAsset* catalog_ = nullptr;
    int bag_capacity_ = kInventoryBagCapacity;
    std::vector<InventoryStack> bag_;
    std::array<InventoryStack, kInventoryHotbarSlots> hotbar_{};
    InventoryEquipped equipped_{};
    std::vector<InventoryStack> ammo_;
    std::vector<InventoryStack> quest_inventory_;
    int selected_hotbar_ = 0;
    InventorySelection ui_selection_{};
    int gold_ = 0;
};

} // namespace engine
