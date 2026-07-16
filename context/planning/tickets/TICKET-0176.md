# TICKET-0176: Dialogue validation lint + node badges

- Epic: EPIC-0006
- Status: proposed
- Agent: cursor-agent
- Priority: P3
- Notion: https://app.notion.com/p/39fd3efc569581579c38f10b045bb7bb

## Goal

Lint empty/missing/unreachable/loops/dupes; badges; click-to-jump.

## Context links

- [DEC-0027](../../decisions/index.md#dec-0027-shared-world-forge-graph-camera)
- [context/formats/world-forge-dialogues.md](../../formats/world-forge-dialogues.md)
- TICKET-0053 baseline graph editor
- Related: TICKET-0165–0179 dialogue UX program

## Acceptance criteria

- [ ] Validate command lists issues; badges on nodes; click jumps.

## Out of scope

Quest ref soft-fail policy changes.

## Dependencies

Owner override of M7 hold. Soft: 0052

## Verification

Rebuild \engine\; \engine_suite_tests --suite world_forge\ when code lands.

## What changed

(Pending implementation.)

## Agent notes

2026-07-16: Created as part of Dialogue Tree Editor UX program.
