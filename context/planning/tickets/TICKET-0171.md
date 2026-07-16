# TICKET-0171: Dialogue inline editing + port drag-create

- Epic: EPIC-0006
- Status: proposed
- Agent: cursor-agent
- Priority: P3
- Notion: https://app.notion.com/p/39fd3efc5695812a8628e47d3cd23365

## Goal

Double-click edit; Enter next node; drag from ports to create.

## Context links

- [DEC-0027](../../decisions/index.md#dec-0027-shared-world-forge-graph-camera)
- [context/formats/world-forge-dialogues.md](../../formats/world-forge-dialogues.md)
- TICKET-0053 baseline graph editor
- Related: TICKET-0165–0179 dialogue UX program

## Acceptance criteria

- [ ] Inline edits + port create without Inspector for basic fields.

## Out of scope

Advanced Inspector replacement.

## Dependencies

Owner override of M7 hold. Soft: 0166

## Verification

Rebuild \engine\; \engine_suite_tests --suite world_forge\ when code lands.

## What changed

(Pending implementation.)

## Agent notes

2026-07-16: Created as part of Dialogue Tree Editor UX program.
