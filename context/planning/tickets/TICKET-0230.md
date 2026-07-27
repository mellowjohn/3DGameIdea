# TICKET-0230: Wind trails + vegetation response

- Epic: EPIC-0013
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/3aad3efc569581319a1ffe40c449a16e

## Goal

Meadows and trees read a shared directional wind: ambient tip flutter plus traveling gust lean on foliage layers, soft canopy sway on tree meshes, and faint camera-local wind-streak particles — within DEC-0013 (no bones / no Tsushima compute blades).

## Context links

- `context/decisions/index.md` — DEC-0013
- `context/formats/foliage-layers.md`
- `context/formats/particle-emitter-assets.md`
- `context/features/particles.md`
- `context/features/terrain-authoring.md`
- Related: TICKET-0229 (tip flutter), TICKET-0122 (particle MVP)

## Acceptance criteria

- [x] Shared wind field (dir/speed/time/gust/ambient) drives foliage VS: keep ambient tip flutter; add traveling gust tip lean along wind; mute under strong player influence falloff.
- [x] Tree meshes (`tree` / `dead-tree` path filter) get height-weighted canopy sway from the same wind field in prop-instance VS (placed trees use the prop path).
- [x] Camera-local ambient wind emitter spawns visible streak particles (`wind_trail.particle.json` + `wind_streak.png`); particle `texture` path samples when set; campfire soft-disc path unchanged.
- [x] Named suites: `foliage` and `particles` pass; wind envelope helper covered in `world_influence`.
- [x] Format/feature/decision docs updated for gust trails + ambient wind emitter + textured particles.

## Out of scope

- Per-region wind authoring UI
- Tsushima-style Bézier compute blades
- Footstep `disturbVfxId` activation
- General multi-texture particle atlas
- Sway on rocks / buildings / non-tree props
- Scene bush prefab wind (foliage-layer bushes only)

## Dependencies

- Extends DEC-0013 + TICKET-0229 tip flutter; soft dependency on TICKET-0122 particle MVP (landed needs-approval).

## Verification

- Rebuild `engine` + `engine_suite_tests` — succeeded (pre-existing getenv/shadow warnings; 0 errors)
- `engine_suite_tests --suite world_influence` — 10/10
- `engine_suite_tests --suite foliage` — 78/78
- `engine_suite_tests --suite particles` — 23/23
- Editor/MCP restarted on `dev-next/engine.exe mcp --project samples/open-world-rpg`; lease released

## What changed

- Summary: Meadows now show ambient tip flutter plus coherent traveling gust lean; tree tops sway with the same wind; faint elongated wind-streak particles drift near the camera in Game/play-test. Campfire still uses the procedural soft disc.
- Files / surfaces: `include/engine/world/wind_field.h`, foliage + prop VS / particle pipeline in `src/rendering/render_app.cpp`, `ParticleSystem` ambient wind + texture index, sample `wind_trail.particle.json` + `wind_streak.png`, suites, context docs, `epics.md` + Notion.
- Schema / API / format deltas: particle `texture` now sampled when non-empty; `ParticleSystem::set_ambient_wind`; `WindFieldParams` + `wind_gust_envelope` / `mesh_uses_wind_sway`.
- Seed / sample data: `assets/vfx/wind_trail.particle.json`, `assets/vfx/wind_streak.png` (project-owned AI placeholder).
- Tests / verification evidence: world_influence 10/10; foliage 78/78; particles 23/23; engine rebuilt to `dev-next/engine.exe`.
- Decisions & tradeoffs: shared CPU wind field + root constants (not WorldInfluenceBus); trees filtered by mesh path on prop draws; single custom particle texture slot for streaks.
- Leftover risk / follow-ons: no per-region wind UI; opaque non-prop path does not sway (trees are prop-instanced); desktop visual QA of gust readability / particle density recommended.

## Agent notes

Lease acquire → kill → rebuild → restart MCP → release completed (`4e32bf5ffcb8-1`).
