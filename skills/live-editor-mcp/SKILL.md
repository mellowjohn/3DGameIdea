---
name: live-editor-mcp
description: >-
  Drive the open-world-rpg live editor via MCP: build lease, kill/rebuild/restart,
  scene_plan routing, and scene/terrain/prefab/asset apply. Use when editing the
  live scene, placing prefabs, sculpting terrain, hot-reloading Lua/UI/particles,
  fixing LNK1168 / locked engine.exe, or when unsure whether work is C++ vs MCP.
---

# Live Editor MCP

Agent loop for content edits against a running `engine.exe mcp` / editor session.

**Read first:** [`context/features/mcp-live-editor.md`](../../context/features/mcp-live-editor.md), [`context/architecture/content-vs-engine-workflows.md`](../../context/architecture/content-vs-engine-workflows.md), [`context/features/agent-build-coordination.md`](../../context/features/agent-build-coordination.md).

**Server:** `project-0-3DGameIdea-ai-rpg-engine` (Cursor `.cursor/mcp.json` → `tools/mcp-server.cmd`).

## Checklist

```
Live edit:
- [ ] engine_editor_status (bridge on? world path? dirty?)
- [ ] engine_scene_plan when target kind unclear
- [ ] Mutate via MCP apply tools — never write .world.json while editor owns scene
- [ ] Screenshot / ui_query to verify when visual
- [ ] If C++ changed: acquire lease → kill engine → MSBuild → restart MCP → release
```

## 1. Route work (`engine_scene_plan`)

| Kind | Tool |
| --- | --- |
| `scene_data` | `engine_scene_apply` (place/move/batch/components) |
| `terrain_data` | `engine_terrain_apply` (height/paint/foliage) |
| `prefab_asset` | `engine_prefab_apply` / `engine_asset_apply` |
| `lua_script` | `engine_lua_apply` (+ `engine_lua_call` to exercise) |
| `hud_asset` / UI canvas | `engine_hud_apply` / `engine_ui_canvas_mutate` / `engine_ui_stack` |
| `world_forge` | `engine_world_forge_apply` (offline OK) |
| `engine_code` | Edit `src/`/`include/` then rebuild (below) |

Direct offline JSON edits to the open world while the editor is live are **rejected by design**.

## 2. Live bridge

1. `engine_editor_status` — confirm live automation.
2. If off: `engine_editor_live` `action=enable`.
3. Prefer `engine_editor_ui_query` + `engine_editor_input` over CV for UI clicks.
4. `engine_editor_screenshot` → project `out/` for visual proof.

## 3. Common mutate patterns

- **Batch place/move:** `engine_scene_apply` `action: batch` with `ops[]`; optional `snapToTerrain` / `groundOffset`.
- **Terrain + grass:** paint `grass.material.json` before grass foliage; never grass on water/moat ([`grass-foliage-on-grass-terrain.mdc`](../../.cursor/rules/grass-foliage-on-grass-terrain.mdc)).
- **No `bush_wide`:** use `bush` / `bush_tall` ([`no-bush-wide-asset.mdc`](../../.cursor/rules/no-bush-wide-asset.mdc)).
- **Particles:** `engine_asset_apply` `kind: particle` then attach on prefab `particles[]`.
- **Validate:** `engine_project_validate` after content batches.

## 4. Rebuild loop (C++ / shaders / locked exe)

Multi-agent Windows checkout — **do not MSBuild while queued**.

1. Claim ticket active in `epics.md` when applicable.
2. `engine_build_coordination` `acquire` (`agentId`, `ticketId`, summary). If `queued`, poll `wait` — never build.
3. Kill locked `engine.exe` (`build/windows-msvc-debug/dev-next/engine.exe` or legacy `Debug/`).
4. MSBuild `engine` target:

```text
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe" ^
  build\windows-msvc-debug\AIRpgEngine.sln /t:engine /p:Configuration=Debug /m
```

5. Restart the same command (typical):

```text
build\windows-msvc-debug\dev-next\engine.exe mcp --project samples/open-world-rpg
```

6. `engine_build_coordination` `release` with the lease token.
7. Mention reset + lease release in the final response.

```text
❌ Leave LNK1168 / ask user to close editor
❌ Start MSBuild while another agent holds the lease
✅ acquire → kill → rebuild → relaunch mcp → release
```

## 5. Play-test automation

| Need | Tool |
| --- | --- |
| Quest / flags / standing | `engine_quest_call` / `engine_flag_call` / `engine_standing_call` |
| Co-op | `engine_coop_call` |
| Lua handlers | `engine_lua_call` |

## Done bar

Status checked; correct apply surface used; visual or validate evidence when relevant; rebuild lease released if acquired.
