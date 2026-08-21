# GPU instance cull (foliage ExecuteIndirect)

Status: **temporarily disabled** (2026-08-06) — ring/clamp/plane-normalize land in tree, but interactive editor AVs ~40s with the cull pass created (and RelWithDebInfo `editor` fails fast). CPU `DrawInstanced` until editor soak is green.

First **GPU-driven** draw path in the engine: compute frustum cull of foliage instances, compact visible indices, then `ExecuteIndirect` DrawInstanced.

## Hang class (2026-08-06) → fix

Observed: RelWithDebInfo Scene free-cam could stall once GPU cull was live past ~100 frames. Mitigations in `gpu_instance_cull.cpp` / upload path:

1. Bind shader-visible UAV heap **before** `ClearUnorderedAccessViewUint`.
2. UAV barrier after ClearUAV before `Dispatch`.
3. **2-slot ring** for visible indices / count / indirect args / upload CBs keyed to swapchain `frame_index`.
4. **`wait_for_gpu()` before growing** cull buffers (recreate while in-flight was the AV class).
5. CS **clamps** compacted `visibleCount` to capacity so a stale clear cannot inflate `InstanceCount`.
6. Frustum planes from `frustum_from_view_projection` are **unit-normalized** so sphere tests compare against world-meter radii (unnormalized planes blanked all grass while AABB cell cull still passed).

Note: hidden `engine editor --frames N` can still AV/hang on teardown with cull **off** (pre-existing); soak via `engine run --frames 180+ --hidden --debug-world` instead.

## Behavior (when re-enabled)

- **When:** Scene / menu (and any world pass that passes a camera `Frustum*` into `draw_foliage_instances`).
- **When not:** Play-test foliage path that passes `frustum == nullptr` (historical uncullable blades) stays on CPU `DrawInstanced`.
- **Cull shape:** Sphere at instance origin (from model matrix translation) with radius from blade height / bend radius — conservative.
- **Fail-closed:** If the cull pass is not ready or `dispatch_cull` returns false, that batch uses `DrawInstanced`.

## Root-argument invariant (2026-08-06)

Foliage root parameter **5** (`t2`, compacted visible indices) must be bound on **every** foliage draw, not only the compacted `ExecuteIndirect` branch. The foliage vertex shader can reach `t2` regardless of `layerBladeTime.w`, and an unbound root argument makes the driver dereference a garbage descriptor: the GPU hangs (`DXGI_ERROR_DEVICE_HUNG`) instead of reading zeros.

With the cull draw disabled this hung the editor roughly 36 s after load on `worlds/main-menu.world.json` and dragged frame rate to ~8 FPS beforehand. `bind_foliage_graphics` now binds parameter 5 to the foliage instance buffer as a safe default, and the compacted branch overrides it. See the 2026-08-06 entry in [`../testing/findings.md`](../testing/findings.md).

Diagnosis path worth reusing: `ENGINE_GPU_VALIDATION=1` plus `--debug-layer` turns on D3D12 GPU-based validation, which named the parameter directly. `ENGINE_GPU_TRACE=1` logs video-memory and deferred-release counters every 64 presents. DRED breadcrumbs are always captured and now print a run-length-encoded history with `BeginEvent` pass markers, so a hung op maps to a pass.

## Pipeline

1. CS (`gpu_instance_cull.cpp`) tests each instance against six frustum planes.
2. Visible absolute instance indices compacted via `InterlockedAdd`.
3. `visibleCount` copied into `D3D12_DRAW_ARGUMENTS.InstanceCount`.
4. Foliage VS reads `visibleIndices[SV_InstanceID]` when `layerBladeTime.w > 0.5`.

## Related

- DEC-0047 follow-on (GPU-driven cull)
- TICKET-0220 CPU cell×mesh frustum + placed far-cull
- [`csm-shadows.md`](csm-shadows.md) practice note on ExecuteIndirect
- [`streaming-lod-budgets.md`](streaming-lod-budgets.md)
- [`../benchmarks/open-world-1440p.md`](../benchmarks/open-world-1440p.md) — RelWithDebInfo capture with cull disabled
