# TICKET-0273: Post-Landfall camp handoff graybox

- Epic: EPIC-0006
- Status: proposed
- Agent: unassigned
- Priority: P1
- Notion: pending mirror to Tickets database

## Goal

Graybox the post-Landfall campsite where the player wakes after the Creotar vision and Arkand introduces the storage, rest, companion-talk, and travel handoff.

## Context links

- `context/story/campaign-beat-sheet.md` — A0-09 and A1-01
- `context/decisions/index.md` — DEC-0033 (anywhere player camp)
- `context/design/dom-answered-questions.md` — Act 0 camp handoff lock
- TICKET-0272 — vision departure handoff
- TICKET-0237 — Act 0 item/storage-facing slice
- TICKET-0280 — camp decompression, Arkand callback, and consequence-recap contract

## Acceptance criteria

- [ ] The graybox contains named landmarks for the wake-up position, Arkand tutorial position, storage chest, rest point, companion-talk positions, and map-travel exit.
- [ ] The layout supports a short, non-combat tutorial loop from wake-up through storage and rest to travel, with no forced non-companion NPC.
- [ ] The camp edge and travel landmark read as a safe handoff outside the ruined Calrenoth event space; the ticket does not lock exact world coordinates.
- [ ] The scene is documented as the reusable travel-only camp instance from DEC-0033, not a combat-escape mechanic.
- [ ] Live scene/editor MCP authoring is followed and `engine validate --project samples/open-world-rpg` succeeds.
- [ ] Editor play-test and screenshots demonstrate the wake-up loop, utility landmarks, and travel exit.

## Out of scope

- Full camp persistence, companion relationship systems, combat encounters, or camp attacks.
- Final foliage, tents, props, lighting, character art, voice, or dialogue.
- Act 1 Ledgeport and broader open-world grayboxing.

## Dependencies

- Blocked by TICKET-0280 for the approved decompression duration, Arkand callbacks, and Larrell outcome recap.
- Receives the vision-to-wake-up handoff from TICKET-0272.
- Soft dependency: TICKET-0237 for storage/inventory interaction wiring.
- Parallel with later Act 1 route and hub graybox work.

## Verification

- `engine validate --project samples/open-world-rpg`
- Editor play-test from wake-up through each tutorial landmark to the travel exit, with screenshots.
- Record scene IDs/paths, landmark names, and screenshot evidence in **What changed** before requesting approval.

## What changed

Not started.

## Agent notes

Proposed from the owner-requested Act 0 MVP graybox tracker pass on 2026-08-05.
