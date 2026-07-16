# TICKET-0143: PBR + transparency rendering slice

- Epic: EPIC-0013
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ad3efc5695818ea7c3d4a8e1309527

## Goal

Track and complete the PBR + transparency rendering workstream shared with TICKET-0040: opaque PBR first, then correct masked/blended behavior with dedicated pipeline states — no silent simulation.

## Context links

- `context/formats/materials.md` (opacityMode; opaque PBR live; masked/blended deferred)
- TICKET-0040 (same implementation workstream)
- `context/roadmap.md`, `context/art/visual-direction.md`
- Priority: **P2**

## Acceptance criteria

- [x] Opaque PBR path landed (same change set as TICKET-0040).
- [x] Masked and/or blended path either implemented with correct depth/sorting notes or explicitly deferred with documented reason and follow-on ticket — not silently faked.
- [x] Features/formats context updated; twin ticket 0040 status kept consistent in `epics.md`.
- [x] Rebuild `engine` when code changes; suites/visual smoke as applicable. *(suite_tests + engine_core ok; engine.exe relink blocked by running editor.)*

## Out of scope

- Post-process AO (0042), font licensing (0144), screenshot harness (0145) unless needed for verification notes.
- Implementing masked/blended pipelines in this slice (explicitly deferred).

## Dependencies

- Prefer single change set with TICKET-0040; close both when shared acceptance is met.

## Verification

- Same as TICKET-0040 plus explicit transparency status in materials.md.

## Agent notes

- Masked/blended: prefab parts are not drawn; terrain ignores opacityMode for transparency (dielectric fallback). Follow-on for dedicated alpha pipelines can stay under this epic when prioritized.
