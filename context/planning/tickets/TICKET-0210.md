# TICKET-0210: Authored AudioSource schema + Add Component dropdown

- Epic: EPIC-0008
- Status: needs-approval
- Agent: cursor-agent
- Priority: P1
- Notion: https://app.notion.com/p/3a5d3efc5695816b8551feb9ed211a0c

## Goal

Ship a Unity-like authored **`audioSource`** component (prefab + scene entity) with Inspector/Prefab Editor fields, and replace the row of Add Component buttons with a type dropdown + single Add action — without yet wiring runtime playback from the component.

## Context links

- [`../../features/audio.md`](../../features/audio.md) (TICKET-0107 session AudioEngine)
- [`../../architecture/components.md`](../../architecture/components.md)
- [`../../formats/prefab-assets.md`](../../formats/prefab-assets.md)
- DEC-0016 / DEC-0017 component authoring
- Follow-on: TICKET-0211 (runtime playOnStart + Lua entity trigger)

## Acceptance criteria

- [x] `AuthoredComponentType` includes `AudioSource` / JSON `"audioSource"`.
- [x] Payload fields validated: `clip` (project-relative `.wav`), `volume` [0,1], `loop`, `spatial`, `playOnStart`, `minDistance`, `maxDistance` (max ≥ min > 0).
- [x] Prefab load/save under `components[]`; seed/propagate/inherit like Rigidbody.
- [x] Scene Inspector: single **Add Component** combo (Collider / Script / Animator / Rigidbody / Audio Source) instead of separate buttons.
- [x] Prefab Editor: same combo pattern; Audio Source edit/remove.
- [x] Clip field uses dropdown of scanned `.wav` assets (stale value kept selectable).
- [x] Docs: `components.md`, `prefab-assets.md`, `audio.md` note 0210 vs 0211.
- [x] Suites cover prefab parse, seed, JSON round-trip, and fail-closed validation.

## Out of scope

- Runtime `playOnStart` / entity spatial play (TICKET-0211)
- Lua `play_entity_audio` (TICKET-0211)
- Mixer, occlusion, music, editor audio browser
- Migrating campfire script off `play_sound` (optional later)

## Dependencies

- Soft: TICKET-0107 AudioEngine
- Blocks: TICKET-0211

## Verification

Rebuild `engine` + `engine_suite_tests` (Debug). Run `engine test --suite animator` (includes audioSource schema checks) and `engine test --suite audio`. Desktop: Inspector Add Component combo shows Audio Source; add/edit/remove persists on save.

## What changed

**Summary.** Entities and prefabs can author an `audioSource` component with clip/volume/loop/spatial/playOnStart/distances. Scene Inspector and Prefab Editor use one Add Component dropdown (including Audio Source). Playback from the component is deferred to TICKET-0211; Lua `play_sound*` still works.

**Files / surfaces touched.**
- `include/engine/world/authored_components.h`, `src/world/authored_components.cpp`
- `include/engine/assets/prefab_asset.h`, `src/assets/prefab_asset.cpp`
- `src/rendering/render_app.cpp` — Inspector + Prefab Editor UI
- Docs: `components.md`, `prefab-assets.md`, `audio.md`, `epics.md`
- Tests: `tests/suite_tests.cpp`

**Schema / API / format deltas.** New type `"audioSource"`; errors `COMPONENT-AUDIO-*`, `PREFAB-AUDIO-*`.

**Seed / sample data.** None required; default clip prefers first scanned `.wav` or `assets/audio/campfire_crackle.wav`.

**Tests / verification evidence.** Rebuild `engine` + `engine_suite_tests` Debug succeeded. `engine test --suite animator` pass; `engine test --suite audio` pass.

**Decisions & tradeoffs.** Trigger model C (playOnStart + Lua) chosen; this ticket is schema/UI only (scope 2).

**Leftover risk / follow-ons.** Authored AudioSource is inert at runtime until 0211.

## Agent notes

Owner chose trigger C + scope 2 (2026-07-22). Implemented on PR #8 branch with 0107 desktop fixes.
