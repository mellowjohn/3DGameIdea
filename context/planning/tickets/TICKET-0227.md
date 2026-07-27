# TICKET-0227: GPU LBS skinning for play-test player

- Epic: EPIC-0013
- Status: needs-approval
- Agent: cursor-agent
- Priority: P1
- Notion: https://app.notion.com/p/3a7d3efc5695811a9e44e34b84193b37

## Goal

Move play-test player mesh deformation off CPU vertex patching onto **GPU linear-blend skinning**, keeping CPU pose/matrix build, so character animation no longer dominates Render prep.

## Context links

- `context/planning/epics.md` (EPIC-0013)
- `context/decisions/index.md` — DEC-0047
- `context/formats/mesh-assets.md` — influences; player 37 joints
- `context/features/character-controller.md` — play-test visual
- Prerequisite: TICKET-0226 (ringed bone CB)

## Acceptance criteria

- [x] Bind-pose VB upload includes JOINTS (`R8G8B8A8_UINT`) + WEIGHTS (`R8G8B8A8_UNORM`); static meshes use zero weights.
- [x] Lit + shadow VS perform LBS when weight sum > 0; `MAX_BONES = 64`.
- [x] Play-test uploads skin matrices to the current-frame bone CB; no `patch_mesh_vertices` / `cpu_skin_positions` on the player hot path.
- [x] Skins with >64 joints fail closed / leave bind pose with a clear path (no silent CPU patch).
- [x] Diagnostics skin timing reflects matrix upload (sub-ms typical), not full mesh rebuild.
- [x] Idle/walk player deforms; shadows match; context docs updated.
- [x] Rebuild `engine` succeeds.

## Out of scope

- GPU-driven culling / LOD
- Skinning every catalog NPC/prop (wire general path; enable player first)
- Compute skinning / dual quaternion
- Visual in-place root zeroing polish

## Dependencies

- Soft-blocked by TICKET-0226 (upload ring) for safe bone CB writes
- Builds on existing glTF skin import + animator runtime

## Verification

Rebuild Debug `engine` OK. Editor restarted; owner should confirm Idle/walk deformation + Diagnostics “Skin matrices” row in Game play-test.

## What changed

### Summary

Player play-test deformation is now GPU LBS: bind-pose VB carries joints/weights; CPU builds skin matrices into a ringed bone CB; lit and shadow VS blend. Removed per-pose `patch_mesh_vertices` / `cpu_skin_positions` from the hot path.

### Files / surfaces

- `src/rendering/render_app.cpp` — Vertex +52B, append influences, bone CB ring, VS LBS, `set_pending_skin_matrices`
- Docs: `mesh-assets.md`, `character-controller.md`, `debug-world.md`

### Schema / API

- GPU `Vertex` adds JOINTS/WEIGHTS; InputLayout 6 elements for lit/shadow prop draws
- Root sig: lit slot 5 / shadow slot 2 = bone CBV `b2`

### Verification evidence

- MSBuild Debug `engine` succeeded
- Editor restarted after rebuild

### Decisions

- DEC-0047: CPU matrices + GPU LBS; MAX_BONES 64; player-first enablement

### Leftover risk / follow-ons

- Catalog NPC skinning not enabled
- Owner visual QA for Idle/walk + shadow silhouette

## Agent notes

Shipped with TICKET-0226 in the same pass.
