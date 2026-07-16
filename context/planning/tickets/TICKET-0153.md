# TICKET-0153: MCP HUD toolkit v1 + player health bar

- Epic: EPIC-0007
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ed3efc569581db9fd7c43d9c6c8645

## Goal

Ship HUD toolkit v1: versioned `*.hud.json` (bar/text/panel), play-test overlay draw, `engine_hud_apply` MCP, Lua `hud_*` / `set_health` binds, and sample player health HUD driven by combat/campfire scripts ([DEC-0024](../../decisions/index.md#dec-0024-mcp-hud-toolkit-with-lua-value-binds)).

## Context links

- [DEC-0024](../../decisions/index.md#dec-0024-mcp-hud-toolkit-with-lua-value-binds)
- [DEC-0023](../../decisions/index.md#dec-0023-live-lua-host-api-agent-iteration-path)
- [`context/formats/hud-assets.md`](../../formats/hud-assets.md)
- [`context/features/hud-toolkit.md`](../../features/hud-toolkit.md)
- Soft: TICKET-0062 (HUD IA consumes this runtime)

## Acceptance criteria

- [x] `*.hud.json` schema v1 loads/validates; invalid widgets fail closed.
- [x] Play-test Game viewport draws bar/text/panel from loaded HUD.
- [x] `engine_hud_apply` writes + hot-reloads during play; scene plan classifies `.hud.json`.
- [x] Lua `hud_set_number` / `hud_set_text` / `hud_set_visible` / `set_health` / `get_health`.
- [x] Sample health HUD + combat hurt damage + campfire heal.
- [x] Suites green; `engine` rebuilt; context indexes updated.

## Out of scope

Buttons/input, full UMG designer, minimap, accessibility product IA, movement APIs.

## Dependencies

Owner override of EPIC-0007 P3 hold. Builds on TICKET-0152 host API.

## Verification

Rebuild `engine`; `scripting` 30/30, `hud` 9/9, `automation` 40/40. Status → `needs-approval` — never `done`.

## Agent notes

2026-07-15: Implemented DEC-0024 toolkit v1. Suites green after full engine rebuild.
