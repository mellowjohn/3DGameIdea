# TICKET-0054: Twine → World Forge dialogue import

- Epic: EPIC-0006
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39fd3efc5695815c8b44e24303c403bb

## Goal

Import Harlowe Twine `.twee` into `dialogues.worldforge.json` via C++ + MCP + Dialogues pane (create/replace a tree). Sync draft lore context from Act 0 Twine where useful without inventing closed answers. Export Twine is deferred.

## Context links

- [DEC-0026](../../decisions/index.md#dec-0026-quest-owned-dialogue-hooks-multi-stage)
- [`context/formats/world-forge-dialogues.md`](../../formats/world-forge-dialogues.md)
- [`context/formats/world-forge-mcp.md`](../../formats/world-forge-mcp.md)
- Twine: [`context/story/sources/wrathful-conquest-act0.twee`](../../story/sources/wrathful-conquest-act0.twee)
- Soft lore sync: companions / the-squire / relationships / open-questions
- Related: TICKET-0052 (asset/runtime), TICKET-0053 (graph editor)

## Acceptance criteria

- [x] C++ Harlowe `.twee` → dialogue tree importer (`twee_import`)
- [x] `engine_world_forge_apply` `action=import_twee` (`kind=dialogues`) upserts by `treeId`
- [x] World Forge Dialogues pane Import Twine UI (usable with no tree selected)
- [x] Suite coverage for importer + command path (temp project; does not mutate sample tree id)
- [x] Format/MCP docs updated
- [x] Draft lore context updated from Twine (no invented resolutions)

## Out of scope

Twine export; full Harlowe macro evaluation; play-test modal; SQ dialogue authorship; resolving Creotar/Creo or other open story questions.

## Dependencies

Owner override of EPIC-0006 hold. Soft depends on TICKET-0052 dialogues asset.

## Verification

- Rebuild `engine` / `engine_suite_tests` — passed (pre-existing C4996 in `render_app.cpp`)
- `engine_suite_tests --suite world_forge` — **87/87** passed
- Sample project validate covered by suite

## What changed

### Summary

Engine can import Act 0 (or any Harlowe `.twee`) into World Forge dialogues via MCP/`import_twee` and the Dialogues pane. Draft lore docs and relationship graph nodes were updated from Twine facts without closing open questions.

### Files / surfaces

- `include/engine/dialogue/twee_import.h`, `src/dialogue/twee_import.cpp`
- `src/automation/world_forge_commands.cpp`, `src/automation/mcp_server.cpp`
- `src/ui/world_forge_editor.cpp` (Import Twine controls)
- `tests/suite_tests.cpp`, format docs
- Lore: `companions.md`, `the-squire.md`, `open-questions.md`, `relationships.worldforge.json`

### Schema / API

- `action=import_twee|import-twee`, `kind=dialogues`
- Params: `tweePath`, `treeId` (required); optional `displayName`, `parentQuestId`, `entryNodeId`, `storyRef`
- Metadata: `nodeCount`, `treeId`, `tweePath`, `requiresReload=world_forge`

### Seed / sample data

- Existing sample tree `dlg_act0_wrathful_conquest` unchanged by tests (import probe uses temp copy + `dlg_act0_import_probe`)
- Relationships: draft nodes Grenge / Larrell / Damius + edges; Asher/Arkand summaries enriched from Twine

### Tests / verification evidence

- Rebuild `engine` / `engine_suite_tests` — passed (C4996 getenv/sscanf in `render_app.cpp` unchanged)
- `world_forge` suite **87/87**

### Decisions & tradeoffs

- Import-only for this ticket (export later)
- Speaker inference is heuristic; keep trees `canonStatus: draft`
- Offline Python tool remains for batch regen

### Leftover risk / follow-ons

- Export Twine; richer Harlowe macros; default `tweePath` assumes sample project relative layout (`../../context/...`)

## Agent notes

2026-07-15: Owner asked import-first + draft lore sync from Twine dialogue context.
