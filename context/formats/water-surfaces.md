# Water Surfaces Format

Persisted authored water for Sculpt and MCP (`assets/terrain/water-surfaces.json`). Implements [DEC-0038](../decisions/index.md#dec-0039-water-swim-and-hydrology-authoring).

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

- `sample_water_surface_y(x,z)` — returns `seaLevel` when authored fill ≥ threshold or inside `seaRegions`, and terrain is at least ~12 cm below `seaLevel` (no mid-air sheet over dry ground)
- Rendered meshes clip wet cells to the terrain waterline (marching-squares shorelines) plus skirts down to the bed so basins follow contours instead of square pads
- Mesh vertices carry vertical column depth (`seaLevel - terrain`); the water pass uses Beer–Lambert absorption so deeper water is more opaque and hides the bed
- `is_underwater`, `water_depth`, `is_deep_water` — swim/navigation/foliage hooks

## Tools

- Sculpt **Water** tool: place / Shift+erase brush, undo/redo, Ctrl+S save
- MCP `engine_water_apply`: `place`, `erase`, `place_along`, `sample`, `undo`, `redo`, `save`, `batch`
  - `place_along`: `{points:[{x,z},...], radius?, strength?, step?, save?}` paints fill along a polyline

World Forge `hydrologyRegions` / `ferryRoutes` on `map.worldforge.json` plan geography; meshes stay in Sculpt.
