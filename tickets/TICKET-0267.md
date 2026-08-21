# TICKET-0267: MCP held tip / onion / slash-review aids

- Epic: EPIC-0019
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: (mirror when board is updated)

## Goal

Agents can inspect the real held-weapon tip path (weld-aware), onion-skin tip/blade ghosts in Animation Studio, stamp tip trails on contact sheets, and frame a melee-friendly `slash_review` camera — so Attack/slash polish is not guessed from grip-only samples or rear views.

## Context links

- `context/features/animation-studio.md`
- `skills/author-character-animation/SKILL.md`
- TICKET-0261 / TICKET-0265 / TICKET-0266

## Acceptance criteria

- [x] `held_tip_series` (and `tip_series` with weld+skin): per-time `gripWorld` / `heldWorld` / `tipWorld` / `tipDir` / cumulative `pathLength`; `tipLocal` override or mesh-bounds default.
- [x] `inspect_weld` (or enhanced `get_weld`): current weld + tip estimate at scrub time.
- [x] `onion_skin`: enable/disable + `times[]`; Animation viewport draws tip trail + grip→tip blade ghosts.
- [x] `seek_times` with `tipTrail=true`: contact-sheet result includes tip world series; each slot stamps a cumulative tip arc (ortho side/front/top map).
- [x] `camera_orbit` preset `slash_review` (aliases `melee` / `slash`): front-¾ yaw ~145°, default closer distance.
- [x] Docs + MCP schema + suite coverage for GPU-free helpers.

## Out of scope

- Full mesh silhouette onion (subject mesh ghosts)
- Automatic edge-vs-flat classification from images
- Changing Attack clip content in this ticket

## Dependencies

TICKET-0265 camera, TICKET-0266 sample_series / seek_times, TICKET-0251 weld.

## Verification

- Suite `animator` 423/423.
- Rebuilt `engine` (MSVC Debug); editor + MCP reset; lease released.
- Live Attack: `held_tip_series` pathLength ~4.1; `inspect_weld` tipWorld; `onion_skin` 7 times; `camera_orbit slash_review` yaw~145°; `seek_times tipTrail` → `out/attack-tiptrail-0267b-*.png` + tipTrail world series.

## What changed

- Summary: Animation Studio MCP can now read weld-aware sword tip paths, draw onion tip/blade ghosts in the viewport, burn tip arcs onto contact sheets, and frame a dedicated `slash_review` camera — closing the grip-only inspect gap that blocked Attack tip polish.
- Files: `include/engine/animation/anim_studio_agent_ops.h`, `src/animation/anim_studio_agent_ops.cpp`, `src/rendering/render_app.cpp`, `src/automation/mcp_server.cpp`, `tests/suite_tests.cpp`, `context/features/animation-studio.md`, `context/planning/epics.md`, `tickets/TICKET-0267.md`, `skills/live-editor-mcp/SKILL.md`.
- MCP: `held_tip_series`, `inspect_weld`, `onion_skin`, `seek_times tipTrail`/`tipTrailView`/`tipLocal`, `camera_orbit` preset `slash_review`|`melee`|`slash`. Subject skin resolves from Studio prefab when `test_skinned_mesh_asset` is unset.
- Tests: animator suite adds tip_local AABB, held tip series, and tip trail stamp checks (423/423).
- Leftover: tipLocal from mesh AABB is an estimate (not authored edge length); tipTrail stamp is ortho map on the full backbuffer (readable arc, not camera-projected).

## Agent notes

Owner ask: tip-forward Attack polish blocked by grip-only inspect and rear cameras — ship MCP aids first.
