---
name: live-editor-mcp
description: >-
  Drive the open-world-rpg live editor via MCP: build lease, kill/rebuild/restart,
  scene_plan routing, and scene/terrain/prefab/asset apply. Use when editing the
  live scene, placing prefabs, sculpting terrain, hot-reloading Lua/UI/particles,
  fixing LNK1168 / locked engine.exe, or when unsure whether work is C++ vs MCP.
---

# Live Editor MCP

Agent loop for content edits against a running **editor** bridged by Cursor’s `engine.exe mcp` proxy.

**Two processes:** `engine.exe editor` = UI + named-pipe bridge; `engine.exe mcp` = windowless stdio proxy Cursor launches. Live tools need the **editor** up — MCP alone reports `editorRunning: false`. See finding in [`context/testing/findings.md`](../../context/testing/findings.md) (2026-08-04 restart-only-mcp).

**MCP first (repo-wide):** if a tool exists for the job, use it or **fix** it — never Python/pipe/script substitutes. See [`mcp-no-python-substitutes.mdc`](../../.cursor/rules/mcp-no-python-substitutes.mdc).

**Read first:** [`context/features/mcp-live-editor.md`](../../context/features/mcp-live-editor.md), [`context/architecture/content-vs-engine-workflows.md`](../../context/architecture/content-vs-engine-workflows.md), [`context/features/agent-build-coordination.md`](../../context/features/agent-build-coordination.md).

**Server:** `project-0-3DGameIdea-ai-rpg-engine` (Cursor `.cursor/mcp.json` → `dev-next/engine.exe mcp --project …`).

## Checklist

```
Live edit:
- [ ] Editor window running (not only mcp proxy)
- [ ] engine_editor_status (editorRunning + live bridge? world path? dirty?)
- [ ] engine_scene_plan when target kind unclear
- [ ] Mutate via MCP apply tools — never write .world.json while editor owns scene
- [ ] Screenshot / ui_query to verify when visual
- [ ] If C++ changed: before handoff, either acquire lease → kill all engine.exe → MSBuild → relaunch editor + MCP → release, or verify the documented supported hot-reload path; never leave a build/reload pending
```


## 1. Route work (`engine_scene_plan`)

| Kind | Tool |
| --- | --- |
| `scene_data` | `engine_scene_apply` (place/move/batch/components; `place_marker` for named shot anchors; `stamp_prefabs` for kit prop batches; `stamp_compositions` for primitive Graybox stacks) |
| `terrain_data` | `engine_terrain_apply` (height/paint/foliage) |
| `prefab_asset` | `engine_prefab_apply` / `engine_asset_apply` |
| `lua_script` | `engine_lua_apply` (+ `engine_lua_call` to exercise) |
| `hud_asset` / UI canvas | `engine_hud_apply` / `engine_ui_canvas_mutate` / `engine_ui_stack` |
| `world_forge` | `engine_world_forge_apply` (offline OK) |
| `animation` / weld | `engine_animation_call` (create_clip/create_state/keys/events/weld) |
| `engine_code` | Edit `src/`/`include/` then rebuild (below) |

Direct offline JSON edits to the open world while the editor is live are **rejected by design**.

## 2. Live bridge

1. If no editor window: launch `build/windows-msvc-debug/dev-next/engine.exe editor --project samples/open-world-rpg` (MCP proxy alone is not enough).
2. `engine_editor_status` — confirm `editorRunning: true` and live automation.
3. If bridge off: `engine_editor_live` `action=enable` (editor must already be running to consume the request).
4. Prefer `engine_editor_ui_query` + `engine_editor_input` over CV for UI clicks.
5. `engine_editor_screenshot` → project `out/` for visual proof.


## 3. Common mutate patterns

- **Batch place/move:** `engine_scene_apply` `action: batch` with `ops[]`; optional `snapToTerrain` / `groundOffset`.
- **Terrain + grass:** paint `grass.material.json` before grass foliage; never grass on water/moat ([`grass-foliage-on-grass-terrain.mdc`](../../.cursor/rules/grass-foliage-on-grass-terrain.mdc)).
- **Heavy terrain/scene recipes:** `engine_job_call` `kind=submit` with `ops[]` (`target` terrain|scene|water), then `kind=wait` — do **not** pack dozens of sculpt ops into one sync `engine_terrain_apply` batch (60s bridge timeout / editor stall).
- **No `bush_wide`:** use `bush` / `bush_tall` ([`no-bush-wide-asset.mdc`](../../.cursor/rules/no-bush-wide-asset.mdc)).
- **Particles:** `engine_asset_apply` `kind: particle` then attach on prefab `particles[]`.
- **UI theme:** `engine_asset_apply` `kind: ui_theme` on `assets/ui/ui-theme.json` (tokens/roles). Widget `themeRole` via `engine_ui_canvas_mutate` `style`.
- **Validate:** `engine_project_validate` after content batches.
- **Animation Studio:** `engine_animation_call` only when live — **not** Python/`*.anim.json` disk rewrites ([`animation-studio-mcp-first.mdc`](../../.cursor/rules/animation-studio-mcp-first.mdc)). Craft quality: [`author-character-animation`](../author-character-animation/SKILL.md) + [`animation-craft.md`](../../context/art/animation-craft.md). Workflow: `open` → `set_state` → `edit_clip` → optional **`set_duration`** → `upsert_key` / `upsert_keys` / **`offset_keys`** / **`shift_keys`** / **`set_pose`** / **`ease_segment`** → inspect with **`list_keys`** / **`sample_pose`** / **`diff_pose`** / **`sample_series`** / **`held_tip_series`** / **`onion_skin`** / **`loop_report`** / **`seek_times`** (`tipTrail`) → frame with **`camera_orbit`** (`slash_review` for melee) → **`save_override`** → `seek`/`play` to verify; `set_held` + `set_weld` / **`inspect_weld`** + `save_weld` for grips; events via `*_event` / `save_events`. Pass `joint` per key (overrides Studio selection). Rotation may use `eulerDeg`. If an MCP animation op is missing or wrong, **fix the engine path** (rebuild lease + reset); do not permanently work around with scripts.

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

5. Relaunch **both** halves (typical):

```text
build\windows-msvc-debug\dev-next\engine.exe editor --project samples/open-world-rpg
rem Cursor respawns mcp on next tool call; if Not connected, reconnect MCP / mcp_auth
```

Confirm `engine_editor_status` → `editorRunning: true` before live work.

6. `engine_build_coordination` `release` with the lease token.
7. Mention reset + lease release in the final response. A C++ change is not ready for handoff until this loop completes, unless a documented supported hot reload was used and verified instead.

```text
❌ Leave LNK1168 / ask user to close editor
❌ Start MSBuild while another agent holds the lease
❌ Relaunch only mcp and treat “not connected” as an MCP bug
✅ acquire → kill → rebuild → relaunch editor + MCP → status live → release
```


## 5. Play-test automation

| Need | Tool |
| --- | --- |
| Quest / flags / standing | `engine_quest_call` / `engine_flag_call` / `engine_standing_call` |
| Co-op | `engine_coop_call` |
| Lua handlers | `engine_lua_call` |

## Done bar

Status checked; correct apply surface used; visual or validate evidence when relevant; rebuild lease released if acquired.
