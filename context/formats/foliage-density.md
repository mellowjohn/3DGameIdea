# Foliage Density Data

Persisted per-sample ground-cover density for procedural terrain cells. Stored at `assets/terrain/foliage-density.json`.

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
      "density": [0, 0, 200],
      "layer": [0, 0, 0]
    }
  ]
}
```

- `resolution` / `cellSize`: must match terrain paint and sculpt grids (33 samples per 40 m cell).
- `density`: `resolution * resolution` unsigned bytes (0–255) per edited cell.
- `layer`: palette index into `assets/foliage/ground-cover.layers.json` for each sample.

Runtime scatters instances deterministically from non-zero density samples. See [foliage layers](foliage-layers.md) for palette fields.

## Editor workflow

1. Open the **Sculpt** viewport tab.
2. Select **Foliage** in the toolbar.
3. Choose **Paint** (single layer), **Erase** (remove all foliage), or **Mixed** (grass + flowers + bushes meadow blend).
4. Adjust strength with **Sparse / Medium / Dense** presets or the strength field.
5. Drag on terrain to paint; hold **Shift** while painting or mixing to erase.
6. Use foliage undo/redo or **Ctrl+Z** / **Ctrl+Y** while Foliage mode is active.
7. **Ctrl+S** saves scene JSON, terrain files, and `foliage-density.json`.
8. Live MCP: `engine_terrain_apply` with `action: paint_foliage` (`layer`: id or index, optional `erase`) or `paint_foliage_mixed`, including batched `ops[]`.

## Validation

`engine validate --project <project>` loads `assets/terrain/foliage-density.json` when present and rejects malformed schema, mismatched sample counts, and out-of-range layer indices.
