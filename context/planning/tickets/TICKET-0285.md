# TICKET-0285: Combat follow-on: hurt follow, parry, death freeze, move/save

- Epic: EPIC-0011
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/3c2d3efc569581b68d33eaa36d96c7da

## Goal

Player and hostile NPC can damage each other after they leave spawn, Block/parry works, death stops locomotion, and gizmo-moving then saving the player/NPC no longer breaks them. Play-test F5 also resets combat HP/stats, NPC animator/combo state stays off the player, and own projectiles do not self-hit.

## Context links

- `context/features/combat-volumes.md`
- `context/features/npc-ai.md`
- `context/testing/combat-sandbox.md`
- `context/formats/character-assets.md`
- `context/testing/findings.md` (2026-08-20 move/save, own-projectile self-hit)
- TICKET-0284 (sandbox hostile AI)

## Acceptance criteria

- [x] Physics-driven hurt sensors follow the motion body (`write_back_transforms`); melee query hits the new pose, not spawn
- [x] Authored gizmo/MCP move teleports the motion body + hurt sensors (same cell) instead of leaving capsules at the old pose
- [x] `Scene::set_transform` retags `placement.cell` so Save after physics follow / End Test cannot persist `WORLD-PLACEMENT-CELL-MISMATCH`
- [x] Hold Q with melee Block: incoming NPC melee deals no HP, sparks, shows `Blocked`, staggers the NPC (suite + host path; owner F5 for VFX)
- [x] Player HUD 0 / NPC HP 0 zeros wish and horizontal velocity (Death clip is not walked out of)
- [x] Combat-sandbox `npc_humanoid` is not a player spawn (`characterAsset` omitted; F5 skips `npcAi` even if selected)
- [x] F5 erases Lua `combat.*` (NPC/dummy HP and death) and refills player HUD health/stamina/runes; `opening.*` stays
- [x] NPC overlay/combo uses the NPC entity id; player `block`/`bowDrawn`/combo buffer do not drive the clone
- [x] Play-test arrows/bolts ignore the shooter's hurt volume (suite + `combat_hurt.lua` no-op for `player_attack`+`body`)

## Out of scope

- Character-vs-character collision
- NPC block / riposte
- Snapping moves to terrain height when leaving the sandbox pad
- Recast / patrol / unique NPC kits
- Full faction/team filter (only the owning placement is ignored)

## Dependencies

Follow-on to TICKET-0284. Uses TICKET-0268 hitFrame melee and TICKET-0283 Block overlay.

## Verification

- Rebuild `engine` (Debug MSBuild `engine` + `engine_suite_tests`) — succeeded, 0 errors, existing getenv/C4324/C4456 warnings only
- `engine_suite_tests --suite combat` — 40/40 (projectile ignore shooter hurt)
- `engine_suite_tests --suite scripting` — 72/72 (`blackboard_erase_prefix`)
- `engine_suite_tests --suite animator` — 510/511 (new two-instance isolation pass; leftover `attack chain start poses` is TICKET-0268 clip continuity, not this change)
- Editor + MCP reset after rebuild; combat-sandbox relaunched

## What changed

- Summary: Hurt/interaction sensors now ride physics-driven fighters, so player and NPC can still hit each other after leaving spawn. Editor gizmo/MCP moves teleport those bodies instead of leaving capsules behind. Save after a move (or after play-test physics follow) retags placement cells so the world can reload. Hold Q with a melee weapon parries NPC swings (spark + `Blocked` + stagger). Death zeros locomotion. The sandbox NPC is no longer a player spawn, so selecting it then F5 does not possess the clone. F5 wipes `combat.*` blackboard + refills HUD vitals. NPC animator params stay on the NPC instance. Arrows/bolts skip the shooter's hurt sphere.
- Files / surfaces touched: `prefab_collision` follow-sensors + authored teleport; `Scene::set_transform` cell retag; play-test spawn skip / strip `npcAi` `characterAsset`; combat host parry/death freeze/F5 reset/projectile ignore; `LuaRuntime::blackboard_erase_prefix`; combat-sandbox world JSON; combat/scripting/animator suites; feature docs + finding.
- Schema / API / format deltas: `CombatContactEvent.blocked` in Lua payload; `query_combat_hits*` `ignore_placement_entity_id`; `blackboard_erase_prefix`; no world schema change besides omitting NPC `characterAsset`.
- Seed / sample data: `combat-sandbox.world.json` `npc_humanoid` no longer has `characterAsset`.
- Tests / verification evidence: combat 40/40, scripting 72/72, animator 510/511 (isolation pass; leftover 0268 chain-pose). Engine rebuilt; editor relaunched on combat-sandbox. Lease `cursor-agent-0285` / `107f40033c640-1` released.
- Decisions & tradeoffs: same-cell authored moves teleport (cheap, keeps body ids); cell-crossing still full-rebuilds so Jolt cell buckets stay correct. F5 spawn never treats `npcAi` as the player even if a stale tag was saved. F5 combat reset is host-side (sandbox/play-test), not a campaign death/respawn system.
- Leftover risk / follow-ons: sandbox pad is small — moving far off the sculpted heightfield still has no floor. No character-vs-character collision. Owner F5 for parry spark / move-save / bow self-hit feel.

## Agent notes

Move+save break was two stacked bugs: physics-driven sync ignored gizmo transforms, and `npc_humanoid` carried `characterAsset` so F5 possessed the selected NPC. Cell mismatch on physics write-back would fail the next world load. Own-projectile self-hit was the nock/muzzle overlapping the player's `body` hurt with attacker id `player_attack` (not the placement id). NPC death persisted because `combat.npcDead.*` survived F5.
