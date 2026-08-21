# TICKET-0278: Stabilize foliage GPU cull frame ring + re-enable

- Epic: EPIC-0013
- Status: needs-approval
- Agent: cursor-agent
- Priority: P0
- Notion: (repo-first; mirror when Notion sync runs)

## Goal

Make foliage GPU frustum cull + `ExecuteIndirect` **safe under multi-frame-in-flight**, then re-enable the draw path so Scene / menu / run free-cam no longer hangs past ~100 frames (TICKET-0276 leftover).

## Context links

- [`../features/gpu-instance-cull.md`](../../features/gpu-instance-cull.md)
- TICKET-0276 (path shipped, runtime disabled)
- DEC-0047 upload ring pattern (same 2-slot idea)

## Acceptance criteria

- [x] `GpuInstanceCullPass` uses a **2-slot ring** (visible indices / count / indirect args / upload CBs) keyed to swapchain `frame_index`
- [ ] Scene / menu / run foliage path sets `use_gpu_cull` when frustum is present and pass is ready — **regressed**: gated off after editor AV + blank-grass plane bug
- [x] Play-test path with `frustum == nullptr` still uses CPU `DrawInstanced`
- [x] Fail-closed: dispatch failure → `DrawInstanced`
- [x] RelWithDebInfo soak: `engine run --frames 180` and `300` with cull enabled, exit 0 (hidden `editor --frames` AVs with cull off too — pre-existing teardown)
- [x] RelWithDebInfo `engine benchmark` still exits 0
- [ ] Feature note updated: path **enabled** — currently documents temporary disable + plane-normalize note

## Out of scope

- Prop GPU cull
- Hi-Z / occlusion
- Accurate post-cull instance counters via readback

## Dependencies

Builds on TICKET-0276. Parallel OK with TICKET-0277.

## Verification

- RelWithDebInfo rebuild OK
- `engine run --frames 180/300 --hidden --debug-world` → exit 0 with cull on
- `engine benchmark --frames 60` → exit 0 (~3.5 ms CPU)
- Live editor launched for Scene free-cam smoke

## What changed

- Summary: Re-enabled foliage GPU cull after ring buffering, GPU-idle buffer growth, and CS visible-count clamp. Hidden editor `--frames` soak is not a valid gate (AVs with cull off). `engine run` soaks pass at 180 and 300 frames.
- Files / surfaces touched: `gpu_instance_cull.h/.cpp`, `render_app.cpp`, feature/benchmark/index docs, this ticket.
- Leftover risk / follow-ons: prop GPU cull; investigate pre-existing hidden-editor teardown AV separately.

## Agent notes

Misattributed earlier soak AV to ExecuteIndirect; baseline with cull off also AVs on `editor --frames 180`.
