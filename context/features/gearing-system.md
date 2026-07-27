# Terraria-shaped gearing system

- Status: planned (design locked)
- Decision: [DEC-0048](../decisions/index.md#dec-0048-terraria-shaped-gearing-with-soft-archetype-affinity)
- Epic: [EPIC-0018](../planning/epics.md) (TICKET-0231+)
- Related: [DEC-0009](../decisions/index.md#dec-0009-starting-archetype-character-creation), [DEC-0044](../decisions/index.md#dec-0044-starting-archetype-lane-orgs-and-rename), [DEC-0032](../decisions/index.md#dec-0032-open-world-travel-discovery-map-and-dual-soft-gates), TICKET-0111, EPIC-0011 melee slice
- Provenance: Dom + John design recordings / chat 2026-07-27 (transcript “Ledgerport” → canon **Ledgeport**; “Thraador” → **Thrator**)

## Product intent

Combat stays **simple** (Souls-lite action + three weapon chains). Gameplay depth comes from a large pool of **items and trinkets with unique effects**, oriented around the three starting archetypes but **not class-locked**.

## Soft archetype affinity

| Rule | Detail |
| --- | --- |
| Equip | Any archetype can equip any weapon / armor / trinket |
| Use | Weapon abilities and item on-use effects are available to whoever equips them |
| Benefit | Matching lane (**Ashfell Blade** / **Outrider** / **Runecaster**) gains better efficiency via **stat allocation / lane multipliers** |
| Co-op | Players may gift/trade items; off-lane use is intentional comedy/power fantasy |

Do **not** implement hard “cannot equip” gates for lane mismatch.

## Combat baseline

- Three chains: melee, ranged, magic (maps to the three lanes).
- Prefer shared swing/shot/cast animations; flavor from item VFX, procs, passives, and on-use.
- Ability **caveats** OK (stamina/mana, cooldown, stance, situational) without BDO-style animation complexity or tab-target rotation design.

## Act tiers and rares

- Commons/uncommons track **act power bands**.
- **Obscure rares** may exceed their act band and remain useful into later acts.
- First-time playthrough should rarely stumble into the wildest rares without deep exploration or external knowledge; story completion must not require them.
- Bosses support **common + rare** tables and optional farm replay when systems allow.

## Acquisition loops

1. World chests / exploration finds
2. Vendors — including **Ledgeport** undermarket / black-market flavor (post–Act 0 camp travel)
3. Mining ores and crystals → materials → crafting (full loop after inventory; Act 0 may stub materials)
4. Named bosses / champions (later acts for easter-egg warlords)

### Act 0 Landfall loot slice (MVP)

Act 0 is **not** a full item pool, but players **must** be able to pick up / receive **some** items during Landfall so gearing fantasy starts in the vertical slice.

| Source | Intent |
| --- | --- |
| Starter kit | Per-archetype default weapon (already on readiness: `art_starter_weapons` / `archetype_starter_kits_bound`) |
| Siege approach finds | 1–2 world containers or fallen-soldier pouches on the Calrenoth approach |
| Combat drops | Low-rate common from Imperium footsoldiers (bandage / scrap / minor trinket)—not required for waves |
| Story beat reward | One granted item from Arkand rescue, Grenge, or camp handoff |
| Optional obscure nook | At most **one** hard-to-spot Act 0 rare (above-band OK per DEC-0048); never story-gated |

**Target count:** roughly **4–8** distinct Act 0 item defs (including starter weapons). Effects can be light (stat bump, small heal, flavor passive)—unique VFX optional.

**Runtime gate:** thin inventory grant + equip (extends TICKET-0111 / TICKET-0232) so “get” is playable, not lore-only. Full soft-affinity multipliers can land with that thin path or immediately after.

Checklist: `gameplay_act0_loot_slice` + `coding_inventory_thin_act0` in `act0_mvp_readiness.worldforge.json` (TICKET-0237).

## Thrator (SQ-13)

- Easter-egg **orc warlord** champion; target **Act 1 or Act 2** (not Act 0 Landfall).
- Side quest tied to a warband; Orgrimmar-flavored camp/arena fantasy.
- Kill reward: **glad mount** (+ optional gear drops).
- FT stays carriage-post based (DEC-0032); glad mount is a ground mount reward that soft-extends near-term “horses only” when implemented.

See [side-quest-catalog.md](../story/side-quest-catalog.md) **SQ-13**.

## Engine sequencing (summary)

1. Design / DEC (TICKET-0231) — this note
2. **Act 0 loot slice** — thin inventory grant/equip + 4–8 Landfall items (TICKET-0237) — MVP foreshadow
3. Item/inventory models with lane multipliers (extends TICKET-0111 / TICKET-0232)
4. Simple action combat + universal weapon ability use
5. Act-tier + obscure-rare loot authoring (catalog scale-up)
6. Materials / mining stub → craft later
7. Thrator SQ + glad mount unlock hook (content + later mount runtime)

## Out of scope (v1 of this epic)

- Tab-target combat as primary feel
- Per-weapon unique animation trees
- Full exotic mount roster beyond glad-mount hook
- Replacing carriage-post fast travel with mounts-only travel
- Making obscure rares required for main story
- Full Terraria-sized catalog in Act 0 (small slice only)