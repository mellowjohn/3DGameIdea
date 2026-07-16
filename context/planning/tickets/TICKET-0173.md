# TICKET-0173: Dialogue inspector collapsible + Advanced Mode

- Epic: EPIC-0006
- Status: proposed
- Agent: cursor-agent
- Priority: P3
- Notion: https://app.notion.com/p/39fd3efc5695819c8972e87245ad1a7e

## Goal

Collapsible Inspector sections; hide technical IDs unless Advanced.

## Context links

- [DEC-0027](../../decisions/index.md#dec-0027-shared-world-forge-graph-camera)
- [context/formats/world-forge-dialogues.md](../../formats/world-forge-dialogues.md)
- TICKET-0053 baseline graph editor
- Related: TICKET-0165–0179 dialogue UX program

## Acceptance criteria

- [ ] Sections collapse; Advanced Mode toggles IDs.

## Out of scope

Schema fields for Voice/Anim until 0174.

## Dependencies

Owner override of M7 hold. Soft: 0172

## Verification

Rebuild \engine\; \engine_suite_tests --suite world_forge\ when code lands.

## What changed

(Pending implementation.)

## Agent notes

2026-07-16: Created as part of Dialogue Tree Editor UX program.
