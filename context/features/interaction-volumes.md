# Interaction Volumes

Prefab-authored trigger volumes that emit gameplay-facing enter/exit events.

## Authoring

Add an `interaction` string to a prefab `collision` entry (see `context/formats/prefab-assets.md`). When present, the volume is forced to `trigger` semantics and registered as an interaction sensor.

```json
{
  "shape": "sphere",
  "interaction": "use_campfire",
  "transform": { "position": [0.0, 0.55, 0.0], "rotation": [0, 0, 0, 1], "scale": [1, 1, 1] },
  "radius": 0.85
}
```

`assets/prefabs/campfire.prefab.json` in the sample project uses `use_campfire`.

## Runtime API

- `InteractionVolumeRegistry` maps `CollisionBody` tokens to `{placement_entity_id, volume_index, interaction_id}`.
- `PlacementCollisionTracker::interaction_registry()` rebuilds bindings when placed prefab collision syncs.
- `InteractionOverlapTracker::update(interactor_id, center, radius, world, registry)` compares trigger overlaps frame-to-frame and returns `InteractionEvent` enter/exit records without requiring a physics step.

## Debug integration

- **Debug world**: a `use_campfire` probe sphere spawns near the origin; the character overlap tracker fires enter/exit while walking into it.
- **Editor**: collision debug draws interaction volumes in gold. Physics contact events against registered interaction triggers append to **Recent interactions** in Diagnostics when placement collision is active.

## Limitations

- No prompt UI, scripting hooks, or quest bindings yet.
- Overlap queries use a spherical probe; capsule-accurate interaction remains future work.
