# TICKET-0157: MCP UI canvas mutate (add/remove/move/style)

- Epic: EPIC-0007
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ed3efc5695818b96c9ca4b2b70afa9

## Goal

Agent-friendly structural MCP ops to add/remove/move/resize UI widgets and change style (color, font) without whole-file rewrites ([DEC-0025](../../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp)).

## Context links

- [DEC-0025](../../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp)
- [`context/features/ui-canvas.md`](../../features/ui-canvas.md)
- Shared mutate API with TICKET-0158

## Acceptance criteria

- [x] MCP mutate ops persist to `*.uicanvas.json` and hot-reload when live
- [x] Allowed during play test (non-scene)
- [x] Automation suite covers add/move/style
- [x] Docs updated

## Out of scope

Focus/input (0159)

## Verification

Rebuild `engine`; hud 38/38, automation 56/56, scripting 35/35. Status → `needs-approval`.

## Agent notes

2026-07-15: `mutate_ui_canvas_*` + `engine_ui_canvas_mutate`; optional `color`/`fontSize` widget fields.
