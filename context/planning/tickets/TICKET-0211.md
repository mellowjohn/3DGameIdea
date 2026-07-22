# TICKET-0211: AudioSource runtime playOnStart + Lua entity trigger

- Epic: EPIC-0008
- Status: ready
- Agent: unassigned
- Priority: P1
- Notion: https://app.notion.com/p/3a5d3efc569581e7a32bddf5c58ae32e

## Goal

Make authored **AudioSource** components playable at runtime: `playOnStart` when a placement becomes active in play/test, and Lua/API to trigger a specific entity AudioSource — so designers can attach SFX to game objects without hard-coding paths in scripts.

## Context links

- TICKET-0210 (schema + inspector — must be merged/approved first)
- TICKET-0107 (`AudioEngine`, `play_sound` / `play_sound_at`)
- [`../../features/audio.md`](../../features/audio.md)
- [`../../architecture/components.md`](../../architecture/components.md)

## Acceptance criteria

- [ ] On play/test start (or placement activate), each AudioSource with `playOnStart` starts via `AudioEngine` using entity world position when `spatial` is true.
- [ ] Volume / loop / minDistance / maxDistance applied from the component.
- [ ] Lua API to play an entity’s AudioSource by entity id (and optional component id); fail-closed if missing/uninitialized.
- [ ] Named suite coverage (extend `audio` or add assertions): playOnStart path headless (`no_device`), Lua trigger, missing component fails closed.
- [ ] Docs updated: `audio.md` + components runtime row; sample optional (campfire may keep script `play_sound` or migrate).
- [ ] Desktop QA: place entity with AudioSource + playOnStart, enter play — hear clip; trigger via Lua/script path.

## Out of scope

- Mixer buses, occlusion, music crossfade
- Animation-event → AudioSource wiring (can use existing event → Lua)
- Editor preview scrubber / waveform UI

## Dependencies

- Blocked by: TICKET-0210
- Soft: TICKET-0107

## Verification

```powershell
engine test --project samples/open-world-rpg --suite audio
# Desktop: editor play-test AudioSource playOnStart + Lua trigger
```

## What changed

(Required before Status → `needs-approval`.)

## Agent notes

Owner chose trigger model C (playOnStart + Lua) with schema-first delivery (0210).
