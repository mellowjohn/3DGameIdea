# Terrain Edit Data

Persisted height deltas for procedural terrain cells. Stored at `assets/terrain/terrain-edits.json`.

## Schema v1

```json
{
  "schemaVersion": 1,
  "resolution": 33,
  "cellSize": 40.0,
  "cells": [
    {
      "x": 0,
      "z": 0,
      "deltas": [0.0, 0.12, 0.0]
    }
  ]
}
```

- `resolution`: height sample lattice width/height. Must be `33` (matches streamed terrain cells).
- `cellSize`: terrain cell size in meters. Must be `40.0`.
- `cells`: edited terrain cells. Each entry stores a flat `deltas` array with `resolution * resolution` floats.
- `deltas`: per-sample height offsets in meters added on top of procedural terrain. Values must be finite and within `[-32, 32]`.

## Runtime merge

`generate_stylized_terrain()` adds cell deltas to procedural heights before building triangles and Jolt heightfields. `sample_terrain_height()` bilinearly interpolates active edits for placement and navigation queries.

## Editor workflow

1. Open the **Sculpt** viewport tab (between Scene and Game).
2. Adjust brush radius and strength in the sculpt toolbar above the viewport.
3. Left-drag on terrain to raise; hold **Shift** while dragging to lower.
4. Use toolbar undo/redo or **Ctrl+Z** / **Ctrl+Y** while the Sculpt tab is active.
5. **Ctrl+S** saves scene JSON and `assets/terrain/terrain-edits.json`.

Loaded cells refresh render and collision immediately after each stroke.

## Validation

`engine validate --project <project>` loads `assets/terrain/terrain-edits.json` when present and rejects malformed schema, mismatched sample counts, and out-of-range deltas.

## MCP helpers

`engine_terrain_apply` height tools beyond raise/lower/flatten:

| Action | Purpose |
| --- | --- |
| `set_height` | One-stroke blend toward `targetHeight` (`strength` 0–1, falloff). Prefer this over repeated flatten. |
| `carve_channel` | Polyline river bed + banks: `points`, `halfWidth`, `bedDepth` or `bedHeight`, `bankOffset`, `bankWidth`, `bankHeight` or `bankClearance`, `step`, `strength` |
| `raise_banks` | Same polyline bank raise without digging the bed |

`carve_channel` clamps `bankOffset` to at least `halfWidth + bankWidth`, paints banks first, then the bed so the channel floor wins any soft-falloff overlap.

Sea level defaults from the bound water store (or `seaLevel` override). Height edits reload water meshes when connected.

## Limitations

- Spherical brush with quadratic falloff only; no smoothing or erase-to-procedural brush yet.
- Edits apply to 40 m terrain streaming cells, not 128 m world-partition scene cells.
- Imported heightmap assets remain future work.
