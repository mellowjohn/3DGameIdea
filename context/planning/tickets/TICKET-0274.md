# TICKET-0274: MCP terrain/scene graybox authoring helpers

- Epic: EPIC-0009
- Status: needs-approval
- Agent: cursor-agent
- Priority: P1
- Notion: (mirror pending — no Notion MCP in this session)

## Goal

Make live MCP terrain sculpt + scene grayboxing efficient for agents: batch polyline height/paint/water ops, bulk terrain snap, entity list/name resolve, smooth/terrace brushes, smart sculpt recipes, and water→foliage cleanup — without Python substitutes.

## Context links

- `context/features/mcp-live-editor.md`
- `context/formats/terrain-edits.md`
- `context/formats/prefab-assets.md` (stamp_compositions)
- DEC-0018 (MCP terrain sculpt/paint apply)
- Owner request: main-menu graybox session tooling gaps (2026-08-06)

## Acceptance criteria

- [x] `engine_terrain_apply` `batch` accepts `carve_channel`, `raise_banks`, `paint_along`, `smooth`, and `terrace` ops (with polyline densify + undo snapshot)
- [x] Top-level `paint_along`, `smooth`, and `terrace` actions work
- [x] Smart sculpt recipes: `gentle_hill` / `steep_cliff` / `flatten_pad` / `smooth_natural` / `canyon`
- [x] `engine_water_apply` `batch` accepts `place_along`; water place paths clear foliage by default (`eraseFoliage: false` to keep)
- [x] `engine_scene_apply` supports `list` / `query`, `snap_to_terrain`, and resolve `move`/`remove`/`rename` by unique `name`
- [x] Graybox `stamp_compositions` keeps neutral gray default when `color` omitted (documented)
- [x] Suite coverage for the new actions; MCP tool descriptions + feature/format docs updated
- [x] `engine` rebuild succeeds

## Out of scope

- Full sculpt UI toolbar for smooth/terrace
- Changing terrain delta limits or cell size
- Parenting elevated VFX to graybox entities (offset snap is enough)
- Terrain Generator / biome painter / hydraulic erosion / Terrain Director / critic (future epic — not this pass)

## Dependencies

None blocking.

## Verification

- Rebuild `engine` Debug → success (warnings only: existing C4996/C4456/C4100)
- `engine_suite_tests --suite terrain` → 241/241
- `engine_suite_tests --suite water` → 60/60
- `engine_suite_tests --suite automation` → 199/199
- Lease acquired/released for TICKET-0274; editor relaunched

## What changed

- Summary: Agents can batch polyline carve/paint/water ops, smart-sculpt hills/cliffs/pads/canyons, bulk-snap props after sculpt, list entities live, and resolve move/remove by name. Water placement clears foliage under the brush by default.
- Files / surfaces touched: `terrain_edits` (smooth/terrace brushes); `editor_session` (terrain batch + smart sculpt + scene list/snap/name resolve); `water_session` (batch place_along + foliage clear); `mcp_server` tool descriptions; docs (`mcp-live-editor.md`, `terrain-edits.md`); suites.
- Schema / API / format deltas: new terrain actions `smooth`, `terrace`, `paint_along`, `gentle_hill`, `steep_cliff`, `flatten_pad`, `smooth_natural`, `canyon`; scene `list`/`query`/`snap_to_terrain`; water `eraseFoliage`; rename-by-name uses `newName`.
- Seed / sample data: none.
- Tests / verification evidence: terrain 241, water 60, automation 199 — all pass; engine rebuilt.
- Decisions & tradeoffs: From the large AI-terrain wishlist, only Smart Sculpt recipes were pulled into this pass; generators/erosion/director deferred.
- Leftover risk / follow-ons: Notion card not mirrored (no Notion MCP). Cursor may need MCP reload to pick up tool description text. Main-menu graybox sculpt pass still unfinished from prior chat.

## Agent notes

Owner asked to improve MCP tools + nice-to-haves from main-menu graybox session feedback.
From the larger AI terrain wishlist, **only Smart Sculpt recipes** landed in this first pass; generators/erosion/director deferred.
