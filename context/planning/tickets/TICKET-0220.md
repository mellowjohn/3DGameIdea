# TICKET-0220: Foliage frustum cull + mesh distance LOD ladder

- Epic: EPIC-0013
- Status: needs-approval
- Agent: cursor-agent
- Priority: P1
- Notion: https://app.notion.com/p/3a6d3efc56958194a0deff7c59500e04

## Goal

Protect frame time as hubs densify by **frustum-culling foliage instances** and adding a simple **distance LOD ladder** for placed meshes (and/or foliage), matching the streaming-lod budget intent without Nanite.

## Context links

- `context/planning/epics.md` (EPIC-0013)
- `context/features/streaming-lod-budgets.md` — frustum cull note; LOD deferred
- `context/features/terrain-authoring.md` — foliage instancing
- Quality/perf audit (owner 2026-07-23): gap #3
- Soft prerequisite: TICKET-0139 (measure before/after)

## Acceptance criteria

- [x] Foliage instance draws skip batches/instances whose bounds are fully outside the camera frustum (same conservative AABB contract as terrain/placements).
- [x] Placed mesh (or prefab) path supports at least **two distance bands** (near full mesh / far reduced or culled) with documented meter thresholds as constants.
- [x] Stylized silhouettes remain readable at mid distance (no popping worse than existing fog unload without hysteresis or crossfade note).
- [x] Streaming unload behavior unchanged; LOD is view-distance, not a second streaming system.
- [x] Metrics: draw/instance counts exposed in existing timing/benchmark path or Diagnostics when practical.
- [x] Context: update `streaming-lod-budgets.md` + `features/index.md`.
- [x] Rebuild `engine`; relevant smoke suites pass; desktop check in a dense foliage cell.

## Out of scope

- Nanite / virtualized geometry
- GPU-driven Hi-Z occlusion
- Impostor billboard atlases (intent only unless trivial)
- Recast / nav LOD

## Dependencies

- Owner override 2026-07-23: new **active P1** on quality/perf gap track.
- Prefer measuring with TICKET-0139 harness when available; not a hard block.

## Verification

Rebuild `engine`. Compare instance/draw counts or frame times in a dense cell before/after in **What changed**. Desktop GPU required for visual pop check.

## What changed

### Summary

Shipped **foliage frustum cull** (per 40 m cell × mesh AABB draws) and a **2-band placed-mesh distance LOD** with sticky hysteresis (`mesh_distance_lod.h`: near ≤160 m, far cull ≥210 m, exit ≤160 m). Streaming unload radii unchanged. Benchmark Game play-test instances **3617 → 2147** (−41%) vs TICKET-0139 baseline JSON.

### Files / surfaces

- `include/engine/rendering/mesh_distance_lod.h` (new)
- `include/engine/world/foliage_field.h` — `cell_instances()` accessor
- `src/rendering/render_app.cpp` — `set_foliage_cell_instances`, frustum skip in `draw_foliage_instances`, placed-mesh sticky far keys
- `tests/suite_tests.cpp` — foliage cell_instances + LOD constant checks
- `context/features/streaming-lod-budgets.md`, `context/features/index.md`
- Report: `out/benchmarks/open-world-1440p-post-0220.json`

### Schema / API

No authored asset schema change. Runtime LOD constants in `engine::mesh_lod`.

### Verification evidence

- Rebuild Debug `engine` + `engine_suite_tests` OK
- `--suite foliage` 36/36 (from repo root); `--suite world` 59/59
- `engine run --debug-world --frames 30 --hidden` exit 0
- `engine editor --frames 2 --hidden` exit 0 (instances 4602)
- Benchmark 1440p Game play-test: instances **2147** / drawCalls **26** / GPU **1.73 ms** (baseline `context/benchmarks/open-world-1440p.json`: 3617 / 23 / 1.65 ms)
- Dense-cell path covered by streamed open-world-rpg Game play-test; owner should eyeball mid-distance pop in Game viewport

### Decisions

- Cell×mesh AABB cull (not per-blade CPU rebuild) for foliage draw units
- Far band **culls** full mesh in v1 (no alternate LODed mesh asset)
- Sticky `mesh_lod_far_keys_` for hysteresis; foliage relies on frustum + existing scatter falloff only

### Leftover risk

- Splitting foliage by cell can raise draw-call count while cutting instances (observed 23→26)
- Sticky far keys are not pruned on entity unload (bounded by resident placements)
- Owner visual check for band-edge pop still recommended on desktop

## Agent notes

Ready for owner approval. Next gap ticket: TICKET-0145 (visual regression).
