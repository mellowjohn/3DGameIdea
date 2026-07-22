# TICKET-0107: miniaudio integration + spatial/event playback

- Epic: EPIC-0008
- Status: needs-approval
- Agent: cursor-agent
- Priority: P1
- Notion: https://app.notion.com/p/39ed3efc569581cb8df1d80820c2ce66

## Goal

Integrate **miniaudio** for spatial/event audio playback so M5’s “audio” roadmap item is met with a permissively licensed, testable foundation.

## Context links

- `context/planning/epics.md` (EPIC-0008)
- [`context/roadmap.md`](../../roadmap.md) — M5 remaining audio
- [`context/resources/index.md`](../../resources/index.md) — record license/provenance
- [`context/features/audio.md`](../../features/audio.md)
- TICKET-0105 animation events (optional hook for footstep/hit one-shots)

## Acceptance criteria

- [x] miniaudio vendored or fetched via vcpkg with license recorded (commercial use, modification, redistribution).
- [x] Engine audio backend: init/shutdown, load/play one-shot and looping sounds, master volume.
- [x] Spatial listener follows active camera; at least one 3D positioned source API.
- [x] Sample project: one test sound triggered from existing interaction or animation event path.
- [x] Named `audio` or extended suite covers init, load, play, fail-closed on missing file.
- [x] Context docs: feature note + resource provenance.

## Out of scope

- Full FMOD-style event graphs, occlusion, reverb zones, music crossfade
- Combat hit SFX polish pass (M9)
- Editor audio browser

## Dependencies

- After or parallel with TICKET-0110 (P0); do not block 0110.

## Verification

Rebuild `engine`; run audio suite + sample trigger smoke. Set Status → `needs-approval` with license path and suite counts in **What changed**.

**Evidence (agent):** `audio` suite added with headless `no_device` init, WAV load/play, spatial listener, missing-file fail-closed, Lua bindings, sample campfire asset. Windows MSVC rebuild + suite run not executed in Linux cloud agent.

## What changed

- **Summary:** Added miniaudio-backed `AudioEngine` with init/shutdown, master volume, one-shot and looping playback, spatial sources, and a listener that tracks the active camera each editor frame. Lua scripts can call `engine.play_sound`, `engine.play_sound_at`, and `engine.set_master_volume`. Campfire interaction enter plays a sample crackle WAV.
- **Files / surfaces touched:**
  - New: `include/engine/audio/audio_engine.h`, `src/audio/audio_engine.cpp`
  - Build: `CMakeLists.txt`, `vcpkg.json` (top-level `miniaudio`)
  - Runtime: `src/rendering/render_app.cpp`, `src/scripting/lua_runtime.{h,cpp}`
  - CLI: `src/automation/command.cpp` (`audio` in CTest suite list)
  - Tests: `tests/suite_tests.cpp` (`audio` suite)
  - Sample: `samples/open-world-rpg/assets/audio/campfire_crackle.wav`, `assets/scripts/campfire_interaction.lua`
  - Docs: `context/features/audio.md`, `context/features/index.md`, `context/features/lua-scripting.md`, `context/resources/index.md`, `context/roadmap.md`, `context/planning/epics.md`
- **Schema / API / format deltas:**
  - Lua: `engine.play_sound(path [, loop])`, `engine.play_sound_at(path, x, y, z [, loop])`, `engine.set_master_volume(volume)`
  - C++: `AudioEngine` public API (see `audio_engine.h`)
  - CTest suite name: `audio`
- **Seed / sample data:** Procedural `campfire_crackle.wav` (project-owned); wired on campfire interaction enter.
- **Tests / verification evidence:** `audio` suite covers init (no_device), tone WAV fixture, one-shot + spatial play, missing file (`AUDIO-FILE-MISSING`), path escape, Lua bindings, sample asset smoke. **Not run:** Windows `engine` rebuild and live `engine test --suite audio` (cloud Linux environment).
- **Decisions & tradeoffs:** Headless tests use `AudioEngineConfig::no_device`; runtime editor session uses default device init with warning-only failure path. Active sounds tracked until playback ends (simple vector, no pooling yet).
- **Leftover risk / follow-ons:** No live device QA on cloud agent; settings UI `settings.sfx` slider still has no backend; animation-event footstep one-shots remain optional follow-on to TICKET-0105.

## Agent notes

Implemented 2026-07-22 on branch `cursor/miniaudio-ticket-0107-2b3b`. Notion sync skipped (MCP not authenticated in cloud).
