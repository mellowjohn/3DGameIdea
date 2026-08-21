# Combat sandbox

Status: active — flat pad for melee hit iteration (Ashfell string, hurt volumes)

World: [`samples/open-world-rpg/worlds/combat-sandbox.world.json`](../../samples/open-world-rpg/worlds/combat-sandbox.world.json)

Launch (does not change campaign `defaultWorld`):

```bat
build\windows-msvc-debug\dev-next\engine.exe editor --project samples/open-world-rpg --world worlds/combat-sandbox.world.json
```

Helper: [`tools/run-combat-sandbox-editor.cmd`](../../tools/run-combat-sandbox-editor.cmd). From a running campaign editor you can also **File → Open World → combat-sandbox**.

## Layout

Isolated terrain at `assets/terrain/instances/combat-sandbox/` so flatten/paint here does not rewrite the shared overworld stores. Grass pad ~4 m high around origin. Keep the player and `npc_humanoid` on that pad — moving them far off the sculpted heightfield leaves no floor. After a gizmo move, Save is safe: capsules/hurt sensors teleport with the mesh, placement cells retag, and the NPC is not a player spawn.

| Object | Purpose |
| --- | --- |
| `player` | Play-test spawn, south of the dummy ring (now has `combatHurt: body` so NPC swings can land) |
| `npc_humanoid` | Player-mesh hostile just north of spawn: chase, Attack overlay, reactive Block between swings, Ashfell in hand, `npc_body` hurt, HP chip ([npc-ai.md](../features/npc-ai.md)) |
| West camp (`camp_*`) | Lean-to camp ~26–30 m west of origin (`camp_fire` ≈`(-26,1)`). Layout: empty lean-to + rack south, chest/crate/barrel east of shelter, log/stump seats around the fire, barricade gate + `camp_hostile_sentry` on the east approach, and three ring hostiles (`camp_hostile_a`/`_b`/`_c`). Outside spawn aggro until you walk over. |
| `combat_weapon_crate` | Crate with a real stash: Ashfell sword, Outrider bow, **Fire / Frost / Lightning** rune foci, 50 crude arrows, plus status-test **Ashfell Bleed Sword** / **Outrider Poison Bow**. Press **E** to open inventory with a Crate panel; drag to/from bag and hotbar. Missing loadout rows are topped up on each open. Magenta **Guild Rune Focus** remains the Runecaster starter (`starter runecaster` / `give guild_rune_focus`). |
| `combat_armor_crate` | Crate beside the weapon stash with the modular iron test set (`iron_test_helmet` / `iron_test_chest` / `iron_test_legs`). Press **E**, drag pieces into **head / chest / legs**, then walk to confirm skinned shells follow Idle/Walk. Diagnostics `iron_test_set` still grants+equips the same kit without the crate. |
| `combat_dummy_front` / `_left` / `_right` / `_far` | Static Target Dummy prefabs with `combatHurt: dummy_body` |

Hits go to `assets/scripts/dummy_hurt.lua` (dummy HP on the blackboard, **not** player HUD). Each dummy upserts a world-space health chip after the first hit (locked above the dummy head via pad anchors — attack contact points do not move it), **never drops below 1 HP**, and regens **150 HP/s**. Hits also fire animator trigger `hit` → `HitReact` (then back to Idle). Health chips use a small label plate + separately bordered HP bar; hits pass `hitJuice` for flash / pulse / gold ghost trail. Damage also spawns floating combat text **above the name** (`engine.combat_text`; crits gold + bigger punch — [combat-text.md](../features/combat-text.md)). Melee combo steps scale damage min→mid→max. Status test weapons (`ashfell_bleed_sword`, `outrider_poison_bow`) apply bleed/poison DoTs with colored `-N` tick floaters and **icon chips + duration tickers** above the dummy HP plate ([status-effects.md](../features/status-effects.md)). School foci (`guild_rune_focus_fire` / `_frost` / `_lightning`) reuse the guild mesh and MagicCast clip; charge + bolt VFX follow `magic_fire` / `magic_frost` / `magic_lightning` tags. Fire applies a **burn** DoT, frost applies **Slow** (hostile chase at 40% wish), lightning **chains** up to two extra nearby hurt volumes at half damage. Hostile `npc_humanoid` uses `npc_body` HP that can reach 0. Hold **Q** with Ashfell to Block NPC melee (spark + `Blocked` text + stagger). The clone also **reads** some of your melee swings (marble bag, not every hit): after its Attack clip it can raise Block with a short reaction and hold the pose. Hit it during its swing, or when the read misses. Unblocked NPC hits chip HUD HP without staggering the player (no HitReact). Death freezes WASD / NPC chase. Diagnostics → **Recent combat hits** still lists contacts. If hits ever seem to stop mid-session after many contacts, ensure you are on a build with the combat event-queue cursor trim fix (2026-08-20 finding).

Play: Game tab, F5. Each F5 resets player HUD vitals, starter loadout, and Lua `combat.*` (NPC/dummy HP and death). The near `npc_humanoid` usually aggroes first; walk **west** to the `camp_*` fire for a three-hostile scrap. Walk to the crate (east of spawn), **E**, drag a weapon onto the footer hotbar, then LMB at dummy torso. Ashfell uses `hitFrame` melee probes; Outrider arrows and Runecaster bolts sample dummy hurt volumes in flight (they skip the player's own hurt sphere). Swap weapons back into the crate the same way. Ashfell is still the default hotbar-0 starter.
