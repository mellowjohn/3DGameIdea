# TICKET-0179: Large dialogue graph performance

- Epic: EPIC-0006
- Status: proposed
- Agent: cursor-agent
- Priority: P3
- Notion: https://app.notion.com/p/39fd3efc569581c2b1adc99987453784

## Goal

Virtualize, cache text, efficient edges, incremental layout for 500-2000 nodes.

## Context links

- [DEC-0027](../../decisions/index.md#dec-0027-shared-world-forge-graph-camera)
- [context/formats/world-forge-dialogues.md](../../formats/world-forge-dialogues.md)
- TICKET-0053 baseline graph editor
- Related: TICKET-0165–0179 dialogue UX program

## Acceptance criteria

- [ ] Smooth pan/zoom on large trees; suite/bench notes.

## Out of scope

GPU graph renderer.

## Dependencies

Owner override of M7 hold. Soft: 0165

## Verification

Rebuild \engine\; \engine_suite_tests --suite world_forge\ when code lands.

## What changed

(Pending implementation.)

## Agent notes

2026-07-16: Created as part of Dialogue Tree Editor UX program.
