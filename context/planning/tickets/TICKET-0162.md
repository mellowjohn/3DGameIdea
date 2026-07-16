# TICKET-0162: Inventory UI canvas sample

- Epic: EPIC-0007
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ed3efc5695811bb9d4dff0d92b72d5

## Goal

Ship a modal `inventory` UI canvas sample (grid/list placeholder) integrated with the canvas stack and play-test input.

## Context links

- [DEC-0025](../../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp)
- [`context/features/ui-canvas.md`](../../features/ui-canvas.md)
- Game systems: inventory runtime TBD (content epic)

## Acceptance criteria

- [x] `assets/ui/inventory.uicanvas.json` (id `inventory`) with panel + placeholder slots/labels
- [x] Register on stack; Lua/MCP `ui_push('inventory')` smoke path
- [x] Play-test hook (debug key **I**) opens inventory; Esc pops
- [x] Docs list sample
- [x] Suite: load/register/push inventory canvas

## Out of scope

Real item data binding, drag-drop, equipment rules.

## Dependencies

TICKET-0159; prefers 0160/0164 for richer slot widgets.

## Verification

Play test open/close inventory; hud suite load/register tests.

## Agent notes

`inventory.close` → `ui_pop`. Debug key I only when no modal and test session active.
