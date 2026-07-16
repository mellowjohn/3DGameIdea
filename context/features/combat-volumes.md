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

Attack prefabs use `combatHit` (for example `sword_slash`). Character or enemy prefabs use `combatHurt` (for example `body`).

## Runtime API

- `CombatVolumeRegistry` maps `CollisionBody` tokens to `{placement_entity_id, volume_index, role, combat_id}`.
- `PlacementCollisionTracker::combat_registry()` rebuilds bindings when placed prefab collision syncs.
- `query_combat_hits(attacker_id, center, radius, world, registry)` returns `CombatContactEvent` records for hurt volumes overlapping a spherical attack probe.
- `query_combat_hits_from_body(attacker_id, hit_body, world, registry)` resolves a registered hit volume body shape and runs the same hurt overlap query.

`CombatContactEvent` is a stub: it records attacker id, hurt placement id, hurt combat id, volume index, and optional contact point. No damage, invulnerability, or animation timing yet.

## Debug integration

- **Debug world**: a `body` hurt probe spawns west of the origin; each frame the character attack probe runs `query_combat_hits` while moving.
- **Editor**: collision debug draws hit volumes in red and hurt volumes in magenta. Registered hit bodies are queried each physics step; contacts append to **Recent combat hits** in Diagnostics.

## Limitations

- Spherical probe only; swept capsules and animation-driven shapes remain future work.
- No damage application, combo rules, or team/faction filtering.
- Trigger-trigger physics contacts are not used; overlap queries drive detection.
