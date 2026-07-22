# Prefab Assets

Prefabs are versioned JSON files under `assets/prefabs/` that describe placeable world objects.

Accepted compositional authoring rules: [DEC-0008](../decisions/index.md#dec-0008-compositional-prefab-meshes-from-primitives).

## Schema v1 (current)

### Required fields

- `schemaVersion`: integer schema version (currently `1`)
- `mesh`: project-relative glTF path used by the editor renderer for single-mesh prefabs
- `entities`: prefab-local entity definitions used by future instantiation tooling

### Optional player spawn binding

Prefabs may declare which character asset they represent when placed in a level:

```json
{
  "characterAsset": "assets/characters/player.character.json"
}
```

When this field is present (or when a character asset's `visualPrefab` matches the prefab path), editor placement tags the scene entity as a **player spawn**. F5 test sessions use that placement instead of spawning a duplicate at the cursor.

### Optional point light

Prefabs may include a warm local light for landmarks and atmosphere tests:

```json
{
  "light": {
    "color": [1.0, 0.62, 0.28],
    "radius": 20.0,
    "strength": 1.35,
    "offset": [0.0, 0.35, 0.0]
  }
}
```

- `color`: linear RGB multipliers
- `radius`: attenuation radius in meters
- `strength`: brightness scale
- `offset`: local-space offset from the placed transform position

The debug renderer evaluates up to two nearest placed prefab lights each frame. Dependency sidecars must list external mesh paths referenced by the prefab.

Sample: `assets/prefabs/Scene Assets/sun.prefab.json` — large-radius daylight fill light used by the open-world vertical slice.

## Schema v2 (active): compositional mesh parts

Multi-part prefabs extend `entities` so each child can carry its own mesh source and local transform. Authors move, rotate, and scale parts inside the prefab to build props from primitives or imported meshes.

### Built-in primitives

| Primitive | Intended use |
|-----------|----------------|
| `cube` | Blocks, stone rings, crate forms |
| `pyramid` | Flame tips, roof wedges, spikes |
| `cylinder` | Trunks, logs, posts |
| `sphere` | Canopies, rocks, ember blobs |
| `capsule` | Player bodies, character placeholders |

Primitives are project-owned, low-poly, and vertex-colored. Tessellation stays deliberately coarse to match the smooth low-poly art direction.

### Per-entity mesh descriptor

Each prefab entity may include exactly one mesh source:

```json
{
  "schemaVersion": 2,
  "entities": [
    {
      "id": "a4f2c8e1-9b3d-4f7a-8c2e-1d5b6a903f41",
      "name": "Log A",
      "transform": {
        "position": [0.15, 0.08, 0.0],
        "rotation": [0.0, 0.0, 0.383, 0.924],
        "scale": [0.7, 0.12, 0.12]
      },
      "parent": null,
      "mesh": {
        "primitive": "cylinder",
        "color": [0.24, 0.13, 0.07]
      }
    },
    {
      "id": "b5a3d1e2-4c6f-5a8b-9d0e-2e6f7a804c52",
      "name": "Flame",
      "transform": {
        "position": [0.0, 0.45, 0.0],
        "rotation": [0.0, 0.0, 0.0, 1.0],
        "scale": [0.35, 0.55, 0.35]
      },
      "parent": null,
      "mesh": {
        "primitive": "pyramid",
        "color": [1.0, 0.72, 0.18]
      }
    }
  ],
  "light": {
    "color": [1.0, 0.62, 0.28],
    "radius": 20.0,
    "strength": 1.35,
    "offset": [0.0, 0.35, 0.0]
  }
}
```

Imported mesh parts use an asset reference instead of `primitive`:

```json
"mesh": {
  "asset": "assets/models/dead-tree.gltf"
}
```

### Optional collision volumes (schema v2+)

Prefabs may declare authored collision parts independent of render meshes:

```json
"collision": [
  {
    "shape": "sphere",
    "layer": "trigger",
    "trigger": true,
    "transform": {
      "position": [0.0, 0.55, 0.0],
      "rotation": [0.0, 0.0, 0.0, 1.0],
      "scale": [1.0, 1.0, 1.0]
    },
    "radius": 0.85
  },
  {
    "shape": "box",
    "layer": "staticWorld",
    "trigger": false,
    "transform": {
      "position": [0.0, 0.2, 0.0],
      "rotation": [0.0, 0.0, 0.0, 1.0],
      "scale": [1.0, 1.0, 1.0]
    },
    "halfExtent": [0.5, 0.2, 0.5]
  }
]
```

- `shape`: `box`, `sphere`, or `capsule`
- `id`: optional stable id (defaults to `collision-N` on load); used for Unity-like instance inheritance
- `layer`: `staticWorld`, `dynamic`, `character`, or `trigger`
- `trigger`: when true, the volume is a sensor regardless of `layer`
- `interaction`: optional gameplay id; when present the volume is a trigger interaction sensor
- `combatHit`: optional attack id; when present the volume is a trigger hit sensor
- `combatHurt`: optional hurtbox id; when present the volume is a trigger hurt sensor (`combatHit` and `combatHurt` are mutually exclusive)
- `transform`: local-space offset relative to the placed prefab root
- `halfExtent`: box half extents in meters (required for `box`)
- `radius`: sphere/capsule radius in meters (required for `sphere` / `capsule`)
- `halfHeight`: capsule cylinder half-height in meters (required for `capsule`; total height ≈ `2*(halfHeight+radius)`)

### Optional `components` array (script bindings / animator / rigidbody / audioSource)

Non-collider components on the prefab asset:

```json
"components": [
  {
    "id": "script-0",
    "type": "scriptBinding",
    "data": { "kind": "handler", "bindingId": "use_campfire" }
  },
  {
    "id": "animator-0",
    "type": "animator",
    "data": { "controller": "assets/animators/player.animator.json", "defaultState": "idle" }
  },
  {
    "id": "rigidbody-0",
    "type": "rigidbody",
    "data": {
      "motionType": "dynamic",
      "mass": 1.0,
      "linearDamping": 0.0,
      "angularDamping": 0.05,
      "useGravity": true,
      "freezeRotation": false
    }
  },
  {
    "id": "audio-0",
    "type": "audioSource",
    "data": {
      "clip": "assets/audio/campfire_crackle.wav",
      "volume": 1.0,
      "loop": false,
      "spatial": true,
      "playOnStart": false,
      "minDistance": 0.5,
      "maxDistance": 40.0
    }
  }
]
```

`kind` is `interaction`, `combatHit`, `combatHurt`, or `handler`. Animator `controller` is a project-relative `*.animator.json` ([`animator-controller-assets.md`](animator-controller-assets.md)). **Rigidbody** ([DEC-0038](../decisions/index.md#dec-0038-authored-rigidbody--dynamic-bodies-for-player-and-entities)): `motionType` is `dynamic` or `kinematic`; `mass` must be positive; damping ≥ 0. Runtime spawn of Jolt bodies is TICKET-0197. **AudioSource** (TICKET-0210): `clip` is project-relative `.wav`; `volume` in [0,1]; `maxDistance` ≥ `minDistance` > 0. Runtime playOnStart / Lua entity trigger is TICKET-0211. Collider entries may also appear under `components` with `type: "collider"` and are merged into `collision[]` on load. Existing `collision[]`-only prefabs remain valid ([DEC-0017](../decisions/index.md#dec-0017-prefab-and-scene-component-authoring-with-unity-like-inheritance)).

On placement, `spawn_prefab_collision()` / `PlacementCollisionTracker` use entity authored components when present (effective volume = local override or linked prefab volume by id); otherwise they fall back to prefab `collision[]`.

### Compatibility rules

- `schemaVersion: 1` prefabs keep the top-level `mesh` field. Renderers treat them as a single implicit part at the prefab root.
- `schemaVersion: 2` prefabs omit the top-level `mesh` when all geometry is expressed through entity parts.
- World placement still references the prefab JSON path; streamed cells and transforms behave the same as v1.
- Selection, picking, and bounds use the union of all part bounds in prefab space.

### Validation

- Each `mesh` block must contain either `primitive` or `asset`, not both.
- Primitive names must be one of the supported built-ins.
- `color` is optional RGB in linear space; defaults follow the stylized terrain/prop palette when omitted.
- Entity transforms must remain finite; zero or negative scale on any axis fails validation.

## Sample assets

- `assets/prefabs/tree.prefab.json` — v2 compositional tree (cylinder trunk, sphere canopy, branch cylinders)
- `assets/prefabs/campfire.prefab.json` — v2 compositional campfire (stone cubes, log cylinders, flame pyramid) plus warm point light

## Implementation status

| Capability | Status |
|------------|--------|
| v1 single external mesh | active |
| v1 optional prefab point light | active |
| v2 multi-part primitives | active |
| Prefab editor part manipulation | active (inspector fields + save; viewport preview at origin) |
| Prefab-authored collision volumes | active (box/sphere, layer, trigger, local transform; cell-owned spawn/unload) |
| Bake compositional prefab to glTF | deferred |
