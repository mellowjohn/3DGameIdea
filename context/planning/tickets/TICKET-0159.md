# TICKET-0159: Interactive focus + button + pause sample



- Epic: EPIC-0007

- Status: needs-approval

- Agent: cursor

- Priority: P2

- Notion: https://app.notion.com/p/39ed3efc569581f3a0fccc46de72938d



## Goal



Interactive canvas v1: focusable `button`, mouse click + keyboard/gamepad navigate, top-of-stack input capture, Esc/back pop; sample pause canvas over always-on HUD ([DEC-0025](../../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp)).



## Context links



- [DEC-0025](../../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp)

- [`context/features/ui-canvas.md`](../../features/ui-canvas.md)

- Soft: TICKET-0060 (full remapping UI stays P3; thin hooks only)



## Acceptance criteria



- [x] `button` widget type; focus ring / navigation

- [x] Top interactive canvas captures input; Esc/back pops

- [x] Sample pause canvas push/pop in open-world sample

- [x] Agents can drive pause via MCP stack ops

- [x] Suites + docs; rebuild `engine`



## Out of scope



Full accessibility settings screen; inventory/dialogue content polish; remapping UI product (0060)



## Dependencies



Blocked by TICKET-0156; wants 0157/0158 for authoring the pause layout.



## Verification



Play test: open pause, navigate buttons, Esc closes; MCP stack smoke.



- `engine_suite_tests --suite hud`: button focus/nav/cancel + mutate add button

- Play test (F5): Esc opens pause overlay; arrow/Tab cycles focus; Enter activates Resume; Esc closes

- `engine_ui_stack` push/pop pause during live editor MCP



## Implementation notes



- `HudWidgetType::Button` with required `bind` → `uiButtons` in `bindings.script.json` or fallback `on_ui_button(payload)`

- `UiCanvasStack::handle_modal_input` + play-test wiring in `render_app.cpp`

- Samples: `pause.uicanvas.json` (Resume/Quit buttons), `main_menu.uicanvas.json` (menu rows as buttons)

- `assets/scripts/ui_handlers.lua` + `uiButtons` entry for `pause.resume`

