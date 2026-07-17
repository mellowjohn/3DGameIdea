# TICKET-0189: World Forge Act lens (organize by acts)

- Epic: EPIC-0002
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39fd3efc569581e99229c0c44df08465

## Goal

Add a global World Forge **Act** lens so authors can separate campaign content by Act 0–4 without splitting worldforge JSON files (DEC-0036 option 1).

## Context links

- `context/formats/world-forge-acts.md`
- `context/story/campaign-beat-sheet.md`
- DEC-0021, DEC-0036
- `context/features/world-forge-scope.md`, `context/features/editor-mvp.md`

## Acceptance criteria

- [x] Optional `acts: act0..act4` on quests, dialogue trees, map regions/POIs, relationship nodes; empty = campaign-wide
- [x] Toolbar Act combo filters Map / Quests / Dialogues / Persons / Relationships; Religion/Factions/Archetypes stay unfiltered
- [x] Detail inspectors expose Acts checkboxes; legacy `actN` tags still count for filter
- [x] Sample data migrates known act tags into `acts`
- [x] Validation rejects unknown act ids (`WORLD-FORGE-ACT-ID`)
- [x] `world_forge` suite covers filter helpers + bad act id
- [x] Context + DEC-0036 recorded

## Out of scope

Per-act file splits; Act-first top-level navigation (option 2); runtime act gating / soft-gate logic.

## Dependencies

Soft: campaign beat sheet act labels (TICKET-0020 / DEC-0021).

## Verification

- Rebuild `engine` + `engine_suite_tests` (MSBuild Debug) — passed (C4996 warnings only in `render_app` on earlier cores rebuild).
- `--suite world_forge` — **163/163** passed.

## What changed

### Summary

World Forge now has a toolbar **Act** lens (All / Act 0–4). Quests, dialogues, map regions/POIs, and relationship people/nodes can declare `acts[]`; empty means campaign-wide. Selecting an act hides other-act content in those panes while Religion, Factions, and Archetypes stay visible.

### Files / surfaces

**Created:** `include/engine/assets/world_forge_acts.h`, `context/formats/world-forge-acts.md`, this stub

**Modified:** quests/dialogues/map/relationships asset headers+cpp; `world_forge_editor.h/.cpp`; sample worldforge JSON; suite_tests; DEC/index, scope, editor-mvp, coverage, README, epics

### Schema / API

- Field `acts: string[]` (`act0`…`act4`) on quests, dialogue trees, regions, POIs, relationship nodes
- Error `WORLD-FORGE-ACT-ID`
- Helpers: `matches_world_forge_act_filter` / `resolve_world_forge_acts` (legacy `actN` tags still count)

### Seed / sample

Promoted existing `act0` tags into `acts` on Act 0 quest/dialogue/map/person entries. Removed accidental draft `orc_warband_guy` node from relationships sample.

### Tests / rebuild

world_forge 163/163; engine.exe rebuilt; editor restarted.

### Decisions & leftovers

DEC-0036. Act-first navigation and per-act files remain out of scope. Runtime soft-gate-by-act not included.

## Agent notes

Owner chose Act lens (option 1).
