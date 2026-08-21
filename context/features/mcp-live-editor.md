# MCP Live Editor Automation

Status: active

The engine exposes a native Model Context Protocol (MCP) stdio server and a Windows named-pipe bridge for live editor edits. The bridge starts enabled for editor sessions; it can be disabled from Diagnostics when needed.

## Process split (required for live tools)

| Process | Command | Role |
| --- | --- | --- |
| **Editor** | `engine editor --project <project-dir>` | Windowed session; owns scene/Animation Studio; hosts the named-pipe bridge |
| **MCP proxy** | `engine mcp --project <project-dir>` | Windowless stdio server Cursor launches; forwards tools to the editor bridge |

`engine_editor_status` with `editorRunning: false` almost always means the **editor was not started** (or was killed and only `mcp` came back). Do not treat that as a missing MCP schema. After rebuilds, relaunch the editor as well as MCP — see [`.cursor/rules/reset-editor-after-edits.mdc`](../../.cursor/rules/reset-editor-after-edits.mdc).

## MCP server

```text
engine mcp --project <project-dir>
```


Tools:

- `engine_editor_status` — live automation state, world path, selection, dirty flag, play session, viewport tab, World Forge map layer/frame fields when live
- `engine_editor_live` — **enable/disable/status** for the live bridge without the Diagnostics checkbox (writes `samples/.../.engine/live-automation.request`; editor consumes next frame). Works offline.
- `engine_editor_screenshot` — capture editor window PNG under project `out/` (requires live bridge)
- `engine_editor_input` — queue mouse/keyboard UI input (`move`/`click`/`drag`/`scroll`/`key`/`wait`/`clear`/`unlock_tab`; `x/y`, `nx/ny`, or `targetId` from ui_query; optional `steps[]`). Yellow MCP cursor overlay. Requires live bridge.
- `engine_editor_session` — play-test session without toolbar clicks (`kind`: `status` | `start` | `end` | `pause` | `resume` | `set_overlays`). Optional overlay bools: `showEventZones`, `showCollisionDebug`, `showWorldForgeMapMarkers`. Prefer `end` before Scene camera framing for article stills. Requires live bridge.
- `engine_editor_camera` — Scene DebugCamera framing (`action`: `status` | `set_pose` | `look_at` | `focus_entity` | `select_entity` | `deselect`). `focus_entity` / `look_at` take `entityId` or unique `name` (+ `distance`/`height`, default 8/3; `yawOffsetDegrees` to orbit; `select=false` clears gizmos). `set_pose` uses `x/y/z` + `yawDegrees`/`pitchDegrees` (or radian `yaw`/`pitch`). Switches to Scene tab. Survives play-test end when framed during a session. Requires live bridge.
- `engine_editor_ui_query` — list named widget hotspots (`id` / `idPrefix` / `contains`) with client rect + center + `nx`/`ny` from the last drawn frame. Prefer over CV for clicking. Requires live bridge.
- `engine_world_forge_map_view` — open World Forge Map Canvas; set cartography/frame/worldMap/zoom/pan/layerId/`lockTab` (requires live bridge)
- `engine_scene_plan` — classify whether work belongs in scene data, prefabs, Lua, terrain, or C++ engine code (see `context/architecture/content-vs-engine-workflows.md`)
- `engine_scene_apply` — place, move, remove, rename, duplicate, undo, redo, save, `sample_terrain` through `CommandHistory`; `place_marker` creates a reproducible named marker using the camera-marker prefab; `stamp_prefabs` expands compact `stamps[]` (`prefab`, `name`, `x`/`y`/`z` or `transform`, terrain snap by default) into one undoable batch; `stamp_scatter` densifies `prefabs[]` (or `prefab`) inside `minX/maxX/minZ/maxZ` or `x/z/radius` with `count`/`minSpacing`/`namePrefix`/`seed`/`clearRadius` (batch cap **512**); `stamp_compositions` synthesizes schema-v2 Graybox prefabs from `stamps[].parts[]` (`cube`/`pyramid`/`cylinder`/`sphere`/`capsule` + local transform/color; **neutral gray default when color omitted**) under `assets/prefabs/Graybox/` then places them in one undoable batch (undo removes placements; generated prefabs remain project content); `list`/`query` returns live entities (`namePrefix`/`contains`/`name`/`limit`, plus `terrainY`/`floatGap`; filter with `floatGapMin`/`floatGapMax`); `snap_to_terrain` bulk-grounds entities (`names`/`namePrefix`/`all`, `groundOffset`/`groundOffsets`/`groundOffsetsByPrefab`/`usePrefabGroundDefaults`, default skip for flame/torch/smoke/camera on bulk); `move`/`remove`/`rename` accept `entityId` **or unique `name`** (`rename` by name needs `newName`); `action: batch` with `ops[]` remains the arbitrary multi-edit path (optional `label`, `save`); `snapToTerrain` / `groundOffset` on place and move; component actions `add_component` / `remove_component` / `set_component`
- `engine_terrain_apply` — raise/lower/flatten/`set_height`/`plateau`/`smooth`/`terrace`, smart sculpt (`gentle_hill`/`steep_cliff`/`flatten_pad` with soft `skirtWidth`/`smooth_natural`/`canyon`), `carve_channel`/`raise_banks`, road/path grade via `set_height_along`/`grade_along` (continuous **strip** grade with lateral `halfWidth` + `skirtWidth`, not overlapping circles; optional `smoothPasses`) and `smooth_along`, paint/`paint_along`, foliage density (`paint_foliage` / `paint_foliage_mixed`), sample, undo/redo (`kind`: height/paint/foliage), save, and `action: batch` with `ops[]` including `sample`, smart sculpt recipes, polyline channel/grade/smooth_along/paint_along/smooth/terrace/plateau (one undo per height/paint/foliage group and one reload; `samplesJson` when batching samples) through the same stores as the Sculpt tab ([DEC-0018](../decisions/index.md#dec-0018-mcp-terrain-sculpt-and-paint-apply)); requires live editor MCP for mutate/save (`sample` works offline). **Heavy multi-op polish:** prefer `engine_job_call` (below) over one giant sync batch — bridge timeout is 60s.
- `engine_job_call` — async heavy recipes (TICKET-0279): `kind` `submit` | `status` | `wait` | `cancel` | `list`. `submit` takes `ops[]` of `{target:terrain|scene|water, action, ...}` (same fields as the apply tools), optional `label` / `opsPerFrame` (1–8, default 1); returns `jobId` immediately. Editor runs one (or few) ops per frame so the UI/bridge stay alive. `wait` polls until `succeeded`/`failed`/`cancelled` (`timeoutSeconds`, default 600). Requires live editor MCP.
- `engine_water_apply` — place/erase/`place_along`, sample, undo/redo, save, `batch` (ops may include `place_along`); water place clears foliage under the brush by default (`eraseFoliage: false` to keep)
- `engine_entity_component_apply` — dedicated add/remove/set component on a scene entity (same commands as scene apply)
- `engine_prefab_apply` — create or update prefab JSON, validate, refresh asset browser catalog; prefab writes propagate components to non-overridden instances
- `engine_prefab_component_apply` — dedicated prefab component write path (same as prefab apply with kind prefab)
- `engine_asset_apply` — create or update prefab, material, particle (`*.particle.json`), or UI theme (`assets/ui/ui-theme.json`, `kind: ui_theme`) JSON; validate; refresh catalog / register particle emitters / hot-reload theme tokens; `action: refresh_catalog` rescans without writing
- `engine_asset_bake` — named Blockbench bake (TICKET-0245): `action` `list`|`bake`, `target` id from catalog, optional `source`; runs `tools/asset_bake.py` offline with fail-closed `ASSET-BAKE-*` gates; queues mesh hot-reload when the editor is live
- `engine_lua_apply` — write Lua script assets and hot reload when live automation is enabled
- `engine_lua_call` — dispatch a live Lua handler without physical overlap (`kind`: `interaction` | `combatHurt` | `handler`; binding `id` or `handler` name; optional `payload`). Requires live editor MCP. Agent-friendly for play-test and automated checks.
- `engine_quest_call` — drive session `QuestRuntime` (`kind`: `start` | `complete_objective` | `abandon` | `resolve_fork` | `status` | `list`; `questId`; `objectiveId` for complete; `forkId` + `outcomeFlag` for resolve_fork). Same path as Lua `engine.quest_*` ([DEC-0028](../decisions/index.md#dec-0028-explicit-quest-progression-runtime) / [DEC-0046](../decisions/index.md#dec-0046-session-story-flag-runtime)). Requires live editor MCP; allowed during play test.
- `engine_flag_call` — drive session `FlagRuntime` (`kind`: `set` | `clear` | `has` | `list`; `flagId` except list). Same path as Lua `engine.flag_*` ([DEC-0046](../decisions/index.md#dec-0046-session-story-flag-runtime)). Requires live editor MCP; allowed during play test.
- `engine_coop_call` — local co-op play-test automation (`kind`: `status` | `start_local` | `end` | `pause` | `resume` | `possess` | `move` | `jump` | `disconnect_guest` | `reconnect_guest`). Possess host/guest, inject camera-relative wish for N frames, simulate guest drop/rejoin. Requires live editor MCP; Game tab auto-selected for move/jump ([`co-op-sessions.md`](co-op-sessions.md)).
- `engine_standing_call` — drive session `StandingRuntime` (`kind`: `get` | `set` | `adjust` | `rank` | `meets` | `lock_in` | `list`; `factionId`; `score` / `delta` / `minScore` / `minRankId` as needed). Same path as Lua `engine.standing_*` ([DEC-0029](../decisions/index.md#dec-0029-continuous-faction-standing-with-hostility-transfer)). Requires live editor MCP; allowed during play test.
- `engine_pathfinding_call` — streamed nav-grid queries for agents (`kind`: `status` | `find_path` | `nearest_walkable` | `line_of_walk`; `from`/`to`/`query` as `{x,y,z}`). Uses live editor field when connected (sculpt edits apply); offline builds a temporary field. See [`navigation-grid.md`](navigation-grid.md).
- `engine_animation_call` — Animation Studio + weld authoring (`kind`: `status` | `open` | `set_subject` | `set_controller` | `set_state` | `play`/`pause`/`stop`/`step`/`seek` | `set_joint` | `set_bone_gizmo` | **`create_clip`** | **`create_state`** | `edit_clip` | `upsert_key` | `delete_key` | `save_override` | `sync_gltf` | `replace_from_source` | timeline event CRUD | `set_held` | **`set_armor`** (`slot` head/chest/legs + `itemId`) | `get_weld`/`set_weld`/`save_weld`). Solo preview (no exitTime graph jumps). Requires live editor MCP ([`animation-studio.md`](animation-studio.md)).
- `engine_hud_apply` — write UI canvas (`*.uicanvas.json`) or legacy HUD (`*.hud.json`) and hot reload during play test ([DEC-0025](../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp))
- `engine_world_forge_apply` — read/validate/write World Forge assets (`factions` / `relationships` / `map` `*.worldforge.json`); works offline; not Scene/Sculpt ([TICKET-0014](../planning/tickets/TICKET-0014.md))
- `engine_ui_stack` — canvas stack `register` / `push` / `pop` / `show` / `hide` / `clear` / `status` (play-test safe; equals Lua `engine.ui_*`)
- `engine_ui_canvas_mutate` — structural canvas edits (`add`/`remove`/`move`/`resize`/`style`); play-test safe
- `engine_project_validate` — run the existing validation command path
- `engine_project_git` — authoring sync via system git (`status`/`fetch`/`pull`/`commit`/`push`); offline; OS credentials ([DEC-0037](../decisions/index.md#dec-0037-git-backed-authoring-sync-in-editor), [`../formats/project-git-sync.md`](../formats/project-git-sync.md))
- `engine_build_coordination` — same-machine agent rebuild lease (`status`/`acquire`/`wait`/`release`/`heartbeat`/`clear-stale`); offline; ticket validated against `epics.md` (TICKET-0228, [`agent-build-coordination.md`](agent-build-coordination.md))

Cursor configuration: `.cursor/mcp.json` launches `build/windows-msvc-debug/dev-next/engine.exe mcp --project samples/open-world-rpg` (windowless proxy). That does **not** start the editor window — launch `engine editor` separately for live bridge tools. Reload / reconnect the MCP server in Cursor after rebuilding `engine` if the proxy dies.

## Live editor bridge

The named pipe is **on by default** for an editor session. Disable or re-enable it from **Diagnostics → MCP connection**, or ask an agent to call `engine_editor_live` with `action=disable` or `action=enable` (writes a project request file the editor polls each frame).

When enabled, the editor opens a project-scoped named pipe and processes one framed JSON request per client connection on the render thread. Scene mutations never bypass undo/redo, collision sync, or in-memory scene authority.

Direct `.world.json` writes while the editor is open are rejected by design. Use `engine_scene_apply` instead.

### Batch scene edits

`place_marker` is supported inside `engine_scene_apply` `action: "batch"` alongside `place`, `move`, `remove`, and `rename`. It uses the camera-marker prefab by default and accepts either `transform` or `x`/`y`/`z` placement shorthand.

`engine_scene_apply` accepts `action: "batch"` with an `ops` array of single-op payloads (`place`, `move`, `remove`, `rename`). All operations run in one bridge round-trip and one undo step. Failed mid-batch applies roll back earlier ops in that batch. Optional `label` names the undo entry; `save: true` persists after a successful batch. Maximum **512** ops per request (raised for forest densify / bulk `snap_to_terrain`). Also: `stamp_scatter` (AABB densify with spacing), `list`/`query` `floatGapMin`/`floatGapMax` float audit (`terrainY`/`floatGap` on every row), `snap_to_terrain` `groundOffsetsByPrefab` / `usePrefabGroundDefaults` (oak/tree −0.9, bush −0.3).

`engine_scene_plan`, `engine_project_validate`, `engine_project_git`, and `engine_build_coordination` work without the bridge. `engine_world_forge_apply` also works offline (file + schema validate). Live scene, prefab, Lua, and HUD apply require the editor plus enabled MCP connection. `engine_lua_apply`, `engine_hud_apply`, `engine_lua_call`, `engine_quest_call`, `engine_flag_call`, `engine_dialogue_call`, `engine_standing_call`, `engine_pathfinding_call`, `engine_coop_call`, `engine_animation_call`, `engine_editor_session`, and `engine_editor_camera` are allowed during play test; **scene placement edits are also allowed live** so Scene free-cam inspection and mid-play tweaks work (terrain/water apply remain blocked while play is active).

Dialogue sandbox pad + MCP scenarios: [`../testing/dialogue-sandbox-mcp.md`](../testing/dialogue-sandbox-mcp.md) (`engine editor … --world worlds/sandbox.world.json`, or **File → Open World → sandbox** in a running editor). Combat melee pad: [`../testing/combat-sandbox.md`](../testing/combat-sandbox.md) (`--world worlds/combat-sandbox.world.json`).

### Modular equipment play-test order

`engine_editor_session` `start` creates a fresh session inventory. For an equipment visual check, prefer **Animation → Armor slots** (studio-only, no bag mutation) so Idle/Walk scrub without F5. For play-test stats, **start the play test first**, confirm `testSession: running`, then use `engine_inventory_call` to `grant` and `set_equip` each item. Equipment written before play-test start is intentionally discarded with the prior session state. Verify an individual slot by selecting `region: "equip"` with its `equipSlot`, calling `unequip_selected`, capturing a screenshot, then restoring it with `set_equip`.

When a modular model is copied from a full player source, it must receive its **own atlas policy**; never reuse the player texture atlas by default. Set `generic.requireDedicatedAtlas: true` and select either `atlasPolicy: "dedicated_embedded"` for a uniquely repainted Blockbench atlas, or `atlasPolicy: "dedicated_material"` plus `flatAtlasRgb` for a temporary coherent material. The named bake fails if a protected modular target omits that policy. Replace the temporary material with a painted dedicated atlas before final art review. Combined Blockbench kits use `generic.keepMeshes` so one `GoodPlayerModelCopy.bbmodel` bakes head / chest / legs without importing the body. Do not apply post-skin instance scale — fit shells in Blockbench, then `engine asset-bake --target iron_test_helmet`.

## UI hotspots (`engine_editor_ui_query`)

Each editor frame registers named widget bounds (ImGui item rects). Query returns `cx`/`cy` client pixels and `nx`/`ny` for `engine_editor_input`.

Common ids:

| Id | Control |
|----|---------|
| `Viewport.Game` | Viewports → Game tab |
| `Toolbar.test_start` | Start Test (F5) |
| `Toolbar.test_pause` / `Toolbar.test_resume` / `Toolbar.test_end` | Play-test controls |
| `WorldForge.Pane.Overview` | World Forge → Overview nav |
| `WorldForge.Pane.Hierarchy` | World Forge → Hierarchy nav |
| `WorldForge.Pane.Map` | World Forge → Map nav |
| `WorldForge.Map.Tool.Select` | Map Canvas Select tool |
| `WorldForge.Map.Tool.Anchor` | Map Canvas Anchor tool |
| `WorldForge.Map.Tool.Route` | Map Canvas Route tool |
| `WorldForge.Map.Tool.Border` | Map Canvas Border tool |
| `WorldForge.Map.Tool.Water` | Map Canvas Water tool |
| `WorldForge.Map.ReturnContinent` | Return to continent plate |
| `WorldForge.Header.Reload` | World Forge header Reload |
| `WorldForge.Map.CanvasMode` | Map → Canvas radio |
| `WorldForge.Map.Cartography` | Cartography radio |
| `WorldForge.Map.Frame` | Frame checkbox |
| `WorldForge.Map.WorldMap` | World map checkbox |
| `WorldForge.Map.Fit` | Fit button |
| `WorldForge.Map.Canvas` | Spatial map draw area |

Example: `engine_world_forge_map_view` → wait a frame → `engine_editor_ui_query` with `idPrefix: "WorldForge.Map"` → `engine_editor_input` with `action: click`, `targetId: "WorldForge.Map.Frame"`.

Screenshots remain for visual QA; do not put a CV stack in the engine.

## Contracts

- Bridge requests/responses use schema version 1 and reuse stable exit codes and diagnostics.
- `editor_status` metadata includes `liveAutomationEnabled`.
- Scene edits map to existing commands in `include/engine/automation/scene_commands.h`.
- Play-test sessions allow Scene free-cam inspect/edit (gizmos, place); terrain/water still blocked until ended.

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
