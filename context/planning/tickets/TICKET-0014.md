# TICKET-0014: Editor/MCP entry points for World Forge ops

- Epic: EPIC-0002
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ad3efc569581288300f82ba49bd1cb

## Goal

Ship command-backed MCP/editor entry points for World Forge core assets (factions, relationships, map): get / validate / apply, offline-capable, sharing one operation path with the live editor bridge. No graph UI panels.

## Context links

- `context/formats/world-forge-mcp.md`
- `context/architecture/content-vs-engine-workflows.md`
- `context/features/mcp-live-editor.md`
- `include/engine/automation/world_forge_commands.h`
- Related: TICKET-0011–0013 schemas

## Acceptance criteria

- [x] MCP tool `engine_world_forge_apply` (actions get|validate|apply; kinds factions|relationships|map).
- [x] Shared `apply_world_forge_operation` used offline and via editor `world_forge_apply`.
- [x] Faction-id cross-check on relationships/map validate/apply when factions file present.
- [x] `engine_scene_plan` classifies `*.worldforge.json` / World Forge as `world_forge`.
- [x] Docs: format note, mcp-live-editor, content-vs-engine workflows, indexes.
- [x] Suite coverage: classifier + get/validate/reject invalid apply in `automation`; `world_forge` still green.

## Out of scope

World Forge docking panels / graph editor UI; quest/dialogue MCP; inventing story canon; Scene mesh edits.

## Dependencies

TICKET-0011–0013 schemas.

## Verification

- Rebuild `engine_core` + `engine_suite_tests` — passed.
- `--suite world_forge` 40/40; `--suite automation` 60/60 (from repo root).
- Reload Cursor MCP after `engine.exe` relink to pick up the new tool (relink may be blocked if editor holds the binary).

## What changed

### Summary

Added `engine_world_forge_apply` for reading, validating, and writing World Forge JSON assets. Works offline; when the editor bridge is live it forwards to the same `world_forge_apply` operation. Classifier routes World Forge paths away from scene/terrain tools.

### Files / surfaces

**Created:** `world_forge_commands.h/.cpp`, `context/formats/world-forge-mcp.md`

**Modified:** `mcp_server.cpp` (tool schema + dispatch), `editor_session.cpp` (operation + classifier), CMakeLists, suites, workflow/MCP/scope docs, indexes

### Schema / API

- Tool: `engine_world_forge_apply` — `action`, optional `kind`/`path`, `json`/`source` for apply
- Editor op: `world_forge_apply`
- Errors: `WORLD-FORGE-CMD-*`

### Tests / rebuild

- `world_forge` 40/40; `automation` 60/60
- `engine_core` + suite tests OK; `engine.exe` may still be locked

### Decisions & leftovers

- File-backed apply only (no UI hot-reload panel yet).
- EPIC-0002 core schema+MCP track (0011–0014) complete pending owner approval; editor graph/map panels remain follow-ons.

## Agent notes

- 2026-07-15: Implemented and verified; awaiting owner approval.
