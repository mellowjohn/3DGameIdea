# World Forge Events (`events.worldforge.json`)

Status: active — TICKET-0221 (runtime) · TICKET-0222 (camera/lock) · TICKET-0223 (editor/MCP) · [DEC-0045](../decisions/index.md#dec-0045-json-event-timelines-with-c-sequencer-world-forge-home)

Ordered theatrical / story-event **sequences** for Act 0 Landfall and later campaign beats. Product home is World Forge ([DEC-0020](../decisions/index.md#dec-0020-world-forge-narrative-tooling-umbrella)). Distinct from animator controller `timelineEvents` ([DEC-0031](../decisions/index.md#dec-0031-controller-authored-animation-timeline-events)).

## Default path

`assets/world-forge/events.worldforge.json`

Helper: `default_world_forge_events_path(project_root)`.

Sample: `samples/open-world-rpg/assets/world-forge/events.worldforge.json` (`evt_act0_timeline_smoke`, `evt_sandbox_zone_pan`).

## Shape

```json
{
  "schemaVersion": 1,
  "id": "tessera_events",
  "sequences": [
    {
      "id": "evt_act0_timeline_smoke",
      "displayName": "Act 0 timeline smoke",
      "canonStatus": "draft",
      "summary": "Headless/play smoke: lock, wait, emit, dialogue, unlock.",
      "acts": ["act0"],
      "steps": [
        { "kind": "lock_control" },
        { "kind": "wait", "seconds": 0.5 },
        { "kind": "emit", "name": "vfx_stub", "payload": {} },
        { "kind": "start_dialogue", "dialogueId": "dlg_act0_wrathful_conquest" },
        { "kind": "unlock_control" }
      ],
      "tags": ["act0", "smoke"]
    }
  ]
}
```

Sandbox playtest sequence `evt_sandbox_zone_pan` is started by the `event_zone` prefab (`interaction: event_sandbox`) placed in `worlds/sandbox.world.json`. It chains three `look_at` subjects (practice NPC → campfire → oak) so desktop play can verify the camera leaving the player for cinematic pans/zooms.

Camera steps (`look_at`) land in TICKET-0222 — **shipped** (orbit pivot relocates to `target` while control is locked).

## MVP step kinds

| `kind` | Fields | Notes |
| --- | --- | --- |
| `wait` | `seconds` ≥ 0 | Headless-friendly |
| `lock_control` | — | Sets runtime control-lock flag |
| `unlock_control` | — | Clears flag; restores player orbit follow |
| `start_dialogue` | `dialogueId` | Existing DialogueRuntime tree id |
| `emit` | `name`, optional `payload` | Host/Lua hook (VFX stub until particle MVP) |
| `look_at` | `target` [x,y,z], `seconds`, optional `distance` / `pitch` | Relocates orbit **pivot** to world subject (cinematic focus); blends yaw/pitch/distance. Pivot holds between shots while control stays locked. |

## Runtime

C++ `EventTimelineRuntime`: bind asset → `start(sequenceId)` → `tick(dt)` → complete/cancel. Inject `set_dialogue_starter` for `start_dialogue`. Lua may start/cancel; does not author steps.

## Validation codes

| Code | Meaning |
| --- | --- |
| `EVENT-SCHEMA` | schemaVersion ≠ 1 |
| `EVENT-ID` / `EVENT-ID-DUP` | Missing or duplicate sequence id |
| `EVENT-STEP-KIND` | Unknown step kind |
| `EVENT-STEP-WAIT` | Invalid wait seconds |
| `EVENT-STEP-DIALOGUE` | start_dialogue missing dialogueId |
| `EVENT-STEP-EMIT` | emit missing name |
| `EVENT-DIALOGUE-MISSING` | dialogueId not in known dialogues set |
| `EVENT-PARSE` / `EVENT-READ` / `EVENT-IO` | IO / JSON failures |
| `EVENT-RT-*` | Runtime bind/start/dialogue starter failures |

Missing events file: `validate_file` succeeds (optional asset).

## Editor / MCP

- World Forge **Events** pane: list/detail sequences + step editor (Act lens on `acts[]` / tags).
- MCP: `engine_world_forge_apply` `kind=events` get | validate | apply ([`world-forge-mcp.md`](world-forge-mcp.md)).

## Related

- Feature: [`../features/event-timelines.md`](../features/event-timelines.md)
- MVP readiness: `coding_event_timeline`, `coding_camera_path`, `cine_event_timeline_ready`
- [`world-forge-dialogues.md`](world-forge-dialogues.md)
- [`../features/world-forge-scope.md`](../features/world-forge-scope.md) — story events
