# TICKET-0221: Event timeline asset + C++ sequencer MVP

- Epic: EPIC-0006
- Status: needs-approval
- Agent: cursor-agent
- Priority: P0
- Notion: https://app.notion.com/p/3a7d3efc56958143ba6dc6e2e4377b00

## Goal

Ship a data-driven event timeline format and headless C++ sequencer so Act 0 theatrical beats can run ordered steps (wait, control lock, dialogue start, emit hooks) without Lua-authored graphs — unlocking Landfall cinematics once camera helpers land ([DEC-0045](../../decisions/index.md#dec-0045-json-event-timelines-with-c-sequencer-world-forge-home)).

## Context links

- [DEC-0045](../../decisions/index.md#dec-0045-json-event-timelines-with-c-sequencer-world-forge-home)
- Follow-ons: TICKET-0222 (camera path + play lock), TICKET-0223 (World Forge Events pane)

## Acceptance criteria

- [x] Format doc `context/formats/world-forge-events.md` for `events.worldforge.json` (schemaVersion 1)
- [x] MVP step kinds: `wait`, `lock_control`, `unlock_control`, `start_dialogue`, `emit` — fail-closed `EVENT-*`
- [x] C++ asset load/validate/save + default path
- [x] `EventTimelineRuntime`: bind/start/tick/cancel; lock flag; drain emits
- [x] `start_dialogue` via injected DialogueRuntime starter
- [x] Sample `evt_act0_timeline_smoke` in samples/open-world-rpg
- [x] Lua `start_event_timeline` / `cancel_event_timeline` / `event_timeline_control_locked` + optional `on_event_timeline_emit`
- [x] `world_forge` suite coverage + project validate
- [x] MVP readiness `coding_event_timeline` → done; feature index + scope docs

## Out of scope

Camera path / play input freeze wiring (TICKET-0222). World Forge Events pane (TICKET-0223). Full prologue content. Particle draw. Animator timeline events.

## Dependencies

Soft: DialogueRuntime (0052). Blocks 0222/0223.

## Verification

- Rebuild `engine` / `engine_suite_tests` — passed (C4996 getenv warning only)
- `engine_suite_tests --suite world_forge` — **239/239** (includes event timeline cases)
- `engine validate --project samples/open-world-rpg` — exit 0

## What changed

### Summary

Act 0 can author and headlessly run JSON event sequences (lock / wait / emit / dialogue / unlock). Lua starts sequences; emit hooks are optional. Camera `look_at` and play-input freeze landed with TICKET-0222 in the same delivery pass.

### Files / surfaces

- `include/engine/assets/world_forge_events_asset.h`, `src/assets/world_forge_events_asset.cpp`
- `include/engine/event/event_timeline_runtime.h`, `src/event/event_timeline_runtime.cpp`
- Sample `events.worldforge.json`; validate hook in `command.cpp`; Lua + editor boot in `lua_runtime` / `render_app`
- Docs: `world-forge-events.md`, `event-timelines.md`, features/lua indexes

### Schema / API

- `events.worldforge.json` sequences/steps; codes `EVENT-*` / `EVENT-RT-*`
- Lua: `engine.start_event_timeline`, `cancel_event_timeline`, `event_timeline_control_locked`; `on_event_timeline_emit`

### Tests / verification evidence

world_forge 239/239; project validate OK.

### Decisions & tradeoffs

DEC-0045. Missing emit handler silent. Dialogue starter injected (not hard-wired).

### Leftover risk / follow-ons

TICKET-0223 WF Events pane. Desktop lock QA noted on 0222.

## Agent notes

Implemented with 0222 in the same session so the Landfall cine gate can open.
