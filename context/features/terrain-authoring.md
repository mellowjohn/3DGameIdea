# Terrain Authoring

Editor and runtime support for saved terrain height edits on top of procedural low-poly heightfields.

## Current slice

- Versioned `assets/terrain/terrain-edits.json` persistence ([format](../formats/terrain-edits.md)).
- `TerrainEditStore` merges deltas into mesh generation, collision heightfields, and `sample_terrain_height()`.
- Editor **Sculpt** viewport tab with raise/lower brush, **Flatten** tool, undo/redo, and save through **Ctrl+S**.
- MCP `engine_terrain_apply` for agent-driven raise/lower/flatten/paint/paint_foliage/sample/undo/redo/save (live editor required for mutate).
- **Paint** mode on the Sculpt tab assigns `.material.json` assets to terrain samples with brush radius, undo/redo, and save to `assets/terrain/terrain-paint.json`.
- Material Inspector (via **Edit Tint** or **Edit** on the paint brush) can switch materials, create new ones with **+ New**, and assign **Use as Paint Brush** or **Use as Global Tint**.
- Streamed cells reload render and Jolt collision after height edits; paint reloads render meshes only.
- **Foliage** mode on the Sculpt tab paints ground-cover density (grass, flowers, bushes) into `assets/terrain/foliage-density.json` using the layer palette at `assets/foliage/ground-cover.layers.json`.
- Foliage brush strength is independent from terrain sculpt/paint strength. **Sparse / Medium / Dense** presets map to common density strengths.
- Foliage brush modes: **Paint** (single layer), **Erase** (removes all layers), **Mixed** (meadow blend of grass, flowers, and bushes). Shift+drag also erases while painting or mixing.
- GPU-instanced foliage scatters deterministically per loaded cell, reloads on brush edits, and streams with terrain. Grass renders as faceted `grass_blade` instances; bushes use `discrete` scatter (one bush per strongly painted sample).
- During **play test**, character movement feeds `WorldInfluenceBus` and blades bend away in the vertex shader (no collision blocking). Fly camera has no bend.

## API

- `TerrainEditStore::apply_brush(world_x, world_z, radius, strength, lower)`
- `StreamedTerrainField::reload_cells(...)` for loaded-cell refresh
- `TerrainEditHistory` + `TerrainBrushStrokeCommand` for undoable strokes
- `FoliageDensityStore::apply_foliage_brush(...)` and `FoliageDensityHistory` for foliage strokes
- `StreamedFoliageField::sync(...)` / `rebuild_cells(...)` for instanced ground cover

## Deferred

- Surface region painting via material-index brushes, flatten/smooth brushes, imported heightmaps, discrete bush/stone/tree placement brushes, and Recast carving from edited meshes.
