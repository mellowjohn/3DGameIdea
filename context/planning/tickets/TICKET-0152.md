# TICKET-0152: Lua host API v1 (log / json_decode / blackboard)

- Epic: EPIC-0006
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ed3efc569581c5bcb7ed4de90fa1f1

## Goal

Ship a sandboxed `engine.*` host API (`log`, `json_decode`, bool/number/string blackboard) so live interaction/combat Lua handlers can read payloads and produce testable side effects without a C++ rebuild for script edits ([DEC-0023](../../decisions/index.md#dec-0023-live-lua-host-api-agent-iteration-path)).

## Context links

- [DEC-0023](../../decisions/index.md#dec-0023-live-lua-host-api-agent-iteration-path)
- [`context/features/lua-scripting.md`](../../features/lua-scripting.md)
- [`context/architecture/content-vs-engine-workflows.md`](../../architecture/content-vs-engine-workflows.md)
- [`include/engine/scripting/lua_runtime.h`](../../../include/engine/scripting/lua_runtime.h)
- Follow-ons: TICKET-0116 (more handlers), 0113/0115 (abilities/queries), DEC-0022 animator drive

## Acceptance criteria

- [x] Global `engine` table registered in sandbox (runtime + validate VMs): `log`, `json_decode`, `blackboard_set`, `blackboard_get`.
- [x] Sample campfire / combat hurt scripts decode payload, log, and set blackboard keys.
- [x] `scripting` suite covers dispatch → blackboard, json_decode, bad JSON fail-closed, invalid log level safe.
- [x] Context documents live agent loop + v1 API; DEC-0023 recorded.
- [x] Rebuild `engine_core` / suite tests; `scripting` suite green (full `engine.exe` link blocked while process locked).

## Out of scope

- Scene mutation, damage, audio, particles, animator drive from Lua
- Hot-reloading `bindings.script.json`
- Changing handler args from JSON string to tables
- Full sandbox redesign (`io`/`os` removal)

## Dependencies

Owner override of M6 P3 hold. No blocker on M5 animation exit for this thin host surface.

## Verification

Rebuild `engine`; run `engine_suite_tests --suite scripting`. Set Status to `needs-approval` after verification — never `done`.

## Agent notes

2026-07-15: Implemented host API v1. `scripting` suite 24/24 passed. Full `engine.exe` link hit LNK1168 (executable locked); `engine_core.lib` + `engine_suite_tests` rebuilt successfully. Restart editor to pick up the new host API in the running process.
