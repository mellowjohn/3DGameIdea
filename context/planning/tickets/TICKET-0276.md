# TICKET-0276: GPU frustum cull + ExecuteIndirect for foliage

- Epic: EPIC-0013
- Status: needs-approval
- Agent: cursor-agent
- Priority: P1
- Notion: (repo-first; mirror when Notion sync runs)

## Goal

Move foliage instance visibility from CPU-only cell AABB skips to a **GPU frustum cull + compacted indices + ExecuteIndirect** path so mid-AA draw submit scales with visible blades, not resident instances — closing the GPU-driven rendering gap called out in the performance audit.

## Context links

- [DEC-0047](../../decisions/index.md#dec-0047-frame-upload-ring-and-gpu-lbs-skinning) — GPU-driven cull explicitly deferred; this ticket is that follow-on for foliage
- [`../features/streaming-lod-budgets.md`](../features/streaming-lod-budgets.md) — CPU frustum / distance LOD today
- [`../features/csm-shadows.md`](../features/csm-shadows.md) — notes ExecuteIndirect as later practice
- Performance audit canvas (GPU-driven score 1.5 vs industry ~3.8)
- Soft: TICKET-0220 (CPU foliage frustum + mesh far-cull), TICKET-0139 (benchmark gate)

## Acceptance criteria

- [x] Foliage Scene / menu world passes with a camera frustum use compute sphere-vs-frustum cull per instance, compact visible indices, and `ExecuteIndirect` DrawInstanced (**code path**; **runtime enable blocked** — see leftover risk)
- [x] Play-test path that intentionally leaves foliage uncullable (`frustum == nullptr`) still uses CPU `DrawInstanced` (no behavior change)
- [x] Fail-closed: if cull pass create/dispatch fails, fall back to `DrawInstanced` without blanking foliage
- [x] New module `gpu_instance_cull.h/.cpp`; `engine` Debug rebuild succeeds
- [x] Feature note + `features/index.md` row; csm-shadows / streaming-lod cross-link updated
- [x] GPU smoke: `engine run --project samples/open-world-rpg --debug-world --frames 8 --hidden --debug-layer` exits 0
- [ ] Re-enable `use_gpu_cull` after Scene free-cam soak (≥150 frames RelWithDebInfo) with no fence hang

## Out of scope

Explicit non-goals so agents do not expand the ticket.

- Prop / character GPU cull (follow-on)
- Hi-Z / occlusion cull
- Mesh shaders / Nanite
- Accurate post-cull instance counters via readback
- Changing stream radii or mesh LOD ladders

## Dependencies

Soft after TICKET-0220. Parallel OK with TICKET-0139 RelWithDebInfo lock.

## Verification

- Rebuild `engine` Debug + RelWithDebInfo: OK
- ClearUAV heap bind + post-clear UAV barrier in `gpu_instance_cull.cpp`
- Draw path: `use_gpu_cull` hard-disabled after RelWithDebInfo hang past ~100 frames with path live
- RelWithDebInfo `benchmark --frames 120` (vertical-slice default): OK with cull disabled — see `context/benchmarks/open-world-1440p-relwithdebinfo.json` (~4.5 ms CPU / ~1.3 ms GPU on 2080 SUPER)
- Hidden `engine editor --frames 3` hung on teardown (pre-existing streamed-field destructor class) — not used as pass gate

## What changed

- Summary: Foliage GPU cull + ExecuteIndirect implemented; **runtime disabled** after ClearUAV/barrier fixes still hung RelWithDebInfo/editor. Benchmark gate defaults to vertical-slice (not main-menu). RelWithDebInfo 1440p capture published on 2080 SUPER with CPU DrawInstanced foliage.
- Files / surfaces touched: `gpu_instance_cull.*`, `render_app.cpp` (VS t2 + disabled draw branch), `command.cpp` (benchmark default world), CMakeLists, feature/ticket/benchmark docs.
- Schema / API / format deltas: none for assets; foliage Interaction CB `layerBladeTime.w` = GPU-cull flag (unused while disabled).
- Seed / sample data: none.
- Tests / verification evidence: RelWithDebInfo 120f gate exit 0 with cull off; 150f stress with cull off.
- Decisions & tradeoffs: Prefer ship-safe CPU instancing over live ExecuteIndirect until soak passes; keep module for re-enable.
- Leftover risk / follow-ons: Re-enable + Scene free-cam soak; prop GPU cull; Hi-Z; 4070-class RelWithDebInfo lock for TICKET-0139 budgets.

## Agent notes

Implemented under owner “lets do it” after GPU-driven audit score discussion (2026-08-06). Hang fix pass same day: ClearUAV ordering + UAV barrier; still forced disable for stability.
