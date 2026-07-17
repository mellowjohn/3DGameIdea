# Lua Scripting

Status: active (host API v1)

Lua provides gameplay logic that can change without recompiling the C++ engine after the native runtime is built. Live edit + hot reload is the primary iteration path for systems expressible in script ([DEC-0023](../decisions/index.md#dec-0023-live-lua-host-api-agent-iteration-path)).

## Scope in v1

- Sandboxed Lua VM with `require`, `dofile`, and `loadfile` disabled
- Versioned `bindings.script.json` maps interaction and combat IDs to global handler functions
- Script assets under `assets/scripts/*.lua`
- Validate-then-load and hot reload through the editor bridge and `AssetMonitor`
- Interaction enter/exit and combat hurt contact dispatch to Lua handlers
- Sandboxed **`engine.*` host API v1**: `log`, `json_decode`, blackboard get/set

Not included yet: inventory, stats, save migration, movement APIs, damage/audio/VFX, collision-query bindings, or broad engine API exposure. **Quest progression** is available via `engine.quest_*` ([DEC-0028](../decisions/index.md#dec-0028-explicit-quest-progression-runtime) / TICKET-0180). **Animator drive** is available via `engine.animator_*` ([DEC-0022](../decisions/index.md#dec-0022-c-animator-backend-with-lua-drive-api) / [`animator.md`](animator.md) / TICKET-0103) — Lua sets params / requests states; it does not own the transition graph. Runtime movement and physics (for example player jump) belong in C++ until bindings are added; see `context/architecture/content-vs-engine-workflows.md`.

## Live agent / content loop

```text
edit .lua (MCP engine_lua_apply or disk)
  → validate-then-reload into the live VM
  → C++ fires interaction/combat events
     (or MCP engine_lua_call synthesizes them)
  → handlers call engine.* (no C++ rebuild)
```

### MCP script dispatch (`engine_lua_call`)

Agents can fire bound handlers without walking into volumes:

| kind | Required | Example |
| --- | --- | --- |
| `interaction` | `id` (+ optional `type` enter/exit) | `{"kind":"interaction","id":"use_campfire","type":"enter"}` |
| `combatHurt` | `id` | `{"kind":"combatHurt","id":"body"}` |
| `handler` | `handler` (+ optional `payload`) | `{"kind":"handler","handler":"on_body_hit","payload":{...}}` |

Requires a running editor with MCP connection. Uses the same dispatch path as gameplay events.

- **Rebuild C++** only when adding new host bindings, event kinds, loaders, or simulation/render changes.
- **No rebuild** for handler body edits, Lua helpers in already-loaded scripts, or rules expressible through `engine.*`.
- Enable **MCP connection** in Diagnostics for live `engine_lua_apply` hot reload.
- Offline/disk: `ScriptFileMonitor` reloads changed `*.lua` files.
- **Friction:** new IDs in `bindings.script.json` still require an editor restart (bindings JSON is not hot-reloaded yet).

## Host API v1

| Lua | Behavior |
| --- | --- |
| `engine.log(level, message)` | Structured log via subsystem `"lua"`; levels `debug`/`info`/`warn`/`error`. Invalid level is ignored (warning logged); VM is not aborted. |
| `engine.json_decode(json_string)` | Returns a Lua table, or `nil, err` on failure. |
| `engine.blackboard_set(key, value)` | Key = non-empty string; value = bool, number, or string. |
| `engine.blackboard_get(key)` | Stored value or `nil`. |
| `engine.hud_set_number(bind, value)` | HUD numeric bind ([DEC-0024](../decisions/index.md#dec-0024-mcp-hud-toolkit-with-lua-value-binds)) |
| `engine.hud_set_text(bind, text)` | HUD text bind |
| `engine.hud_set_visible(widget_id, bool)` | Show/hide widget by id |
| `engine.set_health(current, max)` | Updates `player.health` / `player.healthMax` / `player.healthText` |
| `engine.get_health()` | Returns `current, max` |
| `engine.ui_push/show/hide(id)` / `ui_pop()` / `ui_top()` | Canvas stack ([DEC-0025](../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp)) |
| `engine.quest_start(questId)` | Start quest ([DEC-0028](../decisions/index.md#dec-0028-explicit-quest-progression-runtime)) |
| `engine.quest_complete_objective(questId, objectiveId)` | Complete current objective only |
| `engine.quest_abandon(questId)` | Abandon active quest |
| `engine.quest_status(questId)` | Table: status, currentObjectiveId/Summary, completedObjectiveIds |
| `engine.quest_dialogue_hook(questId, stage)` | Lookup tree id (`start`/`current`/`complete`/`abandon`); does not advance |
| `engine.animator_set_float(entityId, name, value)` | Drive float param ([DEC-0022](../decisions/index.md#dec-0022-c-animator-backend-with-lua-drive-api)) |
| `engine.animator_set_bool(entityId, name, value)` | Drive bool param |
| `engine.animator_set_trigger(entityId, name)` | Arm trigger (consumed on matching transition) |
| `engine.animator_crossfade(entityId, state[, duration[, layer]])` | Request state (default duration `0.15`) |
| `engine.animator_get_state(entityId[, layer])` | Current state name |

Blackboard is a per-runtime scratchpad for flags, counters, and last-event IDs. Layout lives in `*.uicanvas.json` ([`ui-canvas.md`](ui-canvas.md)); Lua pushes values and stack ops. Quest state is session-only until TICKET-0114; HUD bind `quest.objectiveText` mirrors the primary active objective. Animator instances must be attached in C++ (`AnimatorRuntime::attach`) before Lua drive calls succeed.

## Handler contract

Handlers are global functions called with one JSON string argument.

Interaction payload fields: `type`, `interactionId`, `placementEntityId`, `interactorId`, `volumeIndex`.

Combat hurt payload fields: `attackerId`, `hurtPlacementEntityId`, `hurtCombatId`, `hurtVolumeIndex`.

Animation event payload fields ([DEC-0031](../decisions/index.md#dec-0031-controller-authored-animation-timeline-events)): `entityId`, `name`, `state`, `layer`, `time`, `payload` (object). Global `on_animation_event` is optional — missing handler is silent.

Decode with `engine.json_decode` when structured fields are needed.

## Sample project

- `samples/open-world-rpg/assets/scripts/bindings.script.json`
- `samples/open-world-rpg/assets/scripts/campfire_interaction.lua`
- `samples/open-world-rpg/assets/scripts/combat_hurt.lua`

See `context/formats/script-assets.md` for the binding file schema.

## Verification

- `scripting` suite in `tests/suite_tests.cpp`
- Ticket: [TICKET-0152](../planning/tickets/TICKET-0152.md)
