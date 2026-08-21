# TICKET-0272: Realm of Darkness vision graybox

- Epic: EPIC-0006
- Status: proposed
- Agent: unassigned
- Priority: P1
- Notion: pending mirror to Tickets database

## Goal

Create a minimal isolated graybox for Creotar's Realm of Darkness vision, with clear staging and camera landmarks for the post-drawbridge narrative beat.

## Context links

- `context/story/campaign-beat-sheet.md` — A0-08
- `context/story/nefarium-and-the-shroud.md` — Creotar/Shroud truth boundary
- `context/story/prologue-and-opening.md` — Creotar/Frangitur dramatic irony
- TICKET-0221 / TICKET-0222 — event timeline, camera path, and control lock
- TICKET-0271 — drawbridge collapse transition
- TICKET-0273 — camp-handoff destination

## Acceptance criteria

- [ ] An isolated vision graybox has a named arrival point, player conversation position, Creotar staging point, and named departure marker.
- [ ] The space uses a deliberately small set of neutral geometry and sightlines that frames Creotar without presenting final sky, VFX, character, or Shroud art.
- [ ] The arrival and departure markers are documented as the intended handoff from the drawbridge collapse and to the camp wake-up.
- [ ] At least one event-camera framing can be verified using existing editor camera/timeline tools, without committing final dialogue timing.
- [ ] Live scene/editor MCP authoring is followed and `engine validate --project samples/open-world-rpg` succeeds.
- [ ] Screenshots show the arrival, Creotar framing, and departure marker from the editor.

## Out of scope

- Final Creotar model, animation, voice, dialogue, particles, sky, or Shroud visuals.
- A final cinematic sequence or a new runtime vision/loading system.
- Camp placement and Arkand's tutorial, tracked by TICKET-0273.

## Dependencies

- Soft dependency: TICKET-0221 and TICKET-0222 for camera/event validation.
- Receives the named vision-transition marker from TICKET-0271.
- Blocks the spatial handoff into TICKET-0273.

## Verification

- `engine validate --project samples/open-world-rpg`
- Editor camera screenshot for arrival, conversation framing, and departure; record the camera/event references.
- Record scene IDs/paths, handoff markers, and screenshot evidence in **What changed** before requesting approval.

## What changed

Not started.

## Agent notes

Proposed from the owner-requested Act 0 MVP graybox tracker pass on 2026-08-05.
