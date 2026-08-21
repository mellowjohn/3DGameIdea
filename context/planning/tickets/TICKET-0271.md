# TICKET-0271: Calrenoth drawbridge-defense graybox

- Epic: EPIC-0006
- Status: proposed
- Agent: unassigned
- Priority: P1
- Notion: pending mirror to Tickets database

## Goal

Graybox the Landfall climax at Calrenoth's rear drawbridge: Larrell's holdout, readable defense lanes, the bridge mechanism, and the evacuation/hostage-fork exit.

## Context links

- `context/story/campaign-beat-sheet.md` — A0-06 and A0-07
- `context/art/concepts/README.md` — `act0-ld-a0-06-drawbridge-perspective.png` and drawbridge sceneset
- `context/art/blockbench-asset-list.md` — drawbridge kit and Calrenoth props
- TICKET-0270 — command-keep route handoff
- TICKET-0225 — Larrell save/hostage flag runtime
- TICKET-0272 — vision-space destination after the collapse
- TICKET-0280 — drawbridge encounter composition and immediate Larrell/Arkand consequence contract

## Acceptance criteria

- [ ] The lower-castle route terminates at a drawbridge-defense arena with a recognizable bridge, chain/spool mechanism, moat/land-spur boundary, and evacuation direction.
- [ ] The space supports three staged landmarks: Larrell's initial position, a player hold position, and a bridge-control interaction point.
- [ ] At least two defensible approach lanes and one evacuation route are legible in editor play-test without final combat encounters.
- [ ] The scene contains a named collapse/vision-transition marker and a named evacuation/hostage-fork marker; runtime choice logic remains external to this ticket.
- [ ] Live scene/editor MCP authoring is followed and `engine validate --project samples/open-world-rpg` succeeds.
- [ ] Editor screenshots show the defense overview, bridge mechanism, and both named transition markers.

## Out of scope

- Enemy spawning, combat tuning, bridge animation, and final interaction scripts.
- Final drawbridge meshes, siege effects, water rendering, or destruction art.
- The vision space and camp content tracked by TICKET-0272 and TICKET-0273.

## Dependencies

- Blocked by TICKET-0280 for the approved wave purpose/pacing, three-archetype contribution checks, and same-session consequence presentation.
- Blocked by the lower-castle exit from TICKET-0270.
- Soft dependency: TICKET-0225 for the eventual Larrell consequence and TICKET-0221/0222 for the transition event.
- Blocks TICKET-0272's entry marker.

## Verification

- `engine validate --project samples/open-world-rpg`
- Editor play-test across all defense lanes and the evacuation direction; capture overview and marker screenshots.
- Record scene IDs/paths, marker names, and screenshot evidence in **What changed** before requesting approval.

## What changed

Not started.

## Agent notes

Proposed from the owner-requested Act 0 MVP graybox tracker pass on 2026-08-05.
