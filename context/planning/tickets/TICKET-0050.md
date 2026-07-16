# TICKET-0050: Quest data model and validation

- Epic: EPIC-0006
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ad3efc56958139b354cc344771638a

## Goal

Versioned `quests.worldforge.json` schema + C++ asset validation under World Forge, with multi-stage dialogue hooks (DEC-0026). Owner override to start before M5 exit.

## Context links

- [DEC-0026](../../decisions/index.md#dec-0026-quest-owned-dialogue-hooks-multi-stage)
- [`context/formats/world-forge-quests.md`](../../formats/world-forge-quests.md)
- [`context/story/side-quest-catalog.md`](../../story/side-quest-catalog.md)
- World Forge scope / DEC-0019–0020

## Acceptance criteria

- [x] Schema documented (`world-forge-quests.md`)
- [x] C++ `WorldForgeQuestsAsset` parse/validate/save + default path
- [x] Sample seeds SQ-01 / SQ-02 with per-objective and fork `dialogueId`s
- [x] Project `validate` includes quests (regionId soft-check vs map)
- [x] `world_forge` suite covers happy path + rejection cases
- [x] DEC-0026 recorded (quest owns hooks; dialogue may have `parentQuestId`)

## Out of scope

Quest creator UI / MCP mutate (0051); dialogue graph schema/runtime (0052/0053); live quest tracker.

## Dependencies

Owner override of M5 hold. Soft depends on map regions for `regionId`. Aligns with TICKET-0112.

## Verification

- Rebuild `engine` / `engine_suite_tests` — passed
- `engine_suite_tests --suite world_forge` — 54/54 passed
- `engine validate --project samples/open-world-rpg` — exit 0

## What changed

### Summary

Added World Forge quests asset: each quest can attach **different dialogue trees per stage** (start/complete/abandon plus per-objective and per-fork `dialogueId`). Dialogue’s `parentQuestId` lands with 0052. Sample seeds Cart Again and Signal Fire Debt from the side-quest catalog.

### Files

- `include/engine/assets/world_forge_quests_asset.h`, `src/assets/world_forge_quests_asset.cpp`
- `samples/open-world-rpg/assets/world-forge/quests.worldforge.json`
- `context/formats/world-forge-quests.md`, DEC-0026
- `command.cpp` project validate; `suite_tests.cpp` world_forge; CMakeLists

### Leftover

Dialogue file validation of hook IDs; World Forge Quests editor tab (0051).

## Agent notes

2026-07-15: Owner chose link model A + multi-stage dialogues + dialogue may parent to a quest. Implemented schema; awaiting approval.
