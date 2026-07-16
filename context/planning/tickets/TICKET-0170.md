# TICKET-0170: Dialogue connection styles + focus fade

- Epic: EPIC-0006
- Status: proposed
- Agent: cursor-agent
- Priority: P3
- Notion: https://app.notion.com/p/39fd3efc569581c5b4deea8fb213c74e

## Goal

Curved/orthogonal edges, reroutes, hover highlight, fade unrelated.

## Context links

- [DEC-0027](../../decisions/index.md#dec-0027-shared-world-forge-graph-camera)
- [context/formats/world-forge-dialogues.md](../../formats/world-forge-dialogues.md)
- TICKET-0053 baseline graph editor
- Related: TICKET-0165–0179 dialogue UX program

## Acceptance criteria

- [ ] Connection styles selectable; hover/select focus behaviors.

## Out of scope

Schema for edge styles.

## Dependencies

Owner override of M7 hold. Soft: 0165

## Verification

Rebuild \engine\; \engine_suite_tests --suite world_forge\ when code lands.

## What changed

(Pending implementation.)

## Agent notes

2026-07-16: Created as part of Dialogue Tree Editor UX program.
