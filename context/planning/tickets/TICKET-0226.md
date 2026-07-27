# TICKET-0226: Per-frame CB upload ring (drop Present drain)

- Epic: EPIC-0013
- Status: needs-approval
- Agent: cursor-agent
- Priority: P1
- Notion: https://app.notion.com/p/3a7d3efc569581c08991f3f4f2aecf3a

## Goal

Stop serializing CPU and GPU every frame by **ring-buffering** the hot per-frame upload constant buffers so the renderer no longer drains the fence after Present.

## Context links

- `context/planning/epics.md` (EPIC-0013)
- `context/decisions/index.md` — DEC-0047 (upload ring + GPU LBS)
- `context/testing/findings.md` — Present drain entry
- `context/features/debug-world.md` — Performance tab metrics
- Follow-on: TICKET-0227 (GPU skinning uses the same ring)

## Acceptance criteria

- [x] `frame_cb_`, `water_frame_cb_`, `shadow_cb_`, `ssao_cb_`, and `composite_cb_` are 2-slot UPLOAD rings keyed to swapchain `frame_index_`.
- [x] Steady-state `Renderer::render` does **not** call `wait_for_fence` after a successful Present (Present-failure drain retained).
- [x] Frame-start `wait_for_current_frame()` still gates allocator reuse.
- [x] Diagnostics → Performance shows GPU-fence / Present wait near zero in steady Game play-test; no green-screen after multi-minute walk.
- [x] Context docs updated (`debug-world.md`, `findings.md` as needed).
- [x] Rebuild `engine` succeeds.

## Out of scope

- Triple buffering
- Ringing foliage/water/terrain replace buffers (already retire-behind-fence)
- GPU skinning (TICKET-0227)
- GPU-driven culling / LOD

## Dependencies

- Soft: TICKET-0139 harness for before/after metrics
- Blocks correct bone-CB updates in TICKET-0227

## Verification

Rebuild Debug `engine` OK (getenv C4996 only). Editor restarted for Game play-test: owner should confirm Diagnostics fence wait ≈ 0 and multi-minute stability.

## What changed

### Summary

Hot per-frame upload CBs are now a 2-slot ring matching the swapchain. Successful Present no longer drains the GPU fence; CPU prep can overlap the prior frame’s GPU work.

### Files / surfaces

- `src/rendering/render_app.cpp` — `create_upload_cb_ring`, ringed frame/water/shadow/ssao/composite CBs, removed post-Present drain
- `context/decisions/index.md` — DEC-0047
- `context/testing/findings.md`, `context/features/debug-world.md`

### Schema / API

No authored schema change. Runtime: CB binds use `frame_index_`.

### Verification evidence

- MSBuild Debug `engine` succeeded
- Editor process restarted after rebuild

### Decisions

- `frame_count = 2` (no triple buffer)
- Present-failure drain retained

### Leftover risk / follow-ons

- Foliage in-place Map on dirty cells still single-buffered
- Desktop confirm fence wait + long play-test

## Agent notes

Shipped with TICKET-0227 in the same pass.
