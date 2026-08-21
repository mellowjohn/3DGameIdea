# Combat Hit/Hurt Volumes

Prefab-authored trigger sensors for melee hit detection. Hit volumes represent active attacks; hurt volumes represent damageable regions.

## Authoring

Add `combatHit` or `combatHurt` to a prefab `collision` entry (see `context/formats/prefab-assets.md`). A volume cannot set both fields. When either is present, the volume is forced to trigger semantics.

```json
{
  "shape": "sphere",
  "combatHurt": "body",
  "transform": { "position": [0.0, 1.0, 0.0], "rotation": [0, 0, 0, 1], "scale": [1, 1, 1] },
  "radius": 0.9
}
```

Attack prefabs use `combatHit` (for example `sword_slash`). Character or enemy prefabs use `combatHurt` (for example `body`). The combat-sandbox Target Dummy uses `dummy_body` so hits drain dummy HP (`dummy_hurt.lua`) instead of player HUD HP (`combat_hurt.lua` on `body`). The sandbox hostile `npc_humanoid` uses `npc_body` (`npc_hurt.lua`) so it can die. Dummy HP is shown as a world-ui chip over the dummy, not the player health bar. Sandbox dummies never die (HP floors at 1) and regen quickly; `hit` plays the baked `HitReact` clip. Hostiles floor at 0 and fire `die`.

## Runtime API

- `CombatVolumeRegistry` maps `CollisionBody` tokens to `{placement_entity_id, volume_index, role, combat_id}`.
- `PlacementCollisionTracker::combat_registry()` rebuilds bindings when placed prefab collision syncs.
- `query_combat_hits(attacker_id, center, radius, world, registry, ignore_placement_entity_id = {})` returns `CombatContactEvent` records for hurt volumes overlapping a spherical attack probe.
- `query_combat_hits_along_segment(attacker_id, from, to, radius, world, registry, ignore_placement_entity_id = {})` samples the same probe along a flight path (play-test arrows and arcane bolts).
- `query_combat_hits_from_body(attacker_id, hit_body, world, registry, ignore_placement_entity_id = {})` resolves a registered hit volume body shape and runs the same hurt overlap query.

Pass the shooter's placement id as `ignore_placement_entity_id` so a projectile that leaves the nock/muzzle inside the owner's `combatHurt` sphere does not count as a self-hit. Melee probes pass the attacker entity the same way.

`CombatContactEvent` records attacker id, hurt placement id, hurt combat id, volume index, optional contact point, melee combo step, and `blocked` when a melee defender parried.

## Runtime follow

Physics-driven placements (player / hostile NPCs with a Rigidbody) spawn hurt/interaction sensors as kinematic triggers and **follow the entity pose** on each `write_back_transforms`. Editor gizmo / MCP / Inspector moves **teleport** the motion body and sensors to the authored pose (same cell) instead of leaving capsules at spawn. Crossing a partition cell still rebuilds the bodies so they are tagged to the new cell. Static dummies keep authored sensor poses. Without follow, melee probes miss as soon as a fighter leaves spawn.

## Debug integration

- **Debug world**: a `body` hurt probe spawns west of the origin; each frame the character attack probe runs `query_combat_hits` while moving.
- **Editor**: collision debug draws hit volumes in red and hurt volumes in magenta. Registered hit bodies are queried each physics step; contacts append to **Recent combat hits** in Diagnostics.

## Limitations

- Spherical probe only; swept capsules and animation-driven shapes remain future work.
- Play-test Ashfell swings (TICKET-0268 / TICKET-0285) arm a short forward **segment** probe on animator `hitFrame` (0.2–1.7 m, 0.9 m radius) and dispatch contacts into `recent_combat_events` / Lua (hit-once per swing). Player `upperBody` **Block** (hold Q with a melee weapon) marks NPC melee contacts `blocked`: no HP, `Blocked` combat text, parry spark (`sword_impact_flash` + `hit_spark`), and a short NPC HitReact stagger. Hostile NPCs **read** some incoming player melee overlays (3-read / 2-miss bag, ~0.16 s reaction, hold Block through the swing plus ~0.75 s): a covered swing is also `blocked` (`npc_hurt.lua` skips HP / HitReact). Recovery cooldown does not tick during the NPC’s own Attack clip. Unblocked contacts spawn `sword_impact_flash` + `sword_impact` and chip HUD HP **without** player HitReact. Impact emitters use `softOcclusion: false`. Melee impact/parry VFX and combat text prefer the hurt placement’s **live feet** (`hurtEntityPosition`) over hurt-sphere contact points (surface contacts read as a radial miss).
- Play-test Outrider arrows and Runecaster bolts (TICKET-0260) sample hurt volumes along each flight step and dispatch the same Lua path (`dummy_body` → `dummy_hurt.lua`). One projectile, one hurt placement; they still stop on **StaticWorld** solid ray (terrain/props) if they miss a hurt volume. The probe **ignores the shooter's placement** (nock/muzzle starts inside the player's `body` hurt). Solid `ray_cast` is filtered to `StaticWorld` so Dynamic/Character capsules (including the shooter) never eat the shot. Combat impact VFX spawns on the projectile tip (hurt-sphere contact is only used to stop the projectile — remapping VFX to contact XZ caused a fixed radial world offset). Ground/expiry keep dust. Impact emitters use `softOcclusion: false`.
- Continuous Hit-volume overlap (authored attack prefabs) is diagnostics-only and does **not** enqueue into the play-test Lua combat queue while a test session is running — melee/projectile own that path.
- `recent_combat_events` / `recent_interaction_events` keep the newest 32 for Diagnostics; trim repairs the Lua dispatch cursor (`trim_dispatched_event_queue`) so hits do not silently stop after the ring fills.
- No team/faction filtering yet beyond ignoring the attacking placement's own hurt volume.
