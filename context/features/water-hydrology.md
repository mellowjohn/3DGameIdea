# Water and Hydrology

Status: planned ([DEC-0038](../decisions/index.md#dec-0038-water-swim-and-hydrology-authoring))  
Epic: EPIC-0015

Gameplay water for rivers, ponds, lakes, bounded seas, swim traversal, and scripted floating vessels. v1 is **fully authored** — no noise-driven auto hydrology.

## Product intent

| Concern | v1 decision |
| --- | --- |
| Gameplay | Swim mode; deep water drains fatigue and causes damage over time; ships/ferries float via scripted motion |
| Sea level | One world-wide `sea_level` Y; terrain sculpt adjusts land vs ocean |
| Dry basins | Stay dry unless terrain + authored water placement justify fill |
| Open sea | Bounded authored sea regions; map-edge fog-of-war hides beyond bounds |
| Art | Low-poly stylized water with reflection, refraction, and scripted wave motion |
| Shores | Transition to mud/sand materials; shore foam/waves when feasible |
| Foliage | Suppressed underwater |
| Future | Lava, magic pools — same material/system extension, not v1 |

## Authoring split

```text
World Forge Map          Sculpt (+ MCP)              Runtime
─────────────────        ─────────────────           ─────────
River/lake/sea regions   Water surface placement     Streamed water meshes
Ferry route polylines    Shore carve + fill level    Wave shader/sim
POI links (docks)        Undo/save water store       Swim + deep-water rules
Planning overlays        Sample / batch via MCP        Nav + foliage hooks
```

World Forge stores **IDs, regions, routes, and POI links** — not meshes ([DEC-0019](../decisions/index.md#dec-0019-world-forge-editor-home-and-story-canon-split)). Sculpt owns persisted water surfaces and fill, mirroring `TerrainEditStore` / `TerrainPaintStore` patterns ([DEC-0018](../decisions/index.md#dec-0018-mcp-terrain-sculpt-and-paint-apply)).

## Runtime (planned)

### Water query API (target)

- `sample_water_surface_y(x, z)` — surface height if water present, elsenullopt
- `is_underwater(x, z, entity_y)` — for foliage, VFX, audio
- `water_depth(x, z)` — distance from surface to terrain bed (or sea floor)
- `is_deep_water(x, z)` — exceeds shallow band → fatigue/damage swim rules

Central hook: same world XZ queries used by `sample_terrain_height`, navigation grid, and foliage scatter.

### Swim mode

Character controller gains a **swim locomotion mode** when the capsule is in water:

- Surface swim and submerged swim (implementation detail)
- Shallow water: wade or low-cost swim (depth band TBD)
- Deep water: sustained swim drains **fatigue**; at exhaustion, **health damage over time**
- Transitions: walk ↔ swim at water surface; no silent walking on lake beds without surfacing logic

See [`character-controller.md`](character-controller.md) (planned section).

### Vessels

- Ships and ferries use **scripted transform paths** (Lua/handlers), not full buoyancy simulation in v1
- Hulls **snap/float to water surface** at authored attachment points so motion reads believable
- SQ-10 (Island Ferry) is the reference beat: pier, optional dive, scripted crossing to islet

### Navigation

Deep water aligns with [`open-world-navigation.md`](open-world-navigation.md) hard barriers:

- Grid samples below water surface or in `is_deep_water` → unwalkable for AI/assist queries
- Player uses swim + fatigue rules instead of grid pathing

### Rendering

Prerequisites:

1. **Blended material pass** — today masked/blended materials fail closed ([`materials.md`](../formats/materials.md))
2. **Water shader** — reflection + refraction, low-poly facet-friendly, **scripted wave displacement** (deterministic)
3. **Shore blend** — mud/sand material band at shoreline; optional foam pass

Water streams with terrain cells (40 m grid) or as bounded region meshes tied to authored data.

### Foliage

`StreamedFoliageField` skips scatter where `is_underwater` at sample height ([DEC-0012](../decisions/index.md#dec-0012-ground-cover-first-foliage-authoring)).

## Materials (target)

`assets/materials/water.material.json` (sample):

- `opacityMode: "blended"`, low roughness, blue-green `baseColor`
- `physics.surface: "water"` for footsteps/splash hooks later
- Shore variants: `mud_shore`, `sand_shore` for paint brush or auto shore band

## MCP (target)

Extend terrain MCP or add `engine_water_apply` with the same live-editor bridge pattern:

- `place`, `erase`, `set_fill_level`, `sample`, `undo`, `redo`, `save`, `batch`
- Read-only `sample` offline when editor closed

## Dependencies

| Blocker | Owner |
| --- | --- |
| Blended / water render pass | EPIC-0005 / EPIC-0015 |
| Swim mode + stamina/damage | EPIC-0015 + character controller |
| Water persistence format | EPIC-0015 |
| Sculpt Water tool UI | EPIC-0015 |
| World Forge hydrology + ferry routes | EPIC-0002 / EPIC-0015 |
| Fatigue/stamina HUD | EPIC-0007 (if not already present) |

## Out of v1 scope

- Perlin/noise river or lake generation
- Full rigid-body boat physics
- Lava / magic pool liquid types (future)
- Infinite ocean plane for entire map
- Underwater combat polish beyond basic swim

## Open implementation tuning

See [`../interviews/open-questions.md`](../interviews/open-questions.md#water-and-hydrology-dec-0038).
