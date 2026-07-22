# Audio playback (miniaudio)

Status: **needs-approval** (TICKET-0107)

## Overview

Spatial and non-spatial WAV playback via **miniaudio** (vcpkg). The engine owns a single `AudioEngine` per editor session; the listener follows the active gameplay/editor camera each frame.

## Runtime API (C++)

- `AudioEngine::initialize` / `shutdown` — device init; `no_device` for headless suites
- `play_one_shot` / `play_spatial` — absolute paths
- `play_project_sound` / `play_project_sound_at` — project-relative paths under `assets/`
- `set_master_volume` (0–1), `update_listener(position, forward)`, `update(dt)` — drains finished one-shots

## Lua host API

| Function | Description |
| --- | --- |
| `engine.play_sound(path [, loop])` | Project-relative one-shot (optional loop) |
| `engine.play_sound_at(path, x, y, z [, loop])` | Spatial one-shot at world position |
| `engine.set_master_volume(volume)` | Master gain 0–1 |

Paths must stay under the project root (e.g. `assets/audio/campfire_crackle.wav`).

## Sample project

- `samples/open-world-rpg/assets/audio/campfire_crackle.wav` — procedural crackle (project-owned)
- `assets/scripts/campfire_interaction.lua` — plays crackle on interaction **enter**

## Verification

```powershell
engine test --project samples/open-world-rpg --suite audio
```

Headless `audio` suite covers init, WAV load/play, spatial listener calls, missing-file fail-closed, path escape, Lua bindings, and sample asset smoke.

## Out of scope (ticket)

Event graphs, occlusion/reverb, music crossfade, editor audio browser — deferred to later milestones.

## References

- [`context/resources/index.md`](../resources/index.md) — miniaudio license
- [`context/planning/tickets/TICKET-0107.md`](../planning/tickets/TICKET-0107.md)
- [`context/features/lua-scripting.md`](lua-scripting.md)
