# TICKET-0172: Dialogue schema v2 + choice editor

- Epic: EPIC-0006
- Status: proposed
- Agent: cursor-agent
- Priority: P3
- Notion: https://app.notion.com/p/39fd3efc569581329030e0bd29a510b9

## Goal

schemaVersion 2: choice visibility/availability/effects/one-time + reorder UI.

## Context links

- [DEC-0027](../../decisions/index.md#dec-0027-shared-world-forge-graph-camera)
- [context/formats/world-forge-dialogues.md](../../formats/world-forge-dialogues.md)
- TICKET-0053 baseline graph editor
- Related: TICKET-0165–0179 dialogue UX program

## Acceptance criteria

- [ ] Schema documented + validated; choice editor supports new fields + DnD reorder.

## Out of scope

Breaking sample without migration note.

## Dependencies

Owner override of M7 hold. Soft: 0052

## Verification

Rebuild \engine\; \engine_suite_tests --suite world_forge\ when code lands.

## What changed

(Pending implementation.)

## Agent notes

2026-07-16: Created as part of Dialogue Tree Editor UX program.
