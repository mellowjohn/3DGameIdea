# NPC AI (sandbox hostile)

Status: active (TICKET-0284) — first play-test brain: a hostile humanoid in the combat sandbox.

## What ships

Play-test finds placed prefabs with `npcAi.kind = "hostile"` and, while Game F5 is running:

1. Drives their authored **Rigidbody** with the same `RigidbodyLocomotion` path as the player. When not aggroed and `patrolWaypoints` are authored, they **patrol** a loop (local XZ offsets from spawn). Chase and patrol both set a move goal; the play-test host steers along a nav-grid `find_path` when cells are loaded (falls back to straight wish).
2. Sets animator `moveX` / `moveZ` / `grounded` / `block` on **that NPC's entity id** (never the player's combo buffer / `bowDrawn`) and fires `attack` when in range. Overlay params are per `AnimatorRuntime` instance.
3. Arms a `hitFrame` melee **segment** probe that can hit the player's `combatHurt: body` volume (`combat_hurt.lua` → HUD HP). The probe uses the NPC's own overlay combo step, not the player's. Unblocked hits chip HUD HP **without** player HitReact (no stagger interrupt). Player Block (Q, melee held) parries: no HP, spark burst, `Blocked` floater, NPC HitReact stagger. After its own Attack clip, recovery cooldown ticks (it does not start the next swing until that clip ends). Each new player melee overlay (`attack` / `attack2` / `attack3`) is a **read**: a 3-read / 2-miss marble bag decides whether to cover. A read waits ~0.16 s then holds `upperBody` Block through the swing plus ~0.75 s so the guard is visible. A miss leaves them open (they may trade if cooldown is ready). They do not block during their own Attack clip.
4. Takes hits on `combatHurt: npc_body` (`npc_hurt.lua` → NPC HP / HitReact / Death). Hurt sensors **follow** the live rigidbody (same as the player).
5. Death (`combat.npcDead.<id>` / player HUD 0) zeros wish, horizontal velocity, and `moveX`/`moveZ` so the Death clip is not walked out of.
6. Skips the NPC from the static prop cache during F5 (same as the player) so the mesh tracks the live transform. Welds `heldItemId` (default Ashfell) to the authored hand joint — a **local** weld, not the player's `held_attach_weld`.

Sample: `assets/prefabs/NPC/npc_humanoid.prefab.json` placed as `npc_humanoid` in `worlds/combat-sandbox.world.json`. Patrol demo: `npc_humanoid_patrol.prefab.json` on `camp_hostile_sentry` (local rectangle waypoints + smaller aggro so the loop is visible before chase). Mesh is still GoodPlayerModel (`player.gltf`) — kits come later. Place hostiles **ahead of player spawn** (looking +Z at the dummies) so F5 shows clones walking toward the camera. **Do not** stamp `characterAsset` on the placement — that makes F5 treat the clone as the player spawn if it is selected after a move. Engine sync strips `characterAsset` from `npcAi` prefabs.

## Prefab authoring

Top-level prefab field (not an Add Component type):

```json
"npcAi": {
  "kind": "hostile",
  "displayName": "Hostile",
  "heldItemId": "ashfell_arming_sword",
  "aggroRadius": 16,
  "loseAggroRadius": 22,
  "attackRange": 1.9,
  "attackCooldown": 1.35,
  "moveSpeed": 3.5,
  "patrolArrive": 1.25,
  "patrolWaypoints": [
    {"x": 0, "z": 0},
    {"x": 7, "z": 0},
    {"x": 7, "z": 5},
    {"x": 0, "z": 5}
  ]
}
```

`patrolWaypoints` are **local XZ offsets from the placement spawn**. Empty / omitted = no patrol (idle until aggro). Empty / missing `kind` means no brain. `npc_test` talk stand-ins stay idle.

Required on the same prefab: animator (player controller is fine), capsule collider, dynamic rigidbody, `combatHurt: npc_body`.

The **player** prefab now also has `combatHurt: body` so NPC swings can land. Player melee and projectile probes skip the attacker’s own hurt volume. Hold **Q** with a one-handed melee weapon to Block; a blocked NPC swing sparks and staggers the clone instead of chipping HUD HP. Unblocked NPC hits never fire the player `hit` / HitReact trigger. The clone tries to **read** some of your swings (not all): a short reaction, then a held Block pose. Hit it during its Attack, or on swings it misses.

Each F5 start (combat sandbox included) erases Lua `combat.*` blackboard keys so NPC/dummy HP and death flags reset, then refills player HUD health/stamina/runes from the starter loadout. `opening.*` keys are left alone.

## Ownership

| Layer | Owner |
| --- | --- |
| Chase / stop-and-swing / attack-read Block / patrol goals | C++ `tick_hostile_npc` (`include/engine/ai/hostile_npc.h`) |
| Nav-grid path follow for move goals | Play-test host + `StreamedNavigationField::find_path` |
| Locomotion / animator / hit probes | Play-test host (engine_core) |
| NPC HP, HitReact, Death | Lua `npc_hurt.lua` + blackboard `combat.npcHp.*` / `combat.npcDead.*` |
| HP chip follow | C++ world UI (`npc.hp.<entityId>`) |

Lua still has no movement API. Native gameplay hot-reload (`game_module`) cannot drive Jolt yet — this brain stays in `engine_core` until the ABI grows query/command slices.

## Out of scope (follow-ons)

- Companion / faction teams
- Recast/detour navmesh (TICKET-0109)
- Character-vs-character collision
- NPC riposte (auto-attack immediately after a successful block)
- Unique NPC kits (sandbox reuses GoodPlayerModel + Ashfell weld)
- Behavior trees / authored graphs

## Related

- Combat volumes: [combat-volumes.md](combat-volumes.md)
- Navigation grid / pathfinding: [navigation-grid.md](navigation-grid.md)
- Character controller: [character-controller.md](character-controller.md)
- Combat sandbox: [../testing/combat-sandbox.md](../testing/combat-sandbox.md)
- Character assets: [../formats/character-assets.md](../formats/character-assets.md)
