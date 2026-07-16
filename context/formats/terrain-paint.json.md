# Terrain Paint Data

Persisted per-sample material assignments for procedural terrain cells. Stored at `assets/terrain/terrain-paint.json`.

## Schema v1

```json
{
  "schemaVersion": 1,
  "resolution": 33,
  "cellSize": 40.0,
  "materials": [
    "assets/materials/stone.material.json"
  ],
  "cells": [
    {
      "x": 0,
      "z": 0,
      "indices": [0, 0, 1, 1]
    }
  ]
}
```

- `materials`: palette of `.material.json` paths. Index `0` in each cell means procedural surface color; `1` maps to `materials[0]`, and so on.
- `cells`: edited cells with `resolution * resolution` indices per entry.

## Editor workflow

1. Open the **Sculpt** viewport tab.
2. Select **Paint** in the toolbar.
3. Choose a **Paint Material** and drag on terrain to assign it.
4. Use paint undo/redo or **Ctrl+Z** / **Ctrl+Y** while Paint mode is active.
5. **Ctrl+S** saves scene JSON, `terrain-edits.json`, and `terrain-paint.json`.

The **Global Terrain Tint** material multiplies procedural terrain colors only. Painted samples keep their material colors when the tint changes.

## Validation

`engine validate --project <project>` loads `assets/terrain/terrain-paint.json` when present and rejects malformed schema, mismatched sample counts, and out-of-range palette indices.
