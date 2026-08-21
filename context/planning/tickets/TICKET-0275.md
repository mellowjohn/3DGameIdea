# TICKET-0275: Continent-scale world + stream budget (DEC-0054)

- Epic: EPIC-0004
- Status: needs-approval
- Agent: cursor-agent
- Priority: P0
- Notion: (not mirrored — no Notion MCP in session)

## Goal

Lock and implement a seamless playable world that encapsulates the official Tessera continent map, with a raised streaming/view-distance budget and a stress test that proves resident sets stay bounded.

## Context links

- [DEC-0054](../../decisions/index.md#dec-0054-continent-scale-seamless-world--stream-budget)
- [`official-world-map.md`](../../story/official-world-map.md)
- [`streaming-lod-budgets.md`](../../features/streaming-lod-budgets.md)
- Related: TICKET-0032, TICKET-0139, TICKET-0274

## Acceptance criteria

- [x] Decision recorded: 16×16 km world; cartography plate 16000×10667 (map aspect); play stream radius 4 / support 2
- [x] Partition default half-extent 8000; sample worlds + map plate updated
- [x] Mesh LOD + foliage falloff + camera far plane + CSM outer cascade raised for vistas
- [x] Terrain stress walks ~256 km² and asserts bounded residency for the new radius
- [x] `terrain` suite green after rebuild (685/685); `world_forge` 279/279
- [x] Context docs updated for continent-scale wording

## Out of scope

- Relocating Act 0 graybox content across the full plate (LD / D-P2-08)
- Impostor meshes / whole-continent residency
- Changing soft-gate / FT design

## Dependencies

Soft: TICKET-0032 acceptance docs; TICKET-0139 budgets may need retune after capture.

## Verification

- Rebuild `engine` Debug: succeeded (warnings only: C4996 getenv/sscanf, C4456 shadow, C4100)
- `engine_suite_tests --suite terrain`: **685/685**
- `engine_suite_tests --suite world_forge`: **279/279** (16 km cartographyPlate assert)
- `engine_tests` (foundation): crashed after unrelated asset-database FAILs (0xC0000005) — not attributed to partition extent change; partition out-of-bounds assert updated to 8000.01
- Editor relaunched on `main-menu.world.json`; build lease released

## What changed

### Summary

Scaled the playable world from 4×4 km to **16×16 km** so the official continent map is the playable window; raised play stream to radius **4** and matching view-distance LOD.

### Map measurement

- Master: 4096×2730 (aspect ~1.500)
- `local_calrenoth` ≈ 37% of frame → ~6 km Act 0 corridor ⇒ full plate ~16×10.7 km

### Files / surfaces

- `include/engine/world/world_partition.h`, `terrain_field.h`, `water_field.h`, `foliage_scatter.h`, `navigation_grid.h`, `scene.h`
- `include/engine/rendering/mesh_distance_lod.h`, `csm_shadows.h`
- `src/world/scene.cpp`, `world_partition.cpp`, `src/assets/camera_asset.cpp`, `world_forge_map_asset.cpp`, `src/rendering/render_app.cpp`
- Sample worlds + `map.worldforge.json`
- Tests + DEC-0054 + streaming/docs

## Agent notes

Play keeps view-bias + amortize; Scene/menu uses wider full-disc editor radius (6). Content still clustered near old Act 0 coords — LD must spread theaters across the plate later.
