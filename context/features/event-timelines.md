# Event timelines / theatrical sequences

Status: active — TICKET-0221 · [DEC-0045](../decisions/index.md#dec-0045-json-event-timelines-with-c-sequencer-world-forge-home)

Data-driven **event timeline sequences** for Act 0 Landfall theatrical beats (and later campaign events). Authored as `events.worldforge.json`; executed by C++ `EventTimelineRuntime`. Lua may start/cancel sequences and optionally handle `on_event_timeline_emit`. Distinct from animator controller `timelineEvents` ([DEC-0031](../decisions/index.md#dec-0031-controller-authored-animation-timeline-events)).

## Format

[`../formats/world-forge-events.md`](../formats/world-forge-events.md)

## Runtime

- `EventTimelineRuntime::bind` → `start(sequenceId)` → `tick(dt)` → complete / `cancel`
- Step kinds (v1): `wait`, `lock_control`, `unlock_control`, `start_dialogue`, `emit`, `look_at`
- `control_locked()` freezes play locomotion + look (TICKET-0222)
- `camera_directive()` during `look_at` relocates the orbit **pivot** to `target` (subject focus — leaves the player), blending yaw/pitch/distance from the shot-start pose with smoothstep easing; held while control stays locked so multi-shot sequences do not snap back between looks
- `take_emitted_events()` drained each frame to Lua `on_event_timeline_emit` (missing handler silent)

## Lua

- `engine.start_event_timeline(sequenceId)`
- `engine.cancel_event_timeline()`
- `engine.event_timeline_control_locked()` → bool
- Optional global `on_event_timeline_emit(payload)` with `{sequenceId,name,payload}`

## Samples

- Headless/MCP smoke: `evt_act0_timeline_smoke` via `assets/scripts/timeline_smoke.lua`
- **Sandbox cinematic playtest:** walk into `event_zone_sandbox` (blue pad southwest of spawn) → `event_sandbox` → `evt_sandbox_zone_pan`: lock → look_at practice NPC → campfire → oak (distinct distances/pitch) → `dlg_sandbox_event_zone` → unlock (return to player follow). Separate from practice-keeper `talk_sandbox` / `dlg_sandbox_sample`. Re-enter after the beat finishes to run again.

## Editor

Diagnostics **Show event zones** (off by default) draws authored interaction/event trigger volumes in violet with interaction-id labels on **Scene** / **Sculpt** only (hidden on Game) — independent of full **Show collision debug**.

## Follow-ons

- TICKET-0222 — camera path steps + play-session input lock
- TICKET-0223 — World Forge Events pane + MCP `kind=events`
