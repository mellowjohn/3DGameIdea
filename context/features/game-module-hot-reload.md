# Game Module Hot-Reload

Status: active (TICKET-0257 / DEC-0053)

## Summary

`engine.exe` can load a thin Windows **`game_module.dll`** at runtime, tick it each frame, and **reload** it from Diagnostics without restarting the process. Rebuild only the `game_module` CMake target, then Reload (or enable auto-reload on DLL write).

Lua handlers, UI canvases, scenes, prefabs, and assets remain the **primary** hot-reload path ([DEC-0023](../decisions/index.md#dec-0023-live-lua-host-api-agent-iteration-path)). This path is for **native C++** that must live outside Lua but should not require a full editor restart.

## Architecture

| Piece | Role |
| --- | --- |
| Host (`engine_core` / `GameModuleHost`) | `LoadLibrary` on a **copy** of the DLL, ABI check, init/tick/shutdown, Diagnostics UI, optional mtime poll |
| Module (`game_module` SHARED target) | Game-only C++ talking through C ABI host callbacks (log, blackboard) |
| Canonical path | `build/.../dev-next/game_module.dll` (next to `engine.exe`) |
| Mapped path | `dev-next/game_module_generations/game_module.<N>.dll` |

Host owns: D3D12, ImGui, Jolt, EnTT, `LuaRuntime`, editor/MCP, RPG runtimes.

Module must **not** link `engine_core`, imgui, Jolt, or Lua.

## ABI v1

Header: [`include/engine/game/game_module_abi.h`](../../include/engine/game/game_module_abi.h)

Exports: `game_module_abi_version`, `game_module_name`, `game_module_init`, `game_module_tick`, `game_module_shutdown`.

Host table: log, `blackboard_set_number` / `blackboard_set_bool`, optional get helpers.

Sample keys written by the shipped sample: `game.module_ticks`, `game.module_build_id`, `game.module_loaded`.

Optional Lua: after a successful load/reload, host calls `on_game_module_reloaded` with JSON `{ name, abiVersion, generation, source }` if that global exists (missing handler is silent success via normal `call_handler` error ignore at the notify site).

## Editor workflow

1. Leave the editor running.
2. Edit `src/game_module/sample_game_module.cpp` (or your own module sources on the same ABI).
3. MSBuild / CMake build target **`game_module` only** (not a full kill → rebuild `engine` unless you also changed `engine_core`).
4. Diagnostics → **Game Module** → **Load / Reload**, or check **Auto-reload on DLL write**.
5. Confirm **Name**, **Generation**, and blackboard ticks / build id updated.

`engine_core` code changes still require: acquire rebuild lease → kill `engine.exe` → rebuild `engine` → restart.

## Tests

- Suite: `game_module` (`engine_suite_tests --suite game_module`)
- Checks load, tick, blackboard mirror, reload generation bump, ABI mismatch reject (`game_module_abi_mismatch.dll`), missing path fail-closed.

## Non-goals

- Hot-reloading `engine_core` / the whole engine
- Marketplace / arbitrary C++ component plugins
- Moving `GameSession`, quest, inventory, or renderer into the DLL
- Registering `lua_CFunction`s from the DLL in v1

## Related

- [DEC-0053](../decisions/index.md#dec-0053-native-game-module-hot-reload-c-abi)
- [content-vs-engine-workflows.md](../architecture/content-vs-engine-workflows.md)
- EPIC-0020 / TICKET-0257
