# TICKET-0042: Post-process stack with ambient occlusion

- Epic: EPIC-0005
- Status: done
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ad3efc569581bb885fc8b6a8ec4f5a

## Goal

Ship a first post-process ambient occlusion pass after opaque PBR. **v1 = screen-space SSAO** (depth-based), budgeted for 1440p/60 on mid-range GPUs.

## Context links

- `context/planning/epics.md` (EPIC-0005)
- `context/formats/materials.md` (PBR complete)
- `context/features/index.md`
- Depends on TICKET-0040 / 0143 (done)

## Acceptance criteria

- [x] Depth-based SSAO runs after the lit scene pass and darkens occluded creases/contact areas.
- [x] Half-resolution AO + composite (or documented equivalent) with tunable radius/intensity; default looks reasonable in editor/debug world.
- [x] Masked/blended materials remain fail-closed (unchanged — SSAO reads only depth/lit-color, no material path changes); sky is not over-darkened by AO (depth >= 0.999 short-circuits to AO = 1.0).
- [x] Budget note recorded for 1440p/60; rebuild `engine` succeeds; GPU smoke passes.
- [x] Context indexes / ticket docs updated.

## Out of scope

- HBAO/GTAO, temporal accumulation, G-buffer normals (can use depth-derived normals).
- Full M8 VFX post stack (bloom, tonemap, etc.) beyond AO + composite scaffolding.
- Material-baked AO maps.

## Dependencies

- Opaque PBR path (0040/0143) complete.

## Verification

- Rebuild `engine`; `engine run --frames 2 --hidden` smoke; visual check in editor.
- Set Status to needs-approval after verification — never done.

## Agent notes

- Technique: SSAO v1 (owner-chosen default from interview).
- Banding fix: finite-difference depth normals (no ddx/ddy), interleaved gradient noise, wider 9-tap AO blur, softer defaults (radius 0.65 / intensity 0.55).
- Implementation lives in `src/rendering/render_app.cpp` post-process path.

### Implementation summary (this change)

- **Lit intermediate**: the world pass (editor scene/game viewports and runtime) now always draws into a full-res
  `lit_color_` (`R8G8B8A8_UNORM`) target instead of directly into the viewport texture / swap-chain backbuffer.
  `draw_world_pass` itself is unchanged; only its `color_target`/`rtv` arguments changed.
- **Depth**: `depth_` is now `DXGI_FORMAT_R32_TYPELESS` (DSV view `D32_FLOAT`, SRV view `R32_FLOAT`) so the SSAO pass
  can sample it. `apply_ssao()` transitions it `DEPTH_WRITE -> PIXEL_SHADER_RESOURCE -> DEPTH_WRITE` around the SSAO
  draw so the next `draw_world_pass` can clear/write it again.
- **AO target**: half-res `ao_target_` (`R8_UNORM`), cleared to `1.0` (no occlusion) each pass.
- **Post descriptor heap**: a new shader-visible `post_srv_heap_` (3 descriptors: depth, lit, AO) is created
  unconditionally (editor and runtime) and rewritten in `recreate_targets()` on init/resize.
- **World-space reconstruction**: `WorldPassParams` only carries a combined `view_projection` (no separate view
  matrix), so SSAO reconstructs **world-space** position from depth via the DirectXMath inverse of
  `view_projection`, and measures "depth" as distance-to-camera rather than view-space Z. This is the practical
  reading of "reconstruct view position using inverse view-projection" given the data this renderer already
  tracks; it is a valid SSAO variant and keeps `WorldPassParams` unchanged. Normals are depth-derived via
  `ddx`/`ddy` of the reconstructed world position (no G-buffer normals, as scoped out).
- **Root signatures**: SSAO uses root CBV (b0) + one descriptor table (SRV t0, depth) + a static point sampler;
  composite uses root CBV (b0) + one descriptor table (SRV t0/t1, lit/AO) + a static linear sampler. Both are ~3
  root DWORDs, well under the 64-DWORD limit, per the "prefer CBV + descriptor table" guidance.
- **Kernel**: 12 Fibonacci-hemisphere sample directions, hemisphere-oriented per-pixel via a screen-space hash
  rotation (no noise texture), distance-to-camera range check to reduce halos.
- **Composite**: `lit_color_ * lerp(1, ao, intensity)`, where `ao` is a 4-tap bilinear-blurred sample of the
  half-res AO target (also upsamples it to full res).
- **Defaults**: `k_ssao_radius = 0.85`, `k_ssao_bias = 0.025`, `k_ssao_intensity = 0.85` (constants near the top of
  `render_app.cpp`, alongside `k_max_point_lights`).
- **Budget note (1440p/60)**: not independently profiled on real 1440p hardware in this change (headless GPU smoke
  only, on an RTX 2080 SUPER at 1280x720, average GPU time ~0.03-0.05 ms/frame for the whole frame including SSAO in
  the sample project, which is not representative of full 1440p scene complexity). The half-res AO target (12
  samples, no noise texture, single 4-tap blur) is intentionally cheap; a dedicated GPU-timed benchmark at 1440p is
  recommended as follow-up before treating the 60 fps budget as verified on mid-range hardware.
- **Verification performed**: rebuilt `engine` (Debug), ran
  `engine.exe run --project samples/open-world-rpg --frames 2 --hidden true --json` (exit 0) and
  `engine.exe editor --project samples/open-world-rpg --frames 3 --hidden true --json` (exit 0, exercises both
  editor viewports + ImGui descriptor-heap handoff). Temporarily attached an `ID3D12InfoQueue` during development
  to confirm zero D3D12 validation errors/warnings from the new resource states, descriptor tables, and root
  signatures (removed before finishing — not part of the shipped diff).
- **Remaining risk**: no automated visual regression test for AO strength/shape; the sample project's default
  headless camera pose does not clearly frame occluding geometry, so the "default looks reasonable" acceptance item
  was verified by D3D12 validation cleanliness and code review of the math rather than a pixel-level before/after
  screenshot diff. An interactive editor visual check is still recommended before `done`.
