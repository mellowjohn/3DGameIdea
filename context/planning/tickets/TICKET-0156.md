# TICKET-0156: Engine UI canvas stack + MCP/Lua clients

- Epic: EPIC-0007
- Status: done
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ed3efc5695815a90c8f4351fafcee2

## Goal

Engine-owned canvas stack (`push` / `pop` / `show` / `hide`) with MCP and Lua as equal clients ([DEC-0025](../../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp)).

## Context links

- [DEC-0025](../../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp)
- [`context/features/ui-canvas.md`](../../features/ui-canvas.md)

## Acceptance criteria

- [x] Stack API in C++; MCP tools + Lua mirrors
- [x] Always-on HUD layer + modal screen canvases
- [x] Suite coverage for push/pop ordering
- [x] Docs updated

## Out of scope

Focus navigation, button clicks, editor Canvas, structural mutate

## Dependencies

Blocked by TICKET-0155 (done). Soft-blocks 0159.

## Verification

Rebuild `engine`; hud 29/29, scripting 35/35, automation 52/52. Live push/pop pause during play test. Owner closed → `done`.

## Agent notes

2026-07-15: `UiCanvasStack`, sample `pause.uicanvas.json`, MCP `engine_ui_stack`, Lua `engine.ui_*`. Owner smoke confirmed.
