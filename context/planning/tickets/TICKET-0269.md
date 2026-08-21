# TICKET-0269: Landfall prologue and siege-approach graybox

- Epic: EPIC-0006
- Status: proposed
- Agent: unassigned
- Priority: P1
- Notion: pending mirror to Tickets database

## Goal

Create the first playable Landfall graybox sequence: a minimal prologue tableau followed by Arkand's rescue and a readable, under-fire approach into Calrenoth.

## Context links

- `context/story/campaign-beat-sheet.md` — A0-01, A0-03, and A0-04
- `context/art/concepts/README.md` — Act 0 concept inventory
- `context/art/blockbench-asset-list.md` — Act 0 Calrenoth kit boundaries
- TICKET-0280 — Landfall player-experience pacing, optional discovery, and approach encounter contract
- TICKET-0221 / TICKET-0222 — event timeline and camera-control support
- TICKET-0270 — downstream command-keep graybox

## Acceptance criteria

- [ ] The sample project contains a documented graybox route from the Arkand rescue beat to a Calrenoth gate/entry landmark, with no ambiguous alternate route.
- [ ] The route includes distinct landmark volumes for the rescue, siege approach, and fortress entry; their names and positions are recorded in an Act 0 graybox note or scene manifest.
- [ ] The prologue tableau has only the spatial/camera landmarks needed for A0-01; it does not claim final character, VFX, or cinematic assets.
- [ ] Existing scene, terrain, prefab, and event-timeline MCP operations are used for authoring; `engine validate --project samples/open-world-rpg` succeeds.
- [ ] An editor play-test and screenshots verify that the approach reads as an exterior siege route and reaches the Calrenoth entry.

## Out of scope

- Final Calrenoth art, materials, lighting, bespoke props, enemies, or combat tuning.
- Character-creation UI and final prologue dialogue/cinematics.
- The command-keep, drawbridge, vision, and camp spaces tracked by TICKET-0270 through TICKET-0273.

## Dependencies

- Blocked by TICKET-0280 for the approved approach pacing, optional detour, and discovery landmark contract.
- Requires a live editor session and current scene/terrain MCP surface.
- Soft dependency: TICKET-0221 / TICKET-0222 for cinematic hookup; graybox placement may proceed without final event content.
- Blocks TICKET-0270's route handoff.

## Verification

- `engine validate --project samples/open-world-rpg`
- Editor play-test from the rescue landmark to the Calrenoth entry, with screenshots of the rescue, approach, and entry landmarks.
- Record all authored scene IDs/paths and screenshot evidence in **What changed** before requesting approval.

## What changed

Not started.

## Agent notes

Proposed from the owner-requested Act 0 MVP graybox tracker pass on 2026-08-05.
