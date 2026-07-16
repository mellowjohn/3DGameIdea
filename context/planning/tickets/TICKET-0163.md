# TICKET-0163: Dialogue UI canvas sample

- Epic: EPIC-0007
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ed3efc569581b393c0c389944abf8a

## Goal

Ship a modal `dialogue` UI canvas sample (speaker + body text + continue button) on the canvas stack.

## Context links

- [DEC-0025](../../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp)
- [`context/features/ui-canvas.md`](../../features/ui-canvas.md)
- Narrative: TICKET-0053 dialogue graph (P3) — UI only here

## Acceptance criteria

- [x] `assets/ui/dialogue.uicanvas.json` (id `dialogue`) with panel, speaker text, body text, continue button
- [x] Register on stack; `ui_push('dialogue')` + `ui_pop` via continue bind
- [x] Play-test debug open path (**Y**); focus navigates continue
- [x] Docs + suite load/register/push

## Out of scope

Branching dialogue graph, VO, World Forge integration.

## Dependencies

TICKET-0159; soft TICKET-0164 for portrait image slot.

## Verification

Play test dialogue overlay; hud suite smoke.

## Agent notes

`dialogue.continue` → `ui_pop`. Debug key Y when no modal.
