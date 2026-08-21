# Terraria-shaped gearing system

- Status: planned (design locked)
- Decisions: [DEC-0048](../decisions/index.md#dec-0048-terraria-shaped-gearing-with-soft-archetype-affinity), [DEC-0050](../decisions/index.md#dec-0050-inventory-ux-item-kinds-and-positive-soft-affinity), [DEC-0051](../decisions/index.md#dec-0051-no-xp-power-progression-and-quest-ux), [DEC-0057](../decisions/index.md#dec-0057-marble-bag-rng-for-gameplay-rolls)
- Epic: [EPIC-0018](../planning/epics.md) (TICKET-0231+)
- Related: [DEC-0009](../decisions/index.md#dec-0009-starting-archetype-character-creation), [DEC-0044](../decisions/index.md#dec-0044-starting-archetype-lane-orgs-and-rename), [DEC-0032](../decisions/index.md#dec-0032-open-world-travel-discovery-map-and-dual-soft-gates), TICKET-0111, EPIC-0011 melee slice
- Provenance: Dom + John design recordings / chat 2026-07-27 (transcript “Ledgerport” → canon **Ledgeport**; “Thraador” → **Thrator**); inventory UX lock 2026-07-29 ([`../design/recording_item_system_2026-07-29.md`](../design/recording_item_system_2026-07-29.md))

## Product intent

Combat stays **simple** (Souls-lite action + three weapon chains). Gameplay depth comes from a large pool of **items and trinkets with unique effects**, oriented around the three starting archetypes but **not class-locked**.

## Soft archetype affinity (positive)

| Rule | Detail |
| --- | --- |
| Equip / use | Any archetype can use any weapon / armor / trinket |
| Use | Weapon abilities and item on-use effects are available to whoever holds them |
| Benefit | Matching lane (**Ashfell Blade** / **Outrider** / **Runecaster**) gains a **bonus** via stat allocation / lane multipliers |
| Baseline | Off-lane stays at **1×** — do **not** nerf below baseline for experimenting |
| Co-op | Players may gift/trade items; off-lane use is intentional comedy/power fantasy |

Do **not** implement hard “cannot equip” gates for lane mismatch.

## Inventory UX ([DEC-0050](../decisions/index.md#dec-0050-inventory-ux-item-kinds-and-positive-soft-affinity))

| Surface | Rule |
| --- | --- |
| Hotbar | **8 slots** — weapons + utility tools; select slot to use (Terraria-shaped) |
| Equip strip | Named armor: **`head`**, **`chest`**, **`legs`** + **4 trinket** slots (`trinket0`…`trinket3`). Stats from armor + trinkets + weapons. Shields may use a trinket slot |
| Class abilities | Separate UI (~3–4 active); not hotbar entries |
| Bag | Base **20 slots** (not weight); **one entry per slot**; craft/loot **bag upgrades** increase capacity |
| Stacks | Resources etc. up to **99** per bag stack; dedicated **ammo slots** may stack ~**1000** |
| Quest items | Separate **quest inventory** (not bag slots) |
| Camp chest | **Per-player**, persists for the save; trade/gift between players |
| Currencies | Non-slot counters (icon + amount); co-op **shared gold** default stays in save `sharedCampaign.economy` |
| Durability | **None** in v1 (no repair sink) |
| Effects | Data-driven params **and** Lua hooks |
| Classification | Primary **kind tag** (`weapon` / `armor` / `trinket` / `consumable` / `material`) + optional **labels** (`healing`, `utility`, …) |

## Combat baseline

- Three chains: melee, ranged, magic (maps to the three lanes).
- Prefer shared swing/shot/cast animations; flavor from item VFX, procs, passives, and on-use.
- Ability **caveats** OK (stamina/mana, cooldown, stance, situational) without BDO-style animation complexity or tab-target rotation design.

## Act tiers and rares

- Commons/uncommons track **act power bands**.
- **Obscure rares** may exceed their act band and remain useful into later acts.
- First-time playthrough should rarely stumble into the wildest rares without deep exploration or external knowledge; story completion must not require them.
- Bosses support **common + rare** tables and optional farm replay when systems allow.
- Table rolls use **marble bags** ([DEC-0057](../decisions/index.md#dec-0057-marble-bag-rng-for-gameplay-rolls), [`marble-bag-rng.md`](marble-bag-rng.md)): integer counts, draw without replacement, refill when empty. Independent `math.random` weights (current Act 0 pouch/chest Lua) are not the shipping contract.

## Acquisition loops

1. World chests / exploration finds
2. Vendors — including **Ledgeport** undermarket / black-market flavor (post–Act 0 camp travel)
3. Mining ores and crystals → materials → crafting (full loop after inventory; Act 0 may stub materials)
4. Named bosses / champions (later acts for easter-egg warlords)

### Act 0 Landfall loot slice (MVP)

Act 0 is **not** a full item pool, but players **must** be able to pick up / receive **some** items during Landfall so gearing fantasy starts in the vertical slice.

| Source | Intent |
| --- | --- |
| Starter kit | Per-archetype default weapon |
| Siege approach finds | World containers / fallen-soldier pouches on the Calrenoth approach |
| Combat drops | Low-rate commons from Imperium footsoldiers (bandage / potion / ammo / scrap)—not required for waves |
| Story beat reward | One granted item from Arkand rescue, Grenge, or camp handoff |
| Obscure nook | **Vein-Iron Pendant** ships in Act 0; never story-gated |

**Target count:** roughly **8–12** distinct Act 0 item defs (including starters). Effects can be light (stat bump, small heal, flavor passive)—unique VFX optional.

#### Act 0 item ids (concrete)

Ids from display names. Concepts: [`../art/concepts/README.md`](../art/concepts/README.md).

| Id | Display name | Kind | Source |
| --- | --- | --- | --- |
| `ashfell_arming_sword` | Ashfell Arming Sword | weapon | Starter — Ashfell Blade |
| `outrider_shortbow` | Outrider Shortbow | weapon | Starter — Outrider |
| `guild_rune_focus` | Guild Rune Focus | weapon | Starter — Runecaster (arcane bolt) |
| `guild_rune_focus_fire` | Guild Fire Focus | weapon | Sandbox clone — fire bolt + burn DoT |
| `guild_rune_focus_frost` | Guild Frost Focus | weapon | Sandbox clone — frost bolt + Slow |
| `guild_rune_focus_lightning` | Guild Lightning Focus | weapon | Sandbox clone — lightning bolt + chain hops |
| `field_bandage` | Field Bandage | consumable (`healing`) | Common / combat drop |
| `soldiers_scrap_pouch` | Soldier's Scrap Pouch | consumable / material flavor | Approach find |
| `vein_iron_pendant` | Vein-Iron Pendant | trinket | Obscure Act 0 rare (**ship**) |
| `siege_tonic` | Siege Tonic | consumable (`healing`) | Common potion drop / pouch |
| `crude_arrow` | Crude Arrow | material (ammo) | Combat ammo drop; stacks in ammo slot |
| `arkands_favor` | Arkand's Favor | trinket | Story grant (Arkand rescue / camp handoff) |
| `muddied_keep_ring` | Muddied Keep Ring | trinket | Uncommon approach / side nook |
| `imperium_footsoldier_badge` | Imperium Footsoldier Badge | material / junk | Uncommon combat scrap (sell/flavor) |

**Runtime gate:** thin inventory grant + equip/hotbar (extends TICKET-0111 / TICKET-0232 / DEC-0050) so “get” is playable, not lore-only. Positive soft-affinity multipliers can land with that thin path or immediately after.

**Item stats (2026-08-20):** catalog `stats` on Act 0 weapons, iron test armor, trinkets, and healing consumables. Inventory hover tooltip + properties pane show damage range, DPS, heal, and modifiers. Format: [`../formats/item-catalog-assets.md`](../formats/item-catalog-assets.md). Combat still uses play-test gates; these numbers are the UI contract for gear comparison.

```json
{"kind":"set_starter_archetype","archetypeId":"outrider"}
```

Aliases: `ashfell` / `melee`, `outrider` / `bow` / `archer`, `runecaster` / `magic`. Status reports `starterArchetypeId` + `starterWeaponItemId`. `apply_starter` swaps hotbar 0 without restarting play-test. World Forge → Archetypes author `starterWeaponItemId` per row; built-in defaults match the three starters. Loot bag/chest call `inventory_grant`. Combat sandbox `weapon_crate` uses a session **container** region (8 slots, drag to bag/hotbar). Inventory modal aligned to `inventory-ui.pen` (equip + 4 trinkets + bag grid + footer hotbar 8; bag-upgrade stubs). **Drag-and-drop** between bag / hotbar / equip / open crate slots (select click still works).

**Live player stats (2026-08-20):** Equipped armor/trinkets plus the **active hotbar weapon** feed `InventoryRuntime::compute_player_stats`. Baseline 100 health / 100 stamina. **Strength** scales melee and adds +2 health/point; **Agility** scales ranged and **+2% crit chance per point**; **Intellect** scales magic. **Rune pips** are magicka/focus: they show while a magic weapon is held (any lane) and hide otherwise. Stamina stays for dodge. Armor is physical resist; magic/poison/blight/holy/shadow resists use the same curve. Crit chance (gear `critChance` + agility) is a marble bag shown as % + mix. **Hit damage** draws an integer marble from the attribute-scaled `damageMin`…`damageMax` bag (not a fixed midpoint), then may crit ([DEC-0057](../decisions/index.md#dec-0057-marble-bag-rng-for-gameplay-rolls)). Open inventory (I): right-hand **Character** column (scrollable).

**Hotbar equip feedback (2026-07-31):** Selecting a hotbar slot (keys **1–8** / keypad, Terraria-shaped) *is* equipping. HUD + inventory footer tint the active slot gold; detail pane shows `EQUIPPED` for the active hotbar item. Keys work while the Game tab is focused (SDL edge + ImGui), not only when an ImGui window has focus. Active hotbar items with `worldMesh` draw as an unskinned hand-attached mesh. While the weld gizmo is enabled, play-test combat triggers (Attack / Block / Dodge) are suppressed so LMB can drag the gizmo.

**Bone welds (2026-07-31, TICKET-0246):** hand attach is a **weld** — `engine::BoneWeld` in [`include/engine/animation/bone_attachment.h`](../../include/engine/animation/bone_attachment.h), the engine analogue of a Roblox Motor6D C0. The socket chain is `entity placement → skinned mesh prefab part transform → joint global`, so a weld inherits the authored character scale (0.655 on the player prefab); `handAttach.gripScale` multiplies on top of that when a prop bake is the wrong size. `weld_from_world_transform` is the exact inverse used for manipulator write-back.

**Preferred authoring (TICKET-0250 / 0251):** **Animation** viewport → Diagnostics **Animation** → Held item combo + **Held Weapon Weld**, plus **Armor slots** (head / chest / legs). Skinned armor inherits the player bake bind (`matchPlayerBake`) and the 0.655 prefab scale — do not use instance `armorAttach` scale. Studio-session equip does not mutate play-test inventory bags. Gizmo draws in the Animation camera.

**Play-test fallback:** Inspector → **Held Weapon Weld** still works on the active hotbar item (Scene or Game view; **G**/**R**/**T**/**X** — not 1–2). The socket **freezes for the duration of a drag** so an animating joint cannot pull the handles out from under the cursor; Pause (F6) still freezes body and weld together for a fully static pose.

Default joint for an item with no authored `handAttach` is `RightHand`. Welds read the same joint globals the skin does, so they inherit the sagittal clip mirror from the RH→LH import (see [`../testing/findings.md`](../testing/findings.md)): while a clip plays, `RightHand` is the on-screen right hand, but an un-animated bind pose puts that joint on the opposite side. Author welds while a clip is sampled (Play / scrub), not against the bind pose.

**Player combat anim gate (2026-07-31):** Attack (LMB) and Block (Q hold) require the **active hotbar** weapon to carry tags `one_handed` + `melee` (Ashfell arming sword). With Ashfell equipped, LMB drives a buffered 3-hit light string (`attack` → `attack2` → `attack3`; TICKET-0268). Each committed swing costs **15** stamina; insufficient stamina blocks the next step. Dodge (Shift, directional WASD variants), Jump/Land/Fall, HitReact/Death/Revive, and Interact/InteractPickup do not require that gate.

**Melee movement (2026-08-20):** Ashfell Attack/Attack2/Attack3 live on the same `upperBody` overlay as Block, so Walk/Run keep the legs at full wish speed while the swings play on spine/arms. Dodge still owns displacement while the dash timer is active. Overlay swings cancel on dodge/hit/death/interact (full-body base states) and when ungrounded. Outrider BowDraw/BowAim/BowRelease use the same overlay, so aiming no longer freezes the legs. Runecaster MagicCast is on that overlay too: the focus arm coils back then thrusts forward while locomotion stays on base.

**Play-test dodge (2026-07-31):** Shift edge while grounded spends **20** from the HUD secondary resource (`player.resource` stamina), fires the matching dodge animator trigger, dashes ~**3.5 m** over **0.25 s** along camera-relative move wish (facing forward if idle), and spawns `dodge_dust` via `ParticleSystem::spawn_burst`. Insufficient resource blocks anim/dash/VFX. Resource regen is **25/s** after a **0.4 s** delay from the last spend (no regen mid-dash). Melee swings share this stamina pool and regen delay.

**Play-test bow ammo (2026-08-05):** Outrider starts with **20** `crude_arrow`. Draw is gated when ammo is empty; `releaseArrow` consumes one and skips the projectile if none remain. Ammo lives in its own inventory region (deep stacks, `kInventoryAmmoMaxStack`), so it never lands in the bag grid — the inventory panel renders it in a dedicated **Ammo** row (`inventory.ammo.N.icon` / `.count`) under Bags, and `engine_inventory_call status` reports it as `ammo`.

**Play-test Runecaster runes (2026-08-05):** Five discrete rune charges under the stamina bar (`hud_rune_pip_*`). Cast is gated at 0 charges; `castRelease` spends one; charges regenerate **+1 every 2 s**. Stamina remains for dodge.

**Art / acquisition stub (2026-07-30):** starter weapons + arrow + loot bag ship as sandbox Scene Assets. Trinkets / consumables are **icon-only** for now (`assets/ui/icons/items/` + `assets/items/act0_landfall_items.json`) and come from **loot bag** (`open_loot_bag`) or **supply chest** crate stand-in (`open_supply_chest`) — now granted into session inventory. No per-trinket world meshes.

**Owner lock (2026-08-05):** Act 0 starter weapons vertical is **in** — Ashfell / Outrider / Runecaster meshes + play-test cadence (melee string + stamina, bow draw/ammo, MagicCast + rune pips). Dedicated ammo icons: `crude_arrow.png`, `outrider_arrow.png` (no longer reuse the shortbow icon). HandAttach grip polish and further attack polish are deferred. Readiness: `art_starter_weapons` + `combat_weapon_feel` → done.

Checklist: `gameplay_act0_loot_slice` + `coding_inventory_thin_act0` in `act0_mvp_readiness.worldforge.json` (TICKET-0237) — **wip**.

## Thrator (SQ-13)

- Easter-egg **orc warlord** champion; target **Act 1 or Act 2** (not Act 0 Landfall).
- Side quest tied to a warband; Orgrimmar-flavored camp/arena fantasy.
- Kill reward: **glad mount** (+ optional gear drops).
- FT stays carriage-post based (DEC-0032); glad mount is a ground mount reward that soft-extends near-term “horses only” when implemented.

See [side-quest-catalog.md](../story/side-quest-catalog.md) **SQ-13**.

## Engine sequencing (summary)

1. Design / DEC-0048 (TICKET-0231) + inventory UX DEC-0050
2. **Act 0 loot slice** — thin inventory grant/hotbar/equip + concrete Landfall items (TICKET-0237)
3. Item/inventory models with **positive** lane multipliers (extends TICKET-0111 / TICKET-0232)
4. Simple action combat + universal weapon ability use
5. Act-tier + obscure-rare loot authoring (catalog scale-up)
6. Materials / mining stub → craft later
7. Thrator SQ + glad mount unlock hook (content + later mount runtime)

## Power progression ([DEC-0051](../decisions/index.md#dec-0051-no-xp-power-progression-and-quest-ux))

**No traditional XP / player level.** Character strength scales from:

1. **Gear quality** — act/boss loot bands; farmable main-storyline and **chapter bosses** (player-visible); act main-quest beats may also advance bands
2. **Archetype quest lines** — optional for campaign clear; required for **full lane power**; per-quest rewards = gear and/or ability (act-scaled)
3. **Story / act milestones** — ability unlocks alongside archetype quests
4. **Soft affinity + on-equip** — matching archetype bonuses; gear/trinkets may grant archetype-gated abilities while equipped ([DEC-0048](../decisions/index.md#dec-0048-terraria-shaped-gearing-with-soft-archetype-affinity) / [DEC-0050](../decisions/index.md#dec-0050-inventory-ux-item-kinds-and-positive-soft-affinity))

Provenance: [`../design/recording_archetype_quests_power_progression_2026-07-29.md`](../design/recording_archetype_quests_power_progression_2026-07-29.md). Act 0: **no named chapter boss** (2026-08-05); loot-band via Landfall completion milestones; first main-story boss = Pneumyra (A1-05).

## Out of scope (v1 of this epic)

- Tab-target combat as primary feel
- Per-weapon unique animation trees
- Full exotic mount roster beyond glad-mount hook
- Replacing carriage-post fast travel with mounts-only travel
- Making obscure rares required for main story
- Full Terraria-sized catalog in Act 0 (small slice only)
- Weight-based encumbrance
- Gear durability / repair sinks
- Traditional XP / player-level systems ([DEC-0051](../decisions/index.md#dec-0051-no-xp-power-progression-and-quest-ux))
- Independent Bernoulli loot/crit RNG (`math.random` / `if random < p`); gameplay rolls are marble bags ([DEC-0057](../decisions/index.md#dec-0057-marble-bag-rng-for-gameplay-rolls))
