# TICKET-0257: Game module C ABI + host load/reload + Diagnostics

- Epic: EPIC-0020
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/3b2d3efc569581a4b6c8feb50f9a3c83

## Goal

Ship a functional Windows game-module hot-reload path: host loads a C-ABI `game_module.dll` via copy-on-load, ticks it each frame, and reloads from Diagnostics (or auto on DLL write) without restarting `engine.exe`.

## Context links

- [DEC-0053](../../decisions/index.md#dec-0053-native-game-module-hot-reload-c-abi)
- [DEC-0023](../../decisions/index.md#dec-0023-live-lua-host-api-agent-iteration-path) (Lua remains primary)
- [`context/features/game-module-hot-reload.md`](../../features/game-module-hot-reload.md)
- [`context/architecture/content-vs-engine-workflows.md`](../../architecture/content-vs-engine-workflows.md)

## Acceptance criteria

- [x] `game_module_abi.h` v1 C ABI with host log + blackboard hooks
- [x] `GameModuleHost` copy-on-load, ABI check, init/tick/shutdown, unload/reload
- [x] SHARED `game_module` sample target outputs next to `engine` in `dev-next`
- [x] Diagnostics **Game Module** tab: status, Reload, Unload, auto-reload
- [x] Sample writes `game.module_ticks` / `game.module_build_id` via blackboard
- [x] `game_module` suite: load, tick, reload, ABI mismatch reject, missing path

## Out of scope

- Hot-reloading `engine_core`
- DLL-registered Lua C functions
- Moving RPG runtimes / renderer into the DLL
- Non-Windows hot-reload

## Dependencies

None blocking. Complements existing Lua/MCP hot reload.

## Verification

- Rebuilt `engine`, `game_module`, `game_module_abi_mismatch`, `engine_suite_tests` (MSVC Debug)
- `engine_suite_tests --suite game_module` → **18/18** passed
- Killed locked `engine.exe` for link (LNK1168) then rebuilt

## What changed

- Summary: Host can hot-reload a thin Windows `game_module.dll` (copy-on-load) without restarting `engine.exe`. Diagnostics shows status and Reload/auto-reload; sample module ticks blackboard keys for live proof. Lua/content remains the primary iteration path (DEC-0053).
- Files / surfaces touched: `include/engine/game/*`, `src/game/game_module_host.cpp`, `src/game_module/*`, CMake SHARED targets, Diagnostics tab in `render_app.cpp`, `LuaRuntime::blackboard_set_number`, suite + context/DEC/epic docs.
- Schema / API / format deltas: C ABI `ENGINE_GAME_MODULE_ABI_VERSION` 1; error codes `GAME-MODULE-*`; suite name `game_module`.
- Tests / verification evidence: suite `game_module` 18/18; rebuild engine+DLL targets green after fixing stack-local host pointer UAF.
- Decisions & tradeoffs: durable `host_api_` member (module must not keep a dangling init-stack pointer); no Lua C-function registration from DLL in v1.
- Leftover risk / follow-ons: `engine_core` still needs full process restart; expand module host API as needed.

## Agent notes

Implementation landed 2026-08-04; suite green; awaiting owner `done`.
