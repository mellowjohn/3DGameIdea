# TICKET-0167: Dialogue graph navigation

- Epic: EPIC-0006
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39fd3efc5695816aabc7d9b7f16a1b98

## Goal

Search, bookmarks, back/forward history, zoom-to-selected, jump to linked nodes.

## Context links

- [DEC-0027](../../decisions/index.md#dec-0027-shared-world-forge-graph-camera)
- [context/formats/world-forge-dialogues.md](../../formats/world-forge-dialogues.md)
- TICKET-0053 baseline graph editor
- Related: TICKET-0165–0179 dialogue UX program

## Acceptance criteria

- [x] Search by speaker/line/id/flags; history; bookmarks; zoom/frame/jump commands work.

## Out of scope

Schema search fields.

## Dependencies

Owner override of M7 hold. Soft: 0165

## Verification

Rebuild `engine`; `engine_suite_tests --suite world_forge` — **93/93** passed. `engine.exe` LNK1168 while process open.

## What changed

### Summary
Phase 1 dialogue graph UX: shared World Forge graph camera/minimap (DEC-0027), Compact/Standard/Expanded node chrome, search/bookmarks/nav history, toolbar + shortcuts + mutation undo stack. Schema remains v1.

### Files / surfaces
- include/engine/ui/world_forge_graph_camera.h, src/ui/world_forge_graph_camera.cpp (new)
- include/engine/dialogue/dialogue_graph_edit.h, src/dialogue/dialogue_graph_edit.cpp (search, duplicate, display modes)
- include/engine/ui/world_forge_editor.h, src/ui/world_forge_editor.cpp (Dialogues Graph + relationship minimap)
- tests/suite_tests.cpp, context/formats/world-forge-dialogues.md, context/features/world-forge-scope.md, context/decisions/index.md, epics.md

### Schema / API
- Headless: duplicate_dialogue_node, dialogue_search_node_ids, dialogue_node_card_size, infer_dialogue_node_kind
- WorldForgeGraphCamera fit/center/zoom/minimap helpers
- No dialogues.worldforge.json schema bump

### Tests / verification
- engine_core rebuilt (engine.exe LNK1168 if process open)
- world_forge suite 93/93

### Leftover risk / follow-ons
- Preview stub until 0177; full align suite 0169; schema v2 0172+
- engine.exe link blocked when editor running

## Agent notes
2026-07-16: Phase 1 delivered under owner override; awaiting owner done.
\n\n