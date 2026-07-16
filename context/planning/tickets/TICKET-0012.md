# TICKET-0012: Entity and character relationship graph format

- Epic: EPIC-0002
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ad3efc5695816bb828ec15d60debb2

## Goal

Ship a versioned, diffable World Forge relationship graph (`relationships.worldforge.json`) with C++ load/parse/validate/save, typed edges to nodes and faction IDs, and a seeded sample that does not invent open canon answers. Graph **editor** UI is out of scope.

## Context links

- `context/planning/epics.md` (EPIC-0002)
- `context/formats/world-forge-relationships.md`
- `context/formats/world-forge-factions.md`
- `context/features/world-forge-scope.md`
- `context/story/factions.md`, `companions.md`, `the-squire.md`, `campaign-beat-sheet.md`
- `include/engine/assets/world_forge_relationships_asset.h`
- Sample: `samples/open-world-rpg/assets/world-forge/relationships.worldforge.json`
- Related: TICKET-0011 (factions), TICKET-0013 (regions/POIs), TICKET-0014 (MCP)

## Acceptance criteria

- [x] Diffable schema documented in `context/formats/world-forge-relationships.md`.
- [x] Sample seeds known people/deities/artifacts + typed edges; open canon stays in `openQuestions` / draft|open status.
- [x] `WorldForgeRelationshipsAsset` load/parse/to_json/save_atomic/validate + faction-ref cross-check.
- [x] Path helper `default_world_forge_relationships_path`.
- [x] Suite coverage in `world_forge`: load, round-trip, reject empty node id, unknown node ref, self-loop, bad edge kind, unknown faction ref when known set provided.
- [x] Project `validate` loads relationships when present and cross-checks faction endpoints against factions asset when loaded.
- [x] Context indexes linked; Twine-only warband names not seeded as established.

## Out of scope

World Forge graph editor UI; MCP mutation (TICKET-0014); regions/POIs (TICKET-0013); inventing story answers (Grul’thaz / Shadowpaw, romance rules, Luceran agency).

## Dependencies

TICKET-0011 schema (faction endpoints); story IDs from `context/story/`.

## Verification

- Rebuild `engine_core` + `engine_suite_tests` — passed (0 warnings).
- `--suite world_forge` — 27/27 passed.
- `engine.exe` relink blocked (LNK1168 — process holds the file); library + suites verified.

## What changed

### Summary

Added World Forge relationship graph as a versioned JSON asset (`schemaVersion` 1). Nodes cover people/deities/artifacts/organizations; edges are typed and can point at local nodes or at faction IDs from `factions.worldforge.json`. Project validate loads the relationships file when present and cross-checks faction endpoints against the factions registry. Sample seeds only story-backed figures and edges; open/draft tensions stay explicit.

### Files / surfaces

**Created**

- `include/engine/assets/world_forge_relationships_asset.h`
- `src/assets/world_forge_relationships_asset.cpp`
- `samples/open-world-rpg/assets/world-forge/relationships.worldforge.json`
- `context/formats/world-forge-relationships.md`

**Modified**

- `CMakeLists.txt` — compile relationships asset into `engine_core`
- `src/automation/command.cpp` — validate hook + faction-id cross-check
- `tests/suite_tests.cpp` — extended `world_forge` suite
- `context/formats/world-forge-factions.md`, `world-forge-scope.md`, features/README/coverage indexes
- `context/planning/epics.md` / this stub / Notion

### Schema / API

- Path: `assets/world-forge/relationships.worldforge.json` via `default_world_forge_relationships_path`
- Node kinds: `person` | `deity` | `artifact` | `organization`
- Edge kinds: `ally` | `rival` | `member_of` | `leads` | `kin` | `serves` | `opposes` | `influences` | `related`
- Endpoint `target`: `node` | `faction`
- Validation: unique node/edge ids; node refs resolve; no self-loops; optional `validate_faction_refs`
- Error family: `WORLD-FORGE-REL-*`

### Sample data

**Nodes (7):** `luceran_the_hollow`, `asher_the_brittle`, `arkand`, `vanessa`, `frangitur`, `creotar`, `nefarium_shroud`

**Edges (6):** Luceran→Imperium leads; Asher↔Luceran kin (draft); Arkand→Kingdom member_of; Cristallo↔Arrotrebae rival; Frangitur→Luceran influences (open); Luceran→Shroud related

Deliberately omitted: Twine warband names (Grul’thaz / Shadowpaw), invented tribes/clans, romance edges.

### Tests / rebuild

- `world_forge` 27/27
- `engine_core` + `engine_suite_tests` OK
- `engine.exe` relink blocked (LNK1168 while process holds the binary)

### Decisions & leftovers

- Faction endpoints by ID (no duplicated faction nodes) so 0011 remains the faction registry.
- Graph editor UI deferred; next schema piece is TICKET-0013 regions/POIs.
- TICKET-0011 still awaits owner `done` separately.

## Agent notes

- 2026-07-15: Implemented and verified; status needs-approval.
