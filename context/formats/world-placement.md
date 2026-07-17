# World Placement Contract

Placed world objects are ordinary UUID scene entities with transforms plus an optional `placement` object:

```json
{
  "placement": {
    "prefab": "assets/prefabs/tree.prefab.json",
    "cell": [0, 0],
    "characterAsset": "assets/characters/player.character.json"
  }
}
```

The optional `characterAsset` field marks a placement as a **player spawn**. When present (or inferred from the prefab's `characterAsset` field / a character asset's `visualPrefab` match), F5 test sessions spawn the controllable character at that entity's transform instead of creating a duplicate mesh at the cursor. Only one player spawn is used automatically when exactly one exists in the scene; otherwise the selected player spawn is preferred. Runtime movement during a test session does not persist: ending the test restores the spawn transform to its pre-test position.

Optional `characterSettings` stores per-spawn overrides (capsule size, movement, gravity, visual prefab path) authored in the editor Inspector. Values match the `.character.json` schema and round-trip with the scene file.

## Authored components (DEC-0016 / DEC-0017)

Full catalog (core ECS + authored types, authoring matrix, extension checklist): [`../architecture/components.md`](../architecture/components.md).

Entities may include a `components` array. Collider, `scriptBinding`, and `animator` entries are seeded from the prefab on place. Prefab-linked entries use `"source": "prefab"` and `"overridden": false` until an instance edit sets `"overridden": true`. Prefab saves propagate into non-overridden instance components.

Legacy worlds that only reference a prefab (no `components` array) still spawn physics from prefab `collision[]`. The editor **exposes those as entity components** on load/select via `seed_missing_authored_components` / `ensure_authored_components_seeded`, so Inspector edit and green overlays share one authored list. Save the world to persist the seeded `components` array.

```json
"components": [
  {
    "id": "collision-0",
    "type": "collider",
    "source": "prefab",
    "overridden": false,
    "data": {
      "shape": "box",
      "layer": "staticWorld",
      "trigger": false,
      "halfExtent": [0.5, 0.5, 0.5]
    }
  },
  {
    "id": "script-0",
    "type": "scriptBinding",
    "source": "instance",
    "overridden": true,
    "data": { "kind": "handler", "bindingId": "use_campfire" }
  }
]
```

Commands: `add-entity-component`, `remove-entity-component`, `set-entity-component` (also via MCP `engine_scene_apply` / `engine_entity_component_apply`).

The prefab path must be project-relative and begin with `assets/`. Cell ownership is derived from the world transform using the partition configuration; authored cell values that disagree with position fail scene validation.

## Command contract

`place-world-object`, `move-world-object`, `remove-world-object`, and entity component add/remove/set run through `CommandHistory`. Each supports undo/redo and reports a deterministic action summary plus changed entity UUIDs. Moving an object across a 128 m boundary updates cell ownership in the same transaction. Removing an object with children is rejected until hierarchical removal semantics are explicitly implemented.

This layer is the authoritative backend for editor drag-and-drop, transform gizmos, deletion, save operations, and AI automation. The GUI must not mutate ECS components directly.
