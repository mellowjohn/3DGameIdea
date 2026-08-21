# TICKET-0284: Combat sandbox hostile NPC AI

- Epic: EPIC-0011
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/3c2d3efc569581a2a3a7f530c26dba21

## Goal

A combat-sandbox humanoid clone of the player can aggro, chase, swing, take hits, and die so we can iterate NPC combat without waiting on Recast, companions, or kit art.

## Context links

- `context/features/npc-ai.md`
- `context/features/combat-volumes.md`
- `context/features/character-controller.md`
- `context/testing/combat-sandbox.md`
- `context/formats/character-assets.md`
- DEC-0022 (animator C++ backend, Lua drive)
- DEC-0038 (authored rigidbody locomotion)
- DEC-0055 (game_module is default for *new* native gameplay; locomotion/physics stay engine_core until ABI grows)

## Acceptance criteria

- [x] Prefab field `npcAi.kind=hostile` round-trips in `PrefabAsset` load/save
- [x] `tick_hostile_npc` covers idle / chase / attack / cooldown / death (named `combat` suite)
- [x] Combat sandbox `npc_humanoid` chases the player in F5 play-test (live mesh, not static cache)
- [ ] Hostile plays Attack in range; player melee damages `npc_body` (HP chip + HitReact); NPC death fires `die`
- [x] NPC `hitFrame` probe can hit player `body` (HUD HP via `combat_hurt.lua`); self-hits are skipped
- [x] `npc_test` talk stand-ins are unchanged (no `npcAi`)
- [x] Hostile welds Ashfell (`npcAi.heldItemId`) without stomping the player's hand weld

Chase is proven (NPC rigidbody reached player spawn during F5; live Game stills show the clone walking with Ashfell). NPC melee can drop player HUD to 0. Player-vs-NPC HP still needs one clean owner swing before the clone finishes you.

## Out of scope

- Patrol, companion follow, faction filters
- Nav-grid pathing / Recast
- NPC held weapons or unique meshes (sandbox now welds Ashfell; kits still later)
- game_module ABI expansion for movement
- Behavior-tree authoring

## Dependencies

Uses TICKET-0227 GPU skinning, TICKET-0198 rigidbody loco, TICKET-0268 hitFrame melee, TICKET-0283 upper-body Attack overlay. Does not block TICKET-0128 (full M9 one-enemy slice).

## Verification

- Rebuild `engine` (Debug MSBuild `engine` target) — succeeded after killing locked `engine.exe`
- `engine_suite_tests --suite combat` — 23/23
- `engine_suite_tests --suite scripting` — 65/65 (`npc_body` binding)
- Desktop: combat-sandbox Game F5 — one `npc_humanoid` at `(0,-3)` walks toward spawn with Ashfell; Hostile chip; NPC melee dropped player HUD to 0. Player swing vs NPC HP not captured (clone finished the player first).

## What changed

- Summary: Combat sandbox can now host a hostile player-mesh NPC. Prefabs opt in with top-level `npcAi.kind=hostile`. During Game F5 the engine chases the player with rigidbody locomotion, fires the Attack overlay in range, and uses the existing `hitFrame` melee probe. NPC HP lives on the Lua blackboard (`combat.npcHp.<id>`) with a world-space chip; death sets `combat.npcDead.<id>` and triggers `die`. Follow-on: hostile meshes skip the static render cache (so chase is visible) and weld `heldItemId` Ashfell on a per-NPC joint, not the player's weld.
- Files / surfaces touched: `include/engine/ai/hostile_npc.h`, `src/ai/hostile_npc.cpp`, `engine_core` CMake; prefab `npcAi` schema; play-test host in `render_app.cpp`; `npc_humanoid` prefab/character; player prefab `combatHurt: body`; `npc_hurt.lua` + `bindings.script.json`; combat-sandbox placement; `context/features/npc-ai.md` and related indexes.
- Schema / API / format deltas: optional prefab object `npcAi` (`kind`, `displayName`, `heldItemId`, aggro/attack radii, cooldown, `moveSpeed`). Not an Add Component type.
- Seed / sample data: `assets/prefabs/NPC/npc_humanoid.prefab.json` placed as `npc_humanoid` in `combat-sandbox.world.json` (north of player spawn). `npc_test` talk stand-ins unchanged.
- Tests / verification evidence: combat 23/23, scripting 65/65. Engine rebuilt and editor/MCP relaunched. Follow-on F5: live mesh chased from z=-3 to z≈-6.1 with Ashfell in hand, Hostile 100/100 chip, player HUD went 100→0. Player-vs-NPC HP still needs a swing before the clone finishes you.
- Decisions & tradeoffs: brain stays in `engine_core` (Lua has no move API; `game_module` ABI is still log/blackboard). Straight-line wish, no Recast, no character-vs-character collision. Held Ashfell uses a per-NPC weld so Animation Studio / player `held_attach_weld` is untouched. Hostile meshes are excluded from the static render cache during F5 the same way the player is — otherwise the brain chases while the drawn body stays at the F5-start pose.
- Leftover risk / follow-ons: owner F5 swing to chip NPC HP before the clone finishes you; NPC can walk through the player; melee probe is 1 m in front so overlapping bodies may miss the other hurt sphere.

## Agent notes

Owner asked for hostile sandbox fighter as the first NPC AI vertical slice (2026-08-20). Rebuild lease `cursor-agent-0284` expired/reclaimed after the engine rebuild + editor relaunch.
