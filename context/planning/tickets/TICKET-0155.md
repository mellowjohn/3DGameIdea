# TICKET-0155: UI canvas format + responsive draw

- Epic: EPIC-0007
- Status: done
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ed3efc5695819081f1d6d8501b526b

## Goal

Ship versioned `*.uicanvas.json` with default design resolution 1920×1080, load/draw with uniform letterbox/pillarbox scaling, and migrate the sample player HUD from `*.hud.json` onto a canvas ([DEC-0025](../../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp)).

## Context links

- [DEC-0025](../../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp)
- [DEC-0024](../../decisions/index.md#dec-0024-mcp-hud-toolkit-with-lua-value-binds) (migrate, do not delete capabilities)
- [`context/features/ui-canvas.md`](../../features/ui-canvas.md)
- [`context/formats/ui-canvas-assets.md`](../../formats/ui-canvas-assets.md)

## Acceptance criteria

- [x] `*.uicanvas.json` schema v1 loads/validates (fail closed)
- [x] Widgets position in design space; runtime uniform scale + letterbox/pillarbox
- [x] Sample player health HUD works on a canvas during play test
- [x] Docs: format page + feature index; `hud` suite updated
- [x] Rebuild `engine`; relevant suites green

## Out of scope

Stack push/pop, MCP structural mutate, Canvas editor tab, button/focus (0156–0159)

## Dependencies

Builds on TICKET-0153 HUD runtime. Blocks 0156–0159.

## Verification

Rebuild `engine`; `hud` 17/17, `scripting` 30/30, `automation` 47/47. Owner smoke + layout retune; closed → `done`.

## Agent notes

2026-07-15: UiCanvasAsset + letterbox HudRuntime; sample enlarged for 1080p; edge-fill scale mode deferred (open question). Owner closed.
