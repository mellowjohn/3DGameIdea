# TICKET-0053: Dialogue graph editor and headless tests

- Epic: EPIC-0006
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ad3efc5695816f9991d1d0c0bce20f

## Goal

World Forge visual dialogue graph for an authored tree: canvas with nodes/choice edges, select/edit/add/remove, plus headless layout/mutation/reachability tests. Owner override of M7/P3 hold.

## Context links

- [DEC-0026](../../decisions/index.md#dec-0026-quest-owned-dialogue-hooks-multi-stage)
- [`context/formats/world-forge-dialogues.md`](../../formats/world-forge-dialogues.md)
- TICKET-0052 runtime/asset; relationship graph patterns in `world_forge_editor.cpp`
- Twine sample tree `dlg_act0_wrathful_conquest`

## Acceptance criteria

- [x] Dialogues pane offers a **Graph** view for the selected tree
- [x] Canvas: layered layout, pan/zoom, drag nodes, draw choice edges with labels
- [x] Select node → edit speaker/line/choices; add/remove node; link choice by click (or end choice)
- [x] Headless helpers: layout, add/remove node/choice, reachability — covered by `world_forge` suite
- [x] Save/reload still via `apply_world_forge_operation` (`kind=dialogues`)
- [x] Docs mention graph editor

## Out of scope

Play-test modal presentation; SQ tree authorship; undo stack; auto-layout persistence to JSON; full Twine re-import UI.

## Dependencies

Owner override. Soft depends on 0052 dialogues asset.

## Verification

- Rebuild `engine_core` / `engine_suite_tests` / `engine` — passed (pre-existing C4996 in `render_app.cpp`)
- `engine_suite_tests --suite world_forge` — **82/82** passed

## What changed

### Summary

Dialogues pane now has a **Graph** sub-view: layered canvas for the selected tree (Twine Act 0 works), with pan/zoom, drag, choice-edge drawing, and node/choice editing. Headless `dialogue_graph_edit` helpers power the UI and suite tests.

### Files / surfaces

- `include/engine/dialogue/dialogue_graph_edit.h`, `src/dialogue/dialogue_graph_edit.cpp`
- `include/engine/ui/world_forge_editor.h`, `src/ui/world_forge_editor.cpp` (Dialogues → Graph)
- `tests/suite_tests.cpp`, `context/formats/world-forge-dialogues.md`, CMakeLists

### Schema / API

- Headless: `layout_dialogue_graph`, `add/remove_dialogue_node`, `add/remove_dialogue_choice`, `dialogue_reachable_node_ids`
- Errors: `DIALOGUE-GRAPH-*`
- Layout positions remain ephemeral (not written to JSON)

### Leftover

Undo; persist layout; play-test modal binding; SQ dialogue trees.

## Agent notes

2026-07-15: Owner override; built on Twine-seeded 0052 sample.
