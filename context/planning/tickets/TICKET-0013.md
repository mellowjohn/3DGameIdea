# TICKET-0013: Map-asset authoring surface (regions, POIs, links)

- Epic: EPIC-0002
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ad3efc569581c0b07bdff37d675c92

## Goal

Ship a versioned World Forge map asset (`map.worldforge.json`) for regions, POIs, and travel/soft-gate links — IDs and narrative metadata only, with C++ load/validate/save and a story-backed sample. Editor panels and MCP mutate are follow-ons (command-backed via DEC-0003 / TICKET-0014).

## Context links

- `context/formats/world-forge-map.md`
- `context/features/world-forge-scope.md`
- `context/story/campaign-beat-sheet.md`, `story-vision.md`, `factions.md`
- `include/engine/assets/world_forge_map_asset.h`
- Sample: `samples/open-world-rpg/assets/world-forge/map.worldforge.json`
- Related: TICKET-0011, TICKET-0012, TICKET-0014, EPIC-0007 mini-map

## Acceptance criteria

- [x] Schema documented in `context/formats/world-forge-map.md` (regions, POIs, links, softGate, optional anchors).
- [x] Sample seeds Calrenoth / overland / Rampant Wilds + Act 0 POIs/links; no invented city catalog; open geography stays in `openQuestions`.
- [x] `WorldForgeMapAsset` load/parse/to_json/save_atomic/validate + faction-id cross-check.
- [x] Path helper `default_world_forge_map_path`.
- [x] `world_forge` suite: load, round-trip, reject empty region id, unknown POI regionId, unknown link ref, self-link, unknown factionId when known.
- [x] Project `validate` loads map when present and cross-checks region `factionIds`.
- [x] Context indexes linked; mesh placement and mini-map rendering explicitly out of scope.

## Out of scope

World Forge editor panels; MCP mutate (TICKET-0014); Scene mesh placement; mini-map rendering (EPIC-0007); inventing named towns/cities still open in story.

## Dependencies

TICKET-0011 faction IDs; story geography from Act 0 / factions framing.

## Verification

- Rebuild `engine_core` + `engine_suite_tests` — passed.
- `--suite world_forge` — 40/40 passed.
- `engine.exe` relink not required for this schema ticket; prior LNK1168 if editor holds the process.

## What changed

### Summary

Added World Forge map geography as versioned JSON: regions (with parent + optional soft-gate + faction refs), POIs (region-bound, optional scene/prefab refs), and typed links (travel / soft_gate / story_gate / adjacency). Project validate cross-checks region faction ids against the factions asset. Sample covers only story-named Act 0 / wilderness IDs.

### Files / surfaces

**Created:** `world_forge_map_asset.h/.cpp`, `map.worldforge.json` sample, `context/formats/world-forge-map.md`

**Modified:** CMakeLists, `command.cpp` validate, `suite_tests.cpp`, indexes / scope / coverage, epics + stub + Notion

### Schema / API

- Path: `assets/world-forge/map.worldforge.json`
- Region kinds: region|fortress|city|wilderness|chaotic|settlement|other
- POI kinds: landmark|settlement|gate|shrine|camp|other
- Link kinds: travel|soft_gate|story_gate|adjacency
- Errors: `WORLD-FORGE-MAP-*`

### Sample data

**Regions (3):** `tessera_overland`, `calrenoth`, `rampant_wilds`  
**POIs (2):** `calrenoth_drawbridge`, `calrenoth_siege_front`  
**Links (2):** Calrenoth→overland soft_gate; drawbridge adjacency into Calrenoth  

No invented city list; empty scene/prefab refs; no world anchors yet.

### Tests / rebuild

- `world_forge` 40/40
- `engine_core` + `engine_suite_tests` OK

### Decisions & leftovers

- Pure data first (scope table); editor UI + MCP remain for follow-on / TICKET-0014.
- Next World Forge ops ticket: **TICKET-0014**.

## Agent notes

- 2026-07-15: Implemented and verified; awaiting owner approval.
