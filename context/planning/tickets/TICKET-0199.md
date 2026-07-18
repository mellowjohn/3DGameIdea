# TICKET-0199: World Forge hydrology + ferry route map authoring

- Epic: EPIC-0015
- Status: proposed
- Agent: unassigned
- Priority: P2
- Notion: (mirror when pulled)

## Goal

Extend World Forge Map canvas to plan hydrology (river/lake/sea regions) and ferry route polylines linked to dock POIs — metadata only; meshes stay in Sculpt ([DEC-0038](../../decisions/index.md#dec-0038-water-swim-and-hydrology-authoring)).

## Context links

- [`world-forge-scope.md`](../../features/world-forge-scope.md)
- [`world-forge-map.md`](../../formats/world-forge-map.md)
- TICKET-0187/0188 Map canvas
- SQ-10 Island Ferry — [`side-quest-catalog.md`](../../story/side-quest-catalog.md)

## Acceptance criteria

- [ ] Schema fields for hydrology region types and ferry routes (validated JSON)
- [ ] Map canvas draw/edit regions and route polylines with stable IDs
- [ ] Ferry routes reference start/end dock POI ids
- [ ] Act lens filter applies to hydrology objects
- [ ] MCP `engine_world_forge_apply` or documented mutation path
- [ ] No mesh placement from World Forge

## Out of scope

Automatic Sculpt carve from map polylines; runtime ferry scripting (TICKET-0200).

## Dependencies

Soft: TICKET-0187 map canvas. Parallel with TICKET-0197.

## Verification

Validate sample hydrology + ferry route in open-world-rpg; World Forge UI smoke.

## Agent notes

Schema field names open — finalize in format doc during implementation.
