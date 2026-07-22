# TICKET-0107: miniaudio integration + spatial/event playback

- Epic: EPIC-0008
- Status: ready
- Agent: unassigned
- Priority: P1
- Notion: https://app.notion.com/p/39ed3efc569581cb8df1d80820c2ce66

## Goal

Integrate **miniaudio** for spatial/event audio playback so M5’s “audio” roadmap item is met with a permissively licensed, testable foundation.

## Context links

- `context/planning/epics.md` (EPIC-0008)
- [`context/roadmap.md`](../../roadmap.md) — M5 remaining audio
- [`context/resources/index.md`](../../resources/index.md) — record license/provenance
- TICKET-0105 animation events (optional hook for footstep/hit one-shots)

## Acceptance criteria

- [ ] miniaudio vendored or fetched via vcpkg with license recorded (commercial use, modification, redistribution).
- [ ] Engine audio backend: init/shutdown, load/play one-shot and looping sounds, master volume.
- [ ] Spatial listener follows active camera; at least one 3D positioned source API.
- [ ] Sample project: one test sound triggered from existing interaction or animation event path.
- [ ] Named `audio` or extended suite covers init, load, play, fail-closed on missing file.
- [ ] Context docs: feature note + resource provenance.

## Out of scope

- Full FMOD-style event graphs, occlusion, reverb zones, music crossfade
- Combat hit SFX polish pass (M9)
- Editor audio browser

## Dependencies

- After or parallel with TICKET-0110 (P0); do not block 0110.

## Verification

Rebuild `engine`; run audio suite + sample trigger smoke. Set Status → `needs-approval` with license path and suite counts in **What changed**.

## Agent notes

Elevated to **P1 / ready** 2026-07-22: last major M5 gap per roadmap; prefer headless suite evidence over owner mobile QA.
