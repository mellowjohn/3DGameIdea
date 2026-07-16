# TICKET-0051: Quest creator tooling (command-backed)

- Epic: EPIC-0006
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ad3efc56958105a1cbc2f12a37d4af

## Goal

Command-backed World Forge Quests authoring: MCP `kind=quests` plus an editor Quests pane that shares `apply_world_forge_operation` with agents. Seeded from Twine Act 0 as the main-quest spine (`mq_act0_calrenoth`).

## Context links

- [DEC-0026](../../decisions/index.md#dec-0026-quest-owned-dialogue-hooks-multi-stage)
- [`context/formats/world-forge-quests.md`](../../formats/world-forge-quests.md)
- [`context/formats/world-forge-mcp.md`](../../formats/world-forge-mcp.md)
- Twine: [`context/story/sources/wrathful-conquest-act0.twee`](../../story/sources/wrathful-conquest-act0.twee)
- Beat sheet: [`context/story/campaign-beat-sheet.md`](../../story/campaign-beat-sheet.md)
- Depends on TICKET-0050 (schema)

## Acceptance criteria

- [x] `engine_world_forge_apply` supports `kind=quests` (get/validate/apply)
- [x] World Forge editor **Quests** pane lists quests and edits hooks/objectives/forks
- [x] Sample includes `mq_act0_calrenoth` hooked to Twine dialogue tree
- [x] Editor reload/save uses the same command path as MCP
- [x] `world_forge` suite covers quests MCP validate
- [x] Format/MCP docs list `quests`

## Out of scope

Dialogue graph canvas (0053); live quest tracker; inventing Act 1+ main quests; fleshing SQ dialogue trees beyond soft hook IDs.

## Dependencies

Owner override of M5 hold. Soft depends on 0050 schema + 0052 dialogues for linked `startId`.

## Verification

- Rebuild `engine_core` / `engine_suite_tests` — passed (pre-existing C4996 in `render_app.cpp`)
- `engine_suite_tests --suite world_forge` — **87/87** passed
- `engine validate --project samples/open-world-rpg` — exit 0
- `engine.exe` relink blocked (LNK1168 — process holds the file); library + suites verified

## What changed

### Summary

World Forge Quests tooling is live: MCP agents and the editor Quests pane read/write `quests.worldforge.json` through one command path. The sample now leads with Act 0 **Fall of Calrenoth**, hooked to the Twine-imported dialogue tree.

### Files / surfaces

- `src/automation/world_forge_commands.cpp`, `mcp_server.cpp` tool description
- `include/engine/ui/world_forge_editor.h`, `src/ui/world_forge_editor.cpp` (Quests pane)
- `samples/.../quests.worldforge.json` (mq_act0 seed)
- `context/formats/world-forge-mcp.md`

### Schema / API

- MCP `kind=quests` (aliases `quest`); path inference for `quests.worldforge.json`
- Soft regionId checks against map asset on validate/apply

### Seed

- `mq_act0_calrenoth` with beat-sheet objectives; `dialogue.startId=dlg_act0_wrathful_conquest`
- SQ-01/SQ-02 retained with **empty** dialogue hooks (soft) until side-quest lines are authored — Twine is the main quest

### Leftover

SQ dialogue tree content; polish add-quest UX; dialogue graph editor remains 0053.

## Agent notes

2026-07-15: Owner directed Twine Act 0 as the seed for 0051/0052 (Twine = main quest dialogue).
2026-07-15: Cleared SQ dialogueId stubs so project validate stays honest without invented side trees.
