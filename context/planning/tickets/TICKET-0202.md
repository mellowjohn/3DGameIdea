# TICKET-0202: Water persistence + Sculpt water tool + MCP

- Epic: EPIC-0016
- Status: proposed
- Agent: unassigned
- Priority: P2
- Notion: (mirror when pulled)

## Goal

Persist authored water surfaces with one world-wide sea level, bounded region placement, Sculpt tool + undo/save, and MCP apply mirroring terrain patterns ([DEC-0018](../../decisions/index.md#dec-0018-mcp-terrain-sculpt-and-paint-apply)).

## Context links

- [`water-hydrology.md`](../../features/water-hydrology.md)
- [`terrain-authoring.md`](../../features/terrain-authoring.md)
- [`terrain-edits.md`](../../formats/terrain-edits.md) ΓÇö pattern reference
- TICKET-0201 (render pass)

## Acceptance criteria

- [ ] Versioned water asset JSON under `assets/terrain/` or documented path
- [ ] Sculpt tab **Water** tool: place, erase, set fill level; undo/redo; Ctrl+S save
- [ ] `sample_water_surface_y` / depth queries integrated with streaming cells
- [ ] Dry basins remain dry unless explicitly authored
- [ ] MCP mutate path with live editor (sample offline when closed)
- [ ] `terrain` or dedicated suite tests for round-trip + undo

## Out of scope

River spline carve UI; World Forge map overlays (TICKET-0204); swim mode.

## Dependencies

Blocked by TICKET-0201 for visible water. Blocks TICKET-0203, TICKET-0200.

## Verification

Rebuild `engine`; Sculpt place water in sample project; MCP batch sample; suite pass.

## Agent notes

Open: cell schema vs region mesh ΓÇö see open-questions hydrology section.

