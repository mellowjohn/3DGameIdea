# Water Surfaces Format

Persisted authored water for Sculpt and MCP (`assets/terrain/water-surfaces.json`). Implements [DEC-0038](../decisions/index.md#dec-0038-water-swim-and-hydrology-authoride).

## Root fields

| Field | Type | Notes |
| --- | --- | --- |
| `schemaVersion` | `1` | Required |
| `resolution` | int | Samples per cell edge (33, matches terrain) |
| `cellSize` | float | Meters (40.0, matches terrain) |
| `seaLevel` | float | World-wide Y for ocean/lake surfaces |
| `seaRegions` | array | Bounded auto-fill boxes `{id,minX,maxX,minZ,maxZ}` |
| `cells` | array | Painted fill masks `{x,z,fill[]}` — 0–255 per sample |

## Runtime queries

- `sample_water_surface_y(x,z)` — returns `seaLevel` when authored fill ≥ threshold or inside `seaRegions` with terrain below `seaLevel`
- `is_underwater`, `water_depth`, `is_deep_water` — swim/navigation/foliage hooks

## Tools

- Sculpt **Water** tool: place / Shift+erase brush, undo/redo, Ctrl+S save
- MCP `engine_water_apply`: `place`, `erase`, `sample`, `undo`, `redo`, `save`, `batch`

World Forge `hydrologyRegions` / `ferryRoutes` on `map.worldforge.json` plan geography; meshes stay in Sculpt.
