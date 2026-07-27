# TICKET-0222: Camera path + control lock for timelines

- Epic: EPIC-0006
- Status: needs-approval
- Agent: cursor-agent
- Priority: P0
- Notion: https://app.notion.com/p/3a7d3efc56958184bfd2d6a6008ddad1

## Goal

Add camera path / look-at helpers and wire play-session control lock so event timelines can drive staged Act 0 theatrical beats — marking the Landfall `cine_event_timeline_ready` gate once a sample sequence proves camera + dialogue + emit + lock together ([DEC-0045](../../decisions/index.md#dec-0045-json-event-timelines-with-c-sequencer-world-forge-home)).

## Acceptance criteria

- [x] `look_at` step kind (target [x,y,z], seconds, optional distance/pitch); fail-closed `EVENT-CAM-*`
- [x] Play-test: `lock_control` ignores locomotion + look; `unlock_control` restores
- [x] Sample smoke includes look_at + lock + emit + dialogue + unlock
- [x] Suite: look_at blend alpha + smoke walk; cancel unlocks
- [x] MVP readiness `coding_camera_path` + `cine_event_timeline_ready` → done
- [x] Format/feature docs updated

## Out of scope

Cinematic chrome UI. Multi-track editor. Real particles. WF Events pane (0223).

## Dependencies

Blocked by / paired with TICKET-0221.

## Verification

- Rebuild engine + suite — passed
- `world_forge` **239/239**
- `engine validate --project samples/open-world-rpg` — exit 0
- **Desktop QA remaining:** Start Test → `engine.start_event_timeline("evt_act0_timeline_smoke")` → confirm WASD frozen during lock and camera blends (owner desktop session)

## What changed

### Summary

Timelines can blend the orbit camera toward a world look-at and freeze play locomotion/look while `lock_control` is active. Sample smoke + MVP cine gate marked done.

### Files / surfaces

- `look_at` on events asset + `EventTimelineCameraDirective` on runtime
- `OrbitCamera::set_desired_distance`; play-loop apply + move gates in `render_app.cpp`
- Sample + suite + docs

### Schema / API

- Step `look_at`: `target`, `seconds`, optional `distance` / `pitch`
- Codes `EVENT-CAM-TARGET`, `EVENT-CAM-DURATION`

### Leftover risk / follow-ons

Desktop play-test confirmation; TICKET-0223 authoring UI.

## Agent notes

Shipped with 0221 so Act 0 cinematic content can proceed.
