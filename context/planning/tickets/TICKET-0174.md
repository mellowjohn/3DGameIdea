# TICKET-0174: Dialogue narrative metadata (Advanced)

- Epic: EPIC-0006
- Status: proposed
- Agent: cursor-agent
- Priority: P3
- Notion: https://app.notion.com/p/39fd3efc569581fc8760cbeaf7681d26

## Goal

Emotion/camera/voice/subtitle/anim metadata in Advanced; align 0118.

## Context links

- [DEC-0027](../../decisions/index.md#dec-0027-shared-world-forge-graph-camera)
- [context/formats/world-forge-dialogues.md](../../formats/world-forge-dialogues.md)
- TICKET-0053 baseline graph editor
- Related: TICKET-0165–0179 dialogue UX program

## Acceptance criteria

- [ ] Metadata editable in Advanced; does not clutter basic flow.

## Out of scope

Full voice pipeline.

## Dependencies

Owner override of M7 hold. Soft: 0118,0172

## Verification

Rebuild \engine\; \engine_suite_tests --suite world_forge\ when code lands.

## What changed

(Pending implementation.)

## Agent notes

2026-07-16: Created as part of Dialogue Tree Editor UX program.
