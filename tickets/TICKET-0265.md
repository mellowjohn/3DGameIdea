# TICKET-0265: Animation Studio MCP camera framing

- Epic: EPIC-0019
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: (mirror when board is updated)

## Goal

Agents can frame the Animation Studio sandbox camera without leaving the Animation tab or using Scene `engine_editor_camera` (which switches tabs and drives a different DebugCamera).

## Context links

- `context/features/animation-studio.md`
- `skills/live-editor-mcp/SKILL.md`
- TICKET-0248 (dedicated animation camera)

## Acceptance criteria

- [x] `engine_animation_call` kinds: `camera_status`, `camera_set`, `camera_look_at`, `camera_orbit` (presets and/or yaw/pitch/distance).
- [x] Ops mutate Animation Studio camera only; stay on Animation viewport tab.
- [x] Docs + MCP schema updated.
- [x] Live smoke: orbit front/side then screenshot Attack.

## Out of scope

- Scene `engine_editor_camera` rewrite
- Full fly-cam input simulation
- Onion skin ghosts

## Dependencies

Soft: TICKET-0248 animation viewport camera.

## Verification

- `engine_suite_tests --suite animator` 415/415.
- Live: `camera_orbit` preset `side` / `threequarter` returned camYawDeg/camPitchDeg; Attack contact sheet from new framing.

## What changed

- Summary: Moved Studio free-cam onto `EditorState::anim_studio_camera` and exposed MCP framing kinds so agents can reframe stills without Scene tab switches.
- Files: `src/rendering/render_app.cpp`, `src/automation/mcp_server.cpp`, `context/features/animation-studio.md`, `skills/live-editor-mcp/SKILL.md`.
- MCP: `camera_status`, `camera_set`, `camera_look_at`, `camera_orbit` (presets front/back/side/left/right/threequarter).
- Leftover: Scene `engine_editor_camera` still appears unimplemented in bridge (separate gap).

## Agent notes

Shipped with TICKET-0266. Editor + MCP reset; build lease released.
