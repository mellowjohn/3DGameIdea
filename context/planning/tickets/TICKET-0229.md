# TICKET-0229: Stylized grass blade mesh + lean/trample/wind

- Epic: EPIC-0013
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/3aad3efc569581008c50cd633fb89a80

## Goal

Make open-world grass read as tapered blades with height, tip-weighted lean/trample when the player walks through, and light tip wind — within DEC-0013 instancing (no skeletal bones, no Tsushima compute blades).

## Context links

- `context/decisions/index.md` — DEC-0013 (+ 2026-07-27 evolution note)
- `context/formats/foliage-layers.md`
- `context/features/terrain-authoring.md`
- `context/art/theme-palette.md`
- Related: TICKET-0220 (foliage cull/LOD — soft, unchanged streaming)

## Acceptance criteria

- [x] `grass_blade` primitive is a crossed double-card strip with ≥4 vertical segments and AABB height ~0.7 (not a 2-triangle wedge).
- [x] Foliage VS tip-squared lean away from `WorldInfluence` + light trample (Y squash); velocity kick retained.
- [x] Tip wind flutter from `time_seconds` + instance XZ phase; muted under strong interaction falloff.
- [x] Sample `ground-cover.layers.json` grass retuned (density ~0.15, scale range, bend, Canopy Shade color, bladeHeight 0.7).
- [x] `foliage` suite asserts multi-segment mesh + sample density/height; `world_influence` suite still passes.
- [x] Format/feature/decision docs updated for lean/trample/wind semantics.

## Out of scope

- Cubic Bézier GPU blade generation / compute indirect draws
- Voronoi clump fields / multi-entity render-target trails
- Persistent flattened grass trails
- Real skinned bones on foliage
- Activating `disturbVfxId` particles

## Dependencies

- Extends DEC-0013; no blocker on TICKET-0220.

## Verification

- Rebuild `engine` + `engine_suite_tests` — succeeded (3 pre-existing warnings, 0 errors)
- `engine_suite_tests --suite foliage` — 78/78
- `engine_suite_tests --suite world_influence` — 4/4
- Desktop/MCP play-test: blocked this session — Cursor MCP client stayed `Not connected` after rebuild; lease released via CLI. Reconnect MCP / restart editor locally to walk meadows.

## What changed

- Summary: Replaced flat wedge grass with a multi-segment crossed tapered blade; walk-through now leans and tramples tips; idle meadows get subtle tip flutter; sample meadow density/look retuned to theme olive.
- Files / surfaces: `src/assets/mesh_asset.cpp`, foliage VS + influence feed in `src/rendering/render_app.cpp`, sample `ground-cover.layers.json`, `tests/suite_tests.cpp`, format/feature/decision docs, `epics.md` + Notion TICKET-0229.
- Schema / API / format deltas: none (same palette fields; bend semantics documented as lean + trample + tip wind).
- Seed / sample data: grass layer density 0.15, scale 0.7–1.35, bendStrength 0.5, bendRadius 1.5, bladeHeight 0.7, Canopy Shade color.
- Tests / verification evidence: foliage 78/78; world_influence 4/4; engine rebuilt to `dev-next/engine.exe`.
- Decisions & tradeoffs: mid-tier DEC-0013 evolution over Tsushima Bézier; fake spine via tip-squared lean, not bones.
- Leftover risk / follow-ons: denser scatter may raise foliage draw cost — watch Diagnostics after MCP reconnect; full procedural blades deferred. Follow-up 2026-07-27: rebuilt `grass_blade` as 7-blade tufts (BOTW clumps) after visual confirmed spike look; density 0.07.

## Agent notes

Lease acquire → kill → rebuild → release completed. Visual before shot: `out/grass-spikes-before-tufts-*.png` (isolated spikes). Tuft mesh landed; editor restarted on `dev-next`. Cursor MCP needs reconnect for after screenshot.
