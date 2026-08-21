# TICKET-0270: Calrenoth command keep graybox

- Epic: EPIC-0006
- Status: proposed
- Agent: unassigned
- Priority: P1
- Notion: pending mirror to Tickets database

## Goal

Graybox the Calrenoth command-keep encounter so the Grenge/Damius briefing reads clearly and hands the player into the lower-castle retreat route.

## Context links

- `context/story/campaign-beat-sheet.md` — A0-04 and A0-05
- `context/art/concepts/README.md` — Act 0 Landfall concept inventory
- `context/art/blockbench-asset-list.md` — Tier 2 Calrenoth kit
- TICKET-0269 — siege-approach route and entry handoff
- TICKET-0271 — lower-castle/drawbridge continuation
- TICKET-0280 — Landfall player-experience pacing and recovery-beat contract

## Acceptance criteria

- [ ] The scene has a distinct command-keep briefing area with readable staging points for Commander Grenge, Damius, Arkand, and the player.
- [ ] A player route is grayboxed from the exterior entry through the briefing area to one clearly marked lower-castle exit.
- [ ] The briefing area communicates an active siege using only graybox-scale sightlines, cover/obstruction, and landmark placement; no final art requirement is implied.
- [ ] Scene authoring uses the live scene/editor MCP workflow and `engine validate --project samples/open-world-rpg` succeeds.
- [ ] An editor play-test and screenshots demonstrate the exterior-entry handoff, briefing composition, and lower-castle exit.

## Out of scope

- Final dialogue, branching outcomes, NPC behavior, or cinematics.
- Final Calrenoth architecture, decals, destruction meshes, lighting, and audio.
- The Larrell choice/drawbridge defense implemented by TICKET-0271.

## Dependencies

- Blocked by TICKET-0280 for the approved briefing duration, recovery window, and lower-castle handoff pacing.
- Blocked by the entry landmark and route contract from TICKET-0269.
- Soft dependency: TICKET-0221 / TICKET-0222 for eventual briefing staging.
- Blocks TICKET-0271's lower-castle route.

## Verification

- `engine validate --project samples/open-world-rpg`
- Editor play-test from the exterior entry to the lower-castle exit, plus screenshots of the briefing composition and route.
- Record scene IDs/paths, route handoff, and screenshot evidence in **What changed** before requesting approval.

## What changed

Not started.

## Agent notes

Proposed from the owner-requested Act 0 MVP graybox tracker pass on 2026-08-05.
