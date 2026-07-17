# TICKET-0105: Animation events → gameplay/collision hooks

- Epic: EPIC-0008
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: (see Notion Tickets DB by Ticket ID)

## Goal

Controller-authored timeline markers fire during playback and reach Lua so gameplay can react (hit frames, footsteps) without a Lua-authored graph ([DEC-0031](../../decisions/index.md#dec-0031-controller-authored-animation-timeline-events)).

## Context links

- `context/planning/epics.md` (EPIC-0008)
- `context/decisions/index.md` (DEC-0031, DEC-0022)
- `context/formats/animator-controller-assets.md`
- `context/features/animator.md`, `context/features/lua-scripting.md`
- Related: TICKET-0103/0104 (animator + root motion); combat auto-enable deferred

## Acceptance criteria

- [x] `timelineEvents[]` on `*.animator.json` parse/validate/serialize (`state`, `time`, `name`, optional `layer`/`payload`).
- [x] `AnimatorRuntime` fires loop-aware crossings once per cycle; `take_fired_events()` drains them.
- [x] `LuaRuntime::dispatch_animation_event` → optional global `on_animation_event` with JSON payload.
- [x] Invalid event state/layer/time/name fail closed at validate.
- [x] `animator` suite covers fire-once, loop re-fire, and Lua smoke.
- [x] Context docs + DEC-0031 recorded; Status → needs-approval (not done).

## Out of scope

- Auto-enable combat volumes from events (scripts/MCP may).
- glTF extras / per-clip sidecars as the authoring home.
- Visual animator graph UI (TICKET-0135).
- Full play-session animator wiring polish (known leftover from 0103/0104).

## Dependencies

After TICKET-0104; uses DEC-0022 Lua react path.

## Verification

- Rebuild `engine` (MSVC debug).
- CTest / suite `animator` — timeline + Lua assertions.
- MCP: reconnect Cursor MCP after editor kill/rebuild if needed.

## What changed

- Summary: Authors place hit-frame/footstep markers on the animator controller; C++ fires them when state time crosses the marker (loop-aware); hosts drain events and dispatch to Lua `on_animation_event`. Engine does not auto-toggle combat volumes.
- Files / surfaces touched: `animator_controller_asset` (parse/validate/json), `animator_runtime` (evaluate + drain), `lua_runtime` (dispatch), `tests/suite_tests.cpp` (`animator` suite), context formats/features/decisions/ticket/epics.
- Schema / API / format deltas: `timelineEvents[]`; `AnimatorTimelineEvent` / `AnimatorFiredEvent`; `take_fired_events()`; `dispatch_animation_event`; validate codes `ANIM-CTRL-EVENT-*`; Lua handler `on_animation_event` payload `{entityId,name,state,layer,time,payload}`.
- Seed / sample data: none in open-world-rpg (fixture-only in suite).
- Tests / verification evidence: `engine` + `engine_suite_tests` rebuilt (MSVC Debug); `engine_suite_tests --suite animator` → **104/104**. Expected `ANIM-CLIP-MISSING` log from intentional bad-clip fixture.
- Decisions & tradeoffs: DEC-0031 — controller timeline (not glTF extras); missing Lua handler silent; no auto combat enable.
- Leftover risk / follow-ons: play-session still largely wish-velocity move; hosts must call `take_fired_events` + dispatch after tick; combat volume enable remains script-side.

## Agent notes

User chose option 1 (controller timeline). Handed off at needs-approval.
