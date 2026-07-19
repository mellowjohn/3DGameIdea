# TICKET-0200: Scripted floating vessels + shore materials

- Epic: EPIC-0016
- Status: proposed
- Agent: unassigned
- Priority: P3
- Notion: (mirror when pulled)

## Goal

Scripted ships/ferries that float on water surfaces with believable presentation, plus mud/sand shore material transitions at coastlines ([DEC-0039](../../decisions/index.md#DEC-0039-water-swim-and-hydrology-authoring)); vertical slice for SQ-10 ferry.

## Context links

- [`water-hydrology.md`](../../features/water-hydrology.md)
- [`side-quest-catalog.md`](../../story/side-quest-catalog.md) â€” SQ-10
- [`lua-scripting.md`](../../features/lua-scripting.md)
- TICKET-0196, TICKET-0197, TICKET-0198

## Acceptance criteria

- [ ] Hull/prefab snaps to water surface at authored attachment point while moving on script path
- [ ] Sample ferry crossing script (Lua) between two dock anchors
- [ ] Shore band: mud or sand material transition where terrain meets water (paint or auto band)
- [ ] Optional shore foam/wave accent when feasible on water shader
- [ ] SQ-10 pier â†’ islet path demonstrable in sample project (scripted, not physics sim)

## Out of scope

Full rigid-body buoyancy; naval combat; multiplayer boats.

## Dependencies

Blocked by TICKET-0196, TICKET-0197. Soft: TICKET-0199 for route metadata.

## Verification

Rebuild `engine`; play test ferry script; visual check shores in sample map.

## Agent notes

Realistic feel via scripted motion is explicit owner intent â€” not full physics sim.

