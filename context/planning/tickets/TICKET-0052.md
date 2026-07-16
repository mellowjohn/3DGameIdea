# TICKET-0052: Branching dialogue runtime

- Epic: EPIC-0006
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ad3efc569581c99c66c32a254cfdbe

## Goal

Ship `dialogues.worldforge.json` + headless `DialogueRuntime`, seeded from Twine Act 0 as the main-quest dialogue spine (`dlg_act0_wrathful_conquest` → `mq_act0_calrenoth`).

## Context links

- [DEC-0026](../../decisions/index.md#dec-0026-quest-owned-dialogue-hooks-multi-stage)
- [`context/formats/world-forge-dialogues.md`](../../formats/world-forge-dialogues.md)
- Twine source + importer: `context/story/sources/wrathful-conquest-act0.twee`, `tools/twee_to_world_forge_dialogues.py`
- Pairs with TICKET-0051 tooling; graph editor is TICKET-0053

## Acceptance criteria

- [x] Schema documented (`world-forge-dialogues.md`)
- [x] C++ `WorldForgeDialoguesAsset` parse/validate/save + default path
- [x] `DialogueRuntime` headless bind/start/present/choose with `setFlags`
- [x] Sample Twine Act 0 tree with `parentQuestId=mq_act0_calrenoth`
- [x] Project validate includes dialogues (parentQuestId soft-check vs quests)
- [x] MCP `kind=dialogues` + World Forge Dialogues list pane (metadata; not full graph)
- [x] `world_forge` suite covers asset rejection cases + runtime walk of prologue→tutorial

## Out of scope

Full dialogue graph canvas (0053); UI modal presentation wiring beyond existing canvas stub; SQ-01/SQ-02 tree authorship; voice/localization keys.

## Dependencies

Owner override of M5/M7 hold. Soft depends on 0050/0051 quest ids for `parentQuestId`.

## Verification

- Rebuild `engine_core` / `engine_suite_tests` — passed (pre-existing C4996 in `render_app.cpp`)
- `engine_suite_tests --suite world_forge` — **87/87** passed
- `engine validate --project samples/open-world-rpg` — exit 0
- `engine.exe` relink blocked (LNK1168 — process holds the file); library + suites verified

## What changed

### Summary

Dialogue trees are a first-class World Forge asset. Act 0 conversation is imported from Twine (52 nodes) and walkable headlessly. Quests and dialogues cross-validate via DEC-0026 links.

### Files / surfaces

- `include/engine/assets/world_forge_dialogues_asset.h`, `src/assets/world_forge_dialogues_asset.cpp`
- `include/engine/dialogue/dialogue_runtime.h`, `src/dialogue/dialogue_runtime.cpp`
- `samples/.../dialogues.worldforge.json`, `tools/twee_to_world_forge_dialogues.py`
- Editor Dialogues pane; MCP `kind=dialogues`; project validate hook
- Format doc + MCP doc updates

### Schema / API

- Tree/node/choice JSON; `parentQuestId`; errors `WORLD-FORGE-DLG-*`
- Runtime errors `DIALOGUE-RT-*`
- Empty `nextNodeId` completes the session

### Seed

- One tree: `dlg_act0_wrathful_conquest` from Twine (draft canon)

### Leftover

Graph editor polish (0053); SQ dialogue trees (hooks intentionally empty until authored); play-test modal binding.

## Agent notes

2026-07-15: Owner confirmed Twine is the main-quest dialogue seed for starting 0051/0052.
2026-07-15: Leaf nodes with empty choices mark DialogueRuntime complete on arrival.
