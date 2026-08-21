# Status Effects (Poison / Bleed / Burn / Slow)

Status: active — [DEC-0060](../decisions/index.md#dec-0060-status-effects-runtime--typed-combat-text)

C++ owns status timers; catalog weapons apply on hit; combat text shows typed tick numbers. Lightning chain hops are a combat proc (not a lasting status).

## Ownership

| Layer | Owner |
| --- | --- |
| Timers / stacks | C++ `StatusEffectRuntime` |
| Item authorship | `stats.onHit[]` on item catalog defs |
| Apply / clear / anchors | Lua `engine.status_apply` / `status_clear` / `status_set_anchor` |
| Tick presentation | `CombatTextRuntime` kinds `bleed` / `poison` / `burn` (`-N`); Slow spawns `Slowed` on apply |
| Target status chips | World-space icons above afflicted HP chip (dummy `dummy.hp.*` / NPC `npc.hp.*`; not player HUD). Burn/Slow use tinted plates until dedicated droplet art exists.
| Dummy / NPC HP on tick | Lua `on_status_tick` in `dummy_hurt.lua` (loaded last; drains `combat.dummyHp.*` or `combat.npcHp.*`) |
| Player HP on tick | C++ HUD drain when `targetId == "player"` (anchor follows play-test feet) |

## Targets

| Target | Apply path | Stacks | Tick damage |
| --- | --- | --- | --- |
| Training dummy | `dummy_hurt.lua` weapon `onHit` | Yes (soft cap 10) | Lua → dummy HP (never below 1) |
| Hostile NPC | `npc_hurt.lua` weapon `onHit` | Yes | Lua → `combat.npcHp.*` (can kill) |
| Player | `engine.status_apply({ targetId = "player", ... })` | Yes | C++ HUD health |

Player weapons with `stats.onHit` (bleed sword / poison bow / school foci) apply on dummy **and** NPC hits. Same-kind reapply adds a stack and refreshes duration. Frost **Slow** cuts hostile chase wish to 40% (`kStatusEffectSlowWishScale`). Lightning bolts jump up to **two** extra nearby hurt volumes (8 m) at half damage.

## Stacking

| Case | Behavior |
| --- | --- |
| Bleed + poison | Both active at once (separate kinds). |
| Same kind again | Adds a **stack** (soft cap 10), refreshes duration, keeps the higher `damagePerTick`. |
| Tick damage | `damagePerTick × stacks` per interval. |
| HUD / status line | Icon chips under the target HP bar: droplet art, countdown seconds on a dark plate (outlined), thin remaining bar; stack badge `xN` on a gold-rimmed dark pill when stacked. |

## Catalog

```json
"stats": {
  "damageMin": 16,
  "damageMax": 22,
  "onHit": [
    { "status": "bleed", "damagePerTick": 3, "duration": 6, "tickInterval": 1 }
  ]
}
```

Known `status` ids: `bleed`, `poison`, `burn` (DoT), `slow` (duration only; `damagePerTick` may be 0). Lightning chain is not an `onHit` status — MagicCast bolts tagged `magic_lightning` hop from the primary combat contact.

## Melee combo damage

`comboStep` 1 / 2 / 3 → weapon min / mid / max (attribute-scaled). Ranged / step 0 still uses the damage marble bag.

## Test weapons

`assets/items/status_test_weapons.json`:

- `ashfell_bleed_sword` — sword clone + bleed
- `outrider_poison_bow` — bow clone + poison
- `guild_rune_focus_fire` — burn DoT
- `guild_rune_focus_frost` — Slow
- `guild_rune_focus_lightning` — chain hops (engine bolt proc)

Combat sandbox: open `combat_weapon_crate` (east of spawn) — bleed sword, poison bow, and the three school foci are seeded into the crate stash (topped up on each open if missing). Diagnostics still works: `give guild_rune_focus_fire` / `_frost` / `_lightning`, then `hotbar 0 …`.

## World target HUD

Bleed / poison / burn / slow chips draw **on the afflicted target** just below their world HP bar (duration seconds on a dark outlined plate + thin remaining bar + `xN` stacks on a gold-rimmed pill). Not on the player vitals bar.

## Related

- Combat text: [`combat-text.md`](combat-text.md)
- Gearing: [`gearing-system.md`](gearing-system.md)
- Format: [`../formats/item-catalog-assets.md`](../formats/item-catalog-assets.md)
