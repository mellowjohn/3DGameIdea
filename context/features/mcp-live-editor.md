# MCP Live Editor Automation

Status: active

The engine exposes a native Model Context Protocol (MCP) stdio server and an opt-in Windows named-pipe bridge for live editor edits.

## MCP server

```text
engine mcp --project <project-dir>
```

Tools:

- `engine_editor_status` — live automation state, world path, selection, dirty flag, play session
- `engine_scene_plan` — classify whether work belongs in scene data, prefabs, Lua, terrain, or C++ engine code (see `context/architecture/content-vs-engine-workflows.md`)
- `engine_scene_apply` — place, move, remove, rename, duplicate, undo, redo, save, `sample_terrain` through `CommandHistory`; `action: batch` with `ops[]` for multi-edit in one undo step (optional `label`, `save`); `snapToTerrain` / `groundOffset` on place and move; component actions `add_component` / `remove_component` / `set_component`
- `engine_terrain_apply` — raise/lower/flatten height, paint materials, paint foliage density (`paint_foliage` / `paint_foliage_mixed`), sample height, undo/redo (`kind`: height/paint/foliage), save, and `action: batch` with `ops[]` (one undo per height/paint/foliage group and one reload) through the same stores as the Sculpt tab ([DEC-0018](../decisions/index.md#dec-0018-mcp-terrain-sculpt-and-paint-apply)); requires live editor MCP for mutate/save (`sample` works offline)
- `engine_entity_component_apply` — dedicated add/remove/set component on a scene entity (same commands as scene apply)
- `engine_prefab_apply` — create or update prefab JSON, validate, refresh asset browser catalog; prefab writes propagate components to non-overridden instances
- `engine_prefab_component_apply` — dedicated prefab component write path (same as prefab apply with kind prefab)
- `engine_asset_apply` — create or update prefab or material JSON, validate, refresh catalog; `action: refresh_catalog` rescans without writing
- `engine_lua_apply` — write Lua script assets and hot reload when live automation is enabled
- `engine_lua_call` — dispatch a live Lua handler without physical overlap (`kind`: `interaction` | `combatHurt` | `handler`; binding `id` or `handler` name; optional `payload`). Requires live editor MCP. Agent-friendly for play-test and automated checks.
- `engine_quest_call` — drive session `QuestRuntime` (`kind`: `start` | `complete_objective` | `abandon` | `status` | `list`; `questId`; `objectiveId` for complete). Same path as Lua `engine.quest_*` ([DEC-0028](../decisions/index.md#dec-0028-explicit-quest-progression-runtime)). Requires live editor MCP; allowed during play test.
- `engine_standing_call` — drive session `StandingRuntime` (`kind`: `get` | `set` | `adjust` | `rank` | `meets` | `lock_in` | `list`; `factionId`; `score` / `delta` / `minScore` / `minRankId` as needed). Same path as Lua `engine.standing_*` ([DEC-0029](../decisions/index.md#dec-0029-continuous-faction-standing-with-hostility-transfer)). Requires live editor MCP; allowed during play test.
- `engine_hud_apply` — write UI canvas (`*.uicanvas.json`) or legacy HUD (`*.hud.json`) and hot reload during play test ([DEC-0025](../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp))
- `engine_world_forge_apply` — read/validate/write World Forge assets (`factions` / `relationships` / `map` `*.worldforge.json`); works offline; not Scene/Sculpt ([TICKET-0014](../planning/tickets/TICKET-0014.md))
- `engine_ui_stack` — canvas stack `register` / `push` / `pop` / `show` / `hide` / `clear` / `status` (play-test safe; equals Lua `engine.ui_*`)
- `engine_ui_canvas_mutate` — structural canvas edits (`add`/`remove`/`move`/`resize`/`style`); play-test safe
- `engine_project_validate` — run the existing validation command path

Cursor configuration example: `.cursor/mcp.json` launches `tools/mcp-server.cmd`, which starts `engine mcp` with the sample project. Reload the MCP server in Cursor Settings after rebuilding `engine`.

## Live editor bridge (opt-in)

The named pipe is **off by default**. In the running editor, open **Diagnostics** and enable **MCP connection** before Cursor MCP tools can reach the session.

When enabled, the editor opens a project-scoped named pipe and processes one framed JSON request per client connection on the render thread. Scene mutations never bypass undo/redo, collision sync, or in-memory scene authority.

Direct `.world.json` writes while the editor is open are rejected by design. Use `engine_scene_apply` instead.

### Batch scene edits

`engine_scene_apply` accepts `action: "batch"` with an `ops` array of single-op payloads (`place`, `move`, `remove`, `rename`). All operations run in one bridge round-trip and one undo step. Failed mid-batch applies roll back earlier ops in that batch. Optional `label` names the undo entry; `save: true` persists after a successful batch. Maximum 100 ops per request.

`engine_scene_plan` and `engine_project_validate` work without the bridge. `engine_world_forge_apply` also works offline (file + schema validate). Live scene, prefab, Lua, and HUD apply require the editor plus enabled MCP connection. `engine_lua_apply`, `engine_hud_apply`, `engine_lua_call`, `engine_quest_call`, and `engine_standing_call` are allowed during play test (scene edits remain blocked) so agents can iterate scripts, fire handlers, and test quest/standing progression without walking into volumes.

## Contracts

- Bridge requests/responses use schema version 1 and reuse stable exit codes and diagnostics.
- `editor_status` metadata includes `liveAutomationEnabled`.
- Scene edits map to existing commands in `include/engine/automation/scene_commands.h`.
- Play-test sessions block scene mutation until ended.

## Verification

- `automation` and `scripting` suites in `tests/suite_tests.cpp`
- Rebuild `engine` after automation changes
- Manual: launch editor, enable MCP connection in Diagnostics, then call MCP tools from Cursor

## Debug trace logs

File-only JSONL traces help debug MCP and bridge issues without writing to MCP stdio (which would break the protocol).

| Log file | Writer | Typical events |
|----------|--------|----------------|
| `<project>/out/logs/mcp-trace.jsonl` | `engine mcp` process | `server_start`, `request`, `tool_call`, `bridge_probe`, `bridge_response`, `response`, `error` |
| `<project>/out/logs/editor-bridge-trace.jsonl` | Editor when MCP connection is used | `server_start`, `client_connected`, `request_received`, `poll_dispatch`, `response_ready`, `client_send_failed` |

Tracing is on by default. Set `ENGINE_AUTOMATION_TRACE=0` to disable. The Diagnostics panel shows the editor-bridge trace path when MCP connection is enabled.
