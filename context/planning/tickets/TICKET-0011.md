# TICKET-0011: Faction / culture / clan data model (diffable)

- Epic: EPIC-0002
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ad3efc56958107b2bad06823d85024

## Goal

Ship a diffable World Forge factions schema (`factions.worldforge.json`) keyed to story IDs from `context/story/factions.md`, with C++ load/parse/validate/save and a seeded sample that does not invent open canon answers.

## Context links

- `context/planning/epics.md` (EPIC-0002)
- `context/story/factions.md`
- `context/formats/world-forge-factions.md`
- `context/features/world-forge-scope.md`
- `include/engine/assets/world_forge_factions_asset.h`
- Sample: `samples/open-world-rpg/assets/world-forge/factions.worldforge.json`
- Related: TICKET-0010 (scope), TICKET-0021 (canon gaps), TICKET-0012 (relationships)

## Acceptance criteria

- [x] Diffable schema documented in `context/formats/world-forge-factions.md` (shape, enums, path, non-goals).
- [x] Sample `assets/world-forge/factions.worldforge.json` seeds known groups with `canonStatus` and `openQuestions`; no invented theology/council/orc names/influence thresholds.
- [x] `WorldForgeFactionsAsset` load/parse/to_json/save_atomic/validate with `WORLD-FORGE-FACTION-*` errors.
- [x] Path helper `default_world_forge_factions_path`.
- [x] Suite coverage: load sample, round-trip, reject duplicate id / bad kind / unknown parentId / empty id.
- [x] Optional validate hook: project `validate` loads factions file when present.
- [x] Context indexes linked (`world-forge-scope.md`, `features/index.md`, `context/README.md`).

## Out of scope

World Forge UI; MCP mutation commands (TICKET-0014); relationship graph (TICKET-0012); inventing story canon answers.

## Dependencies

TICKET-0021 (owner approved); TICKET-0010 scope.

## Verification

- Rebuild `engine_core` + `engine_suite_tests` (MSBuild) — passed.
- `--suite world_forge` — 14/14 passed; `--suite assets` — 53/53 passed.
- `engine.exe` relink blocked (file locked by running process); library + suites verified.
- Status → needs-approval after verification — never done.

## What changed

### Summary

Added World Forge factions as a first-class versioned JSON asset (`schemaVersion` 1). The engine can load, validate, serialize, and atomically save faction/culture/clan/warband entities keyed to story IDs. Open canon stays explicit via `canonStatus` + `openQuestions` rather than inventing theology, council rules, orc names, or influence thresholds. Project `validate` now checks the default factions path when the file exists.

### Files / surfaces

**Created**

- `include/engine/assets/world_forge_factions_asset.h`
- `src/assets/world_forge_factions_asset.cpp`
- `samples/open-world-rpg/assets/world-forge/factions.worldforge.json`
- `context/formats/world-forge-factions.md`

**Modified**

- `CMakeLists.txt` — `world_forge_factions_asset.cpp` in `engine_core`; new suite name `world_forge`
- `src/automation/command.cpp` — `validate` calls `WorldForgeFactionsAsset::validate_file(default_world_forge_factions_path(...))`
- `tests/suite_tests.cpp` — `world_forge` suite cases
- `context/testing/coverage.md`, `context/features/world-forge-scope.md`, related indexes
- `context/planning/epics.md` / this stub / Notion

### Schema / API

- Path: `assets/world-forge/factions.worldforge.json` via `default_world_forge_factions_path`
- Entity fields: `id`, `kind`, `displayName`, `canonStatus`, `summary`, `storyRef`, `tags`, optional `politicalRole`, `parentId`, `openQuestions`
- Kinds: `faction` | `culture` | `clan` | `warband`
- `canonStatus`: `established` | `draft` | `proposal` | `open`
- `politicalRole` (optional): `arena` | `faction` | `unknown`
- Validation: `schemaVersion == 1`; unique non-empty ids; enum checks; non-empty `parentId` must reference another entity
- Error code family: `WORLD-FORGE-FACTION-*` (e.g. `SCHEMA`, `ID`, `ID-DUP`, `KIND`, `CANON`, `PARENT`, `PARSE`, `READ`, `IO`)

### Sample data

Seeded five entities from `factions.md` only: `kingdom_tessera`, `chaotic_imperium`, `cristallo`, `arrotrebae`, `orc_warbands`. No child clans/warbands invented; open items live under `openQuestions` (kingdom arena vs faction, Cristallo theology, Arrotrebae council, orc warband names, influence rules).

### Tests / rebuild

- `world_forge` 14/14; `assets` 53/53
- `engine_core` + `engine_suite_tests` rebuilt OK
- `engine.exe` relink not completed while the process held the binary

### Decisions & leftovers

- Schema-first World Forge (no UI/MCP mutate yet) — UI/MCP is TICKET-0014; relationship graph is TICKET-0012
- Did not invent answers to TICKET-0021 open questions
- Follow-on: close editor and relink `engine` when convenient; next model piece is TICKET-0012

## Agent notes

- 2026-07-15: Implemented and verified; status needs-approval. Standing ticket policy now requires **What changed** for all new tickets at approval time.
