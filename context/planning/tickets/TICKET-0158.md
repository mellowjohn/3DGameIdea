# TICKET-0158: Editor Canvas view (drag + inspector)

- Epic: EPIC-0007
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ed3efc569581b488d5cfe4d1c5ab40

## Goal

In-editor Canvas view to select, drag, and inspect widgets (color/font/etc.) on the same `*.uicanvas.json` documents MCP mutates ([DEC-0025](../../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp)).

## Context links

- [DEC-0025](../../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp)
- [`context/features/ui-canvas.md`](../../features/ui-canvas.md)
- Shared mutate API with TICKET-0157

## Acceptance criteria

- [x] Canvas editor shows design-resolution canvas with letterbox preview
- [x] Select / drag move; inspector edits style fields; saves atomic canvas asset
- [x] Changes visible to play-test/runtime reload path
- [x] Docs updated

## Out of scope

Full UMG designer; accessibility settings UI; mini-map; focus (0159)

## Verification

Rebuild `engine`; open **UI** viewport tab — drag widgets, Save; play-test reflects. Status → `needs-approval`.

## Agent notes

2026-07-15: Viewport tab **UI** + shared mutate path (moved from floating panel).
