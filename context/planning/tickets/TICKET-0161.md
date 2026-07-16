# TICKET-0161: Fill-edge scale + settings sample

- Epic: EPIC-0007
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ed3efc5695819889ccc84271906845

## Goal

Per-canvas `scaleMode` (`letterbox` default, `fill_edges` cover) and a sample `settings.uicanvas.json` opened from the main menu.

## Context links

- [DEC-0025](../../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp)
- [`context/interviews/open-questions.md`](../../interviews/open-questions.md#ui-canvas-scale-modes-non-blocking)
- [`context/features/ui-canvas.md`](../../features/ui-canvas.md)

## Acceptance criteria

- [x] `scaleMode` on `*.uicanvas.json` (`letterbox` | `fill_edges`); parse/serialize + layout math
- [x] HUD and modal draw use per-canvas mode; letterbox remains default
- [x] Sample `assets/ui/settings.uicanvas.json`; main menu Settings pushes it
- [x] Editor + MCP document/accept `scaleMode`
- [x] Suite tests for cover scale; rebuild `engine`

## Out of scope

Per-widget scale modes; stretch (non-uniform) scaling.

## Dependencies

Soft: TICKET-0160 for settings rows (toggle/slider); can ship letterbox-only settings first.

## Verification

Play test: settings opens from main menu; session stays paused; Back pops to menu.

## Agent notes

`compute_ui_canvas_layout(..., scale_mode)` added; draw uses viewport clip for fill_edges overflow.
