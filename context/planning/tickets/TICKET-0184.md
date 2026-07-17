# TICKET-0184: World Forge Hierarchy pane (Religion/Factions/Persons)

- Epic: EPIC-0002
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39fd3efc56958103ba95da270adb0f1c

## Goal

Add World Forge **Hierarchy** top-level tab with Religion / Factions / Persons authorship sub-pages (tree + detail). Remove flat top-level Factions tab.

## Context links

- TICKET-0183 pantheon, TICKET-0185 node parentId
- `src/ui/world_forge_editor.cpp`
- `context/features/editor-mvp.md`
- DEC-0035

## Acceptance criteria

- [x] Hierarchy pane with nested Religion | Factions | Persons
- [x] Each page: ImGui tree from parentId + detail inspector + quick-create
- [x] Factions detail keeps standing/politicalRole; parent reparent via dropdown
- [x] Top-level Factions tab removed
- [x] Save/Reload via `apply_world_forge_operation` including pantheon

## Out of scope

Drag-drop reparent; Graph canvas changes; runtime faith.

## Dependencies

TICKET-0183 (Religion), TICKET-0185 (Persons parentId).

## Verification

- Rebuild `engine` — passed.
- `--suite world_forge` — 139/139.
- Hierarchy UI is manual (no automated GUI test).

## What changed

### Summary

World Forge top-level tabs are now Hierarchy / Relationships / Map / Quests / Dialogues. Hierarchy nests Religion, Factions, and Persons authorship pages with parentId forests, detail inspectors, and quick-create. Flat Factions tab removed.

### Files / surfaces

**Modified:** `world_forge_editor.h/.cpp`, editor-mvp, world-forge-scope, DEC-0035

### Schema / API

Session loads/saves pantheon alongside other World Forge assets.

### Leftover risk

Manual Hierarchy smoke in editor recommended; drag-drop reparent deferred.

## Agent notes

Shipped with TICKET-0183/0185.
