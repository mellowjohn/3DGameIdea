# TICKET-0154: MCP engine_lua_call script event dispatch

- Epic: EPIC-0007
- Status: done
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ed3efc569581d48daaf0bc0f2133a9

## Goal

Let agents and humans dispatch live Lua gameplay handlers (`interaction`, `combatHurt`, or raw `handler`) through MCP without requiring physical volume overlaps — core agent-friendly play-test loop.

## Context links

- [`context/features/lua-scripting.md`](../../features/lua-scripting.md)
- [`context/features/mcp-live-editor.md`](../../features/mcp-live-editor.md)
- Builds on TICKET-0152 / TICKET-0153 host API + HUD

## Acceptance criteria

- [x] `engine_lua_call` MCP tool + bridge `lua_call` op
- [x] Supports `interaction`, `combatHurt`, `handler` kinds
- [x] Works during play test when MCP enabled (ops run before scene/play gates)
- [x] Automation suite covers combatHurt damage + campfire heal via lua_call
- [x] Docs updated

## Out of scope

Lua REPL console UI; synthesizing physics contacts; combatHits dispatch

## Verification

Rebuild `engine`; `automation` 46/46, `scripting` 30/30, `hud` 9/9. Live MCP `combatHurt` + campfire heal verified during play test. Owner closed → `done`.

## Agent notes

2026-07-15: Fixed gate ordering so `lua_call` / `lua_apply` / `hud_apply` stay available without scene and during play test. Live play-test smoke confirmed; owner closed.
