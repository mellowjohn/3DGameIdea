# TICKET-0010: Define World Forge scope vs existing editor/MCP

- Epic: EPIC-0002
- Status: needs-approval
- Agent: cursor-agent
- Priority: P1
- Notion: https://app.notion.com/p/39ad3efc569581e38ffdd865acf3b641

## Goal

Produce a written scope boundary for World Forge as the **narrative tooling umbrella**: lore/story vision → engine-integrable data; relationship graph/editor for people, factions, clans; map/story geography; and the product home for quest, dialogue, and story-event tools — without duplicating Scene/Sculpt/MCP placement paths.

## Context links

- `context/features/world-forge-scope.md` (deliverable)
- `context/planning/epics.md` (EPIC-0002, EPIC-0006)
- `context/architecture/content-vs-engine-workflows.md`
- `context/architecture/overview.md`
- `context/features/index.md`
- `context/roadmap.md` (M10 editor; M6/M7 quests/dialogue delivery)
- DEC-0003, DEC-0011, DEC-0015, DEC-0019, DEC-0020
- Follow-ons: TICKET-0011–0014; EPIC-0006 quest/dialogue; soft dependency on TICKET-0021

## Acceptance criteria

- [x] Doc lists World Forge as narrative umbrella: relationship graph + editor, faction/culture/clan authoring, regions/POIs/links, and product home for quests, dialogue, and story events — as product intent, not implementation yet.
- [x] Explicit non-overlap list: terrain height sculpt, mesh/prefab placement, live scene MCP apply, material inspect/save remain Scene/Sculpt/MCP-owned.
- [x] States which future surfaces are command-backed (DEC-0003) vs pure data formats (including EPIC-0006 tools hosted in World Forge).
- [x] Canon split recorded (DEC-0019): story markdown vs World Forge structured data.
- [x] Narrative umbrella recorded (DEC-0020); EPIC-0006 notes point to World Forge as product home.
- [x] Open questions listed in `context/interviews/open-questions.md` or resolved — not silently assumed.
- [x] `epics.md` Notes updated with pointer to the scope doc.

## Out of scope

- Implementing World Forge UI, schemas, or MCP commands.
- Changing open-world partition format or terrain tools.
- Faction canon content (TICKET-0021).
- Pulling M6/M7 quest/dialogue **implementation** ahead of M5 without owner override (product ownership only).

## Dependencies

- None hard for the scope doc.
- Blocks clear kickoff of TICKET-0011–0014 design; clarifies EPIC-0006 product home.

## Verification

- Doc-only review against acceptance checklist.
- No engine rebuild required.

## Agent notes

- 2026-07-15: Owner chose DEC-0019 — editor panels; story markdown canon + World Forge JSON keyed to IDs. Initial scope doc.
- 2026-07-15 (rework): Owner expanded vision — relationship graph/editor; encapsulate lore/story vision for engine integration; World Forge hosts dialogue, quest, story-event tools. Recorded DEC-0020; revised `world-forge-scope.md` and EPIC-0002/0006 notes. Awaiting owner approval.
