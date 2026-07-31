# TICKET-0244: MCP Scene camera + play-test session tools

- Epic: EPIC-0009
- Status: needs-approval
- Agent: cursor-agent
- Priority: P1
- Notion: https://app.notion.com/p/3acd3efc5695811991c0fedf816aa9e2

## Goal

Agents can stop play-test and frame the Scene DebugCamera on a named entity (or world point) through MCP, so article/blog stills and Scene QA do not depend on desktop RMB fly or tiny toolbar clicks.

## Context links

- [`context/features/mcp-live-editor.md`](../../features/mcp-live-editor.md)
- Existing patterns: `engine_coop_call`, World Forge marker focus
- Related: blog/article capture skill (`skills/record-article-capture`)

## Acceptance criteria

- [x] `engine_editor_session` supports `kind=status|start|end|pause|resume|set_overlays` over the live bridge
- [x] Optional overlay bools: `showEventZones`, `showCollisionDebug`, `showWorldForgeMapMarkers`
- [x] `engine_editor_camera` supports `action=status|set_pose|look_at|focus_entity|select_entity`
- [x] `focus_entity` resolves unique entity `name` (exact or unique substring) or `entityId`, frames Scene camera (`distance`/`height` defaults 8/3), selects entity by default, switches to Scene tab
- [x] Framing during an active play-test updates restore pose so `end` does not wipe the framed camera
- [x] Tools documented in `mcp-live-editor.md`; rebuild `engine` succeeds

## Out of scope

- Game/orbit camera scripting during play-test (use `engine_coop_call` move/look paths)
- Hierarchy search UI
- Changing desktop `windows-computer-use` RMB APIs

## Dependencies

None. Soft: live editor MCP already enabled.

## Verification

1. Rebuild `engine` (MSBuild Debug) — succeeded (C4996/C4456/C4100 warnings only, pre-existing style).
2. Live sandbox: bridge `editor_session` `kind=end` + `showEventZones=false` → `editor_camera` `focus_entity` `name=campfire_mesh_test` `distance=4.2` `height=1.8` → screenshot under `out/blog-fire-clean-*.png` (tight Scene framing confirmed).
3. Lease released after kill → rebuild → editor restart.

## What changed

- Summary: Added two live MCP tools so agents can end play-test and frame the Scene DebugCamera on entities/points without desktop RMB fly. Used them to reframe sandbox campfire blog stills.
- Files / surfaces: `src/rendering/render_app.cpp` (bridge handlers + shared `pose_debug_camera_looking_at` / entity name resolve), `src/automation/mcp_server.cpp`, `src/automation/editor_session.cpp` (error hint), `context/features/mcp-live-editor.md`, `skills/record-article-capture/SKILL.md`, blog stills under `blog/public/images/`.
- Schema / API / format deltas: MCP tools `engine_editor_session`, `engine_editor_camera`; bridge ops `editor_session`, `editor_camera`; error codes `SESSION-*`, `CAMERA-*`.
- Seed / sample data: none (sandbox `campfire_mesh_test` used for verification only).
- Tests / verification evidence: MSBuild `engine` OK; live bridge smoke on sandbox focus+screenshot. Cursor MCP stdio was killed during rebuild — reload MCP server in Cursor to pick up new tool schemas.
- Decisions & tradeoffs: Session and camera are separate tools (mirror `coop_call` style). Degrees preferred for pose; radian `yaw`/`pitch` still accepted. Default focus distance 8/height 3 (tighter than spawn framing).
- Leftover risk / follow-ons: Notion mirrored; reload Cursor MCP after rebuild. Follow-up: `deselect` + `yawOffsetDegrees` + `select=false` clears gizmos (landed same ticket). Article captures refreshed 2026-07-29 afternoon (video + clear stills).

## Agent notes

Owner ask 2026-07-29 during blog fire-capture: Scene framing blocked by missing camera/session MCP. Implemented same day.
