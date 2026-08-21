# Item Catalog Assets (`assets/items/*.json`)

Status: active (schemaVersion 1)

Project item defs live under `samples/open-world-rpg/assets/items/`. `load_project_item_catalog` merges every `*.json` in that folder (later files win on id).

## Entity shape

```json
{
  "id": "ashfell_arming_sword",
  "displayName": "Ashfell Arming Sword",
  "kind": "weapon",
  "tags": ["one_handed", "melee"],
  "icon": "assets/ui/icons/items/ashfell_arming_sword.png",
  "worldMesh": "assets/models/ashfell_arming_sword.gltf",
  "stats": {
    "damageMin": 16,
    "damageMax": 22,
    "attacksPerSecond": 1.1,
    "modifiers": { "strength": 2 }
  }
}
```

`id` is required (slug from display name). `kind` is `weapon` | `armor` | `trinket` | `consumable` | `material`. Optional `iconOnly`, `notes`, `handAttach`, `armorAttach`, `tags`.

`armorAttach` is stored on armor defs but **not applied** as a post-skin instance transform. Skinned shells inherit `player.gltf` bind space via catalog `matchPlayerBake` and draw with a per-piece bone palette (player animation locals × the shell's inverse binds). Fit the mesh in Blockbench on GoodPlayerModelCopy and rebake. Origin-centered instance scale pulled helmets off the head.

## Stats (tooltip + equipped runtime)

Optional `stats` object. Hover chips and the inventory properties column format these lines from `ItemDef` (`format_item_stat_lines`). **Equipped armor/trinkets and the active hotbar weapon** also sum `modifiers` into live player totals (`InventoryRuntime::compute_player_stats`): base **100** max health / **100** max stamina, plus armor, strength, and the held weapon's damage range/DPS. HUD `player.healthMax` / `player.resourceMax` follow those caps (current HP/stamina is kept, then clamped if max drops). Incoming combat/swim damage is reduced by armor (`incoming * 100 / (100 + armor)`).

| Field | Meaning |
| --- | --- |
| `damageMin` / `damageMax` | Weapon damage range (lower / top end). If min > max and max > 0, they swap on load. Melee combo steps 1/2/3 use min/mid/max; otherwise combat rolls draw an integer marble from the attribute-scaled range ([DEC-0057](../decisions/index.md#dec-0057-marble-bag-rng-for-gameplay-rolls)). |
| `attacksPerSecond` | Cadence used for **DPS** = average(min, max) × APS |
| `heal` | Consumable restore amount (`Restores N Health`) |
| `modifiers` | Map of stat id → flat grant while the item is **equipped** (armor/trinket strip) or **held** (active hotbar weapon) |
| `onHit` | Array of status applications on a successful hit: `{ status, damagePerTick, duration, tickInterval }` (`bleed` / `poison` / `burn` / `slow` — [DEC-0060](../decisions/index.md#dec-0060-status-effects-runtime--typed-combat-text)). `slow` may set `damagePerTick` 0. Lightning chain hops are not authored here. |

`tags` may include combat school (`melee` / `ranged` / `magic`) plus optional spell flavor on magic weapons: `magic_arcane` (default), `magic_fire`, `magic_frost`, `magic_lightning`. Play-test MagicCast picks charge/bolt VFX from those tags. Fire/frost foci also author `onHit` burn/slow; lightning bolts chain to nearby hurt volumes. Tooltips on magic weapons start with `Spell  Fire` (etc.).

School scaling: held `melee` / `ranged` / `magic` tag picks Strength / Agility / Intellect at **+2% damage per point**. Strength also adds **+2 max health per point**. **Agility also adds +2% crit chance per point** (stacks with `critChance` mods before the marble bag). **Rune charge pips** (existing HUD) are magicka/focus: they show while a **magic** weapon is the active hotbar item and hide otherwise. Crit chance converts to a 20-marble bag (`displayed = crit/total`).

Weapons (melee / ranged / magic) use the same damage + APS fields. Armor and trinkets typically only set `modifiers`. Consumables typically only set `heal`.

## Related

- Feature: [`../features/gearing-system.md`](../features/gearing-system.md)
- Runtime: `include/engine/assets/item_catalog_asset.h`
- Inventory UI: [`../features/ui-canvas.md`](../features/ui-canvas.md)
