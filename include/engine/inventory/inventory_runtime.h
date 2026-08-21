#pragma once

#include "engine/assets/item_catalog_asset.h"
#include "engine/core/result.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine {

class HudRuntime;

constexpr int kInventoryBagCapacity = 20;
constexpr int kInventoryHotbarSlots = 8;
constexpr int kInventoryTrinketSlots = 4;
constexpr int kInventoryContainerSlots = 8;
constexpr int kInventoryMaxStack = 99;
constexpr int kInventoryAmmoMaxStack = 1000;

/// Act 0 unarmored baseline. Gear modifiers add on top (DEC-0050).
inline constexpr float kPlayerBaseMaxHealth = 100.0f;
inline constexpr float kPlayerBaseMaxStamina = 100.0f;
inline constexpr float kPlayerBaseMaxMagicka = 100.0f;
/// Strength also grants a little max health (melee attacker fantasy).
inline constexpr float kHealthPerStrength = 2.0f;
/// Strength / agility / intellect: +2% school damage per point.
inline constexpr float kDamagePerAttribute = 0.02f;
inline constexpr float kMagickaPerIntellect = 2.0f;
/// Agility also adds this many crit-chance percent points (marble bag).
inline constexpr float kCritChancePerAgility = 2.0f;
/// Crit damage multiplier when a crit marble is drawn.
inline constexpr float kCritHitMultiplier = 1.5f;
/// Crit chance (percent) converts to this many marbles (DEC-0057).
inline constexpr int kCritBagSize = 20;

enum class WeaponSchool { None, Melee, Ranged, Magic };

struct CritMarbleMix {
    int crit = 0;
    int hit = 0;
    [[nodiscard]] int total() const noexcept { return crit + hit; }
    [[nodiscard]] float displayed_chance() const noexcept {
        const int n = total();
        if (n <= 0) return 0.0f;
        return 100.0f * static_cast<float>(crit) / static_cast<float>(n);
    }
};

struct PlayerStatTotals {
    float max_health = kPlayerBaseMaxHealth;
    float max_stamina = kPlayerBaseMaxStamina;
    float max_magicka = 0.0f;
    float armor = 0.0f;
    float strength = 0.0f;
    float agility = 0.0f;
    float intellect = 0.0f;
    float magic_resist = 0.0f;
    float poison_resist = 0.0f;
    float blight_resist = 0.0f;
    float holy_resist = 0.0f;
    float shadow_resist = 0.0f;
    float crit_chance = 0.0f;
    int crit_marbles = 0;
    int hit_marbles = 0;
    float damage_min = 0.0f;
    float damage_max = 0.0f;
    float attacks_per_second = 0.0f;
    float dps = 0.0f;
    float damage_multiplier = 1.0f;
    bool holding_magic_weapon = false;
    WeaponSchool weapon_school = WeaponSchool::None;
};

struct PlayerAttackRoll {
    float damage = 0.0f;
    bool crit = false;
    WeaponSchool school = WeaponSchool::None;
    int combo_step = 0;
};

[[nodiscard]] WeaponSchool weapon_school_from_tags(const std::vector<std::string>& tags) noexcept;
[[nodiscard]] const char* weapon_school_id(WeaponSchool school) noexcept;
[[nodiscard]] float attribute_damage_multiplier(float attribute) noexcept;
[[nodiscard]] CritMarbleMix crit_marbles_from_chance(float percent) noexcept;
/// Shared resist curve: `incoming * 100 / (100 + resist)`. Armor is physical resist.
[[nodiscard]] float resist_mitigation(float incoming, float resist) noexcept;
/// Armor reduces incoming physical damage: `incoming * 100 / (100 + armor)`. No floor.
[[nodiscard]] float mitigate_damage_with_armor(float incoming, float armor) noexcept;

/// Keep current HP/stamina/magicka; raise caps from gear. Clamps current when max drops.
void apply_player_stats_to_hud(HudRuntime& hud, const PlayerStatTotals& totals);

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
    std::string region; // bag | hotbar | equip | ammo | container | none
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
    std::vector<InventoryStack> container;
    std::string container_id;
    int container_capacity = kInventoryContainerSlots;
    int selected_hotbar = 0;
    InventorySelection ui_selection{};
    int gold = 0;
    PlayerStatTotals player_stats{};
};

/// Thin session inventory (TICKET-0237 / DEC-0050). No hard class locks.
class InventoryRuntime {
public:
    [[nodiscard]] Result<void> bind(const ItemCatalogAsset* catalog);
    void reset() noexcept;

    [[nodiscard]] Result<void> grant(const std::string& item_id, int count = 1);
    [[nodiscard]] Result<void> remove(const std::string& item_id, int count = 1);
    /// Total count across bag + ammo + hotbar stacks for `item_id` (0 if unbound/missing).
    [[nodiscard]] int count_item(const std::string& item_id) const;
    [[nodiscard]] Result<void> set_hotbar(int slot, const std::string& item_id, int count = 1);
    [[nodiscard]] Result<void> clear_hotbar(int slot);
    [[nodiscard]] Result<void> set_equip(const std::string& equip_slot, const std::string& item_id, int count = 1);
    [[nodiscard]] Result<void> clear_equip(const std::string& equip_slot);
    /// Move one stack between bag / hotbar / equip / container. `to_index` ignored for named equip slots.
    [[nodiscard]] Result<void> move_to(const std::string& from_region, int from_index,
        const std::string& from_equip_slot, const std::string& to_region, int to_index,
        const std::string& to_equip_slot);
    /// Open a world container (crate/chest) for drag-drop. Empty id closes.
    [[nodiscard]] Result<void> open_container(const std::string& container_id);
    void close_container() noexcept;
    [[nodiscard]] bool container_open() const noexcept { return !open_container_id_.empty(); }
    [[nodiscard]] const std::string& open_container_id() const noexcept { return open_container_id_; }
    /// Grant into the currently open container (first free / stackable slot).
    [[nodiscard]] Result<void> grant_container(const std::string& item_id, int count = 1);
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
    /// Sums `stats.modifiers` from equipped armor/trinkets plus the active hotbar weapon.
    [[nodiscard]] PlayerStatTotals compute_player_stats() const;
    /// Draw one crit marble (refills the bag when empty). False when chance is 0.
    [[nodiscard]] bool draw_crit_marble();
    /// Draw one integer damage marble from [lo, hi] (DEC-0057 bag; refills when empty).
    [[nodiscard]] int draw_damage_marble(int lo, int hi);
    /// Held-weapon damage: combo_step 1/2/3 → min/mid/max; 0 → damage marble bag. Then crit marble.
    [[nodiscard]] PlayerAttackRoll roll_player_attack(int combo_step = 0);
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
    [[nodiscard]] int first_empty_container() const;
    [[nodiscard]] Result<void> add_to_bag_or_ammo(const ItemDef& def, int count);
    [[nodiscard]] std::vector<InventoryStack>* open_container_slots();
    [[nodiscard]] const std::vector<InventoryStack>* open_container_slots() const;

    const ItemCatalogAsset* catalog_ = nullptr;
    int bag_capacity_ = kInventoryBagCapacity;
    std::vector<InventoryStack> bag_;
    std::array<InventoryStack, kInventoryHotbarSlots> hotbar_{};
    InventoryEquipped equipped_{};
    std::vector<InventoryStack> ammo_;
    std::vector<InventoryStack> quest_inventory_;
    std::unordered_map<std::string, std::vector<InventoryStack>> containers_;
    std::string open_container_id_;
    int selected_hotbar_ = 0;
    InventorySelection ui_selection_{};
    int gold_ = 0;
    std::vector<std::uint8_t> crit_remaining_{};
    int crit_bag_crit_ = 0;
    int crit_bag_hit_ = 0;
    std::uint32_t crit_bag_seed_ = 1;
    std::vector<int> damage_remaining_{};
    int damage_bag_lo_ = 0;
    int damage_bag_hi_ = 0;
    std::uint32_t damage_bag_seed_ = 1;
};

} // namespace engine
