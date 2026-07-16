# TICKET-0160: Toggle + slider widgets

- Epic: EPIC-0007
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ed3efc5695815c8bc1de1b62278bd7

## Goal

Add focusable `toggle` and `slider` widget types to the UI canvas stack with draw, play-test input, editor authoring, and MCP mutate support.

## Context links

- [DEC-0025](../../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp)
- [`context/features/ui-canvas.md`](../../features/ui-canvas.md)
- Predecessor: TICKET-0159 (button + focus)

## Acceptance criteria

- [x] `toggle` widget: bool `bind`, optional `text`/`label`; draw checkbox + label; focus + activate flips value
- [x] `slider` widget: `bind` + optional `maxBind`; draw track + thumb; focus + arrows adjust; click sets value
- [x] `HudRuntime` bool helpers (`set_bool` / `get_bool`) and Lua `engine.hud_set_bool` / `hud_get_bool`
- [x] `engine_ui_canvas_mutate` `add`/`style` supports toggle/slider
- [x] UI editor: Add toggle/slider toolbar + inspector fields
- [x] Suite tests + docs; rebuild `engine`

## Out of scope

Image-backed controls (TICKET-0164); settings screen content polish (0161 sample only).

## Dependencies

Blocked by TICKET-0159 approval; parallel-safe with 0161.

## Verification

`engine_suite_tests --suite hud`; play test focus/adjust on settings rows.

## Agent notes

Shipped with 0161–0164 batch. Left/Right arrows and gamepad D-pad adjust focused sliders.
