# TICKET-0207: Map Canvas dual view + borders + travel routes

- Epic: EPIC-0002
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: (see Notion Tickets DB by Ticket ID)

## Goal

World Forge Map Canvas supports Cartography vs Top-down modes, region border polylines, travelRoutes (track/road/highway), kind-aware markers, and culture-aware labels — establishing the XZ world-placement bridge without spawning meshes.

## Context links

- `context/formats/world-forge-map.md`
- `context/formats/world-forge-factions.md`
- `context/art/cartography-design.md`
- `include/engine/assets/world_forge_map_asset.h`
- Related: TICKET-0187, TICKET-0188, TICKET-0205, TICKET-0061 (follow-on consumer)

## Acceptance criteria

- [ ] Schema: optional `region.border`, `travelRoutes[]`, faction `emblemPath` / `mapColor` / `mapTypefaceId`
- [ ] Canvas toggle Cartography | Top-down; parchment vs terrain underlay
- [ ] Draw/edit travel routes and borders; kind-aware markers; culture fonts when loaded
- [ ] Reference affordance for official Tessera map (not slice underlay)
- [ ] Rebuild `engine`; validate sample map/factions

## Out of scope

Player mini-map (0061); spawning Scene prefabs from canvas; true 3D Scene embed.

## Dependencies

TICKET-0187/0188; TICKET-0205 art/fonts.

## Verification

Rebuild engine; world_forge / project validate; manual Map Canvas smoke.

## Notion

- https://app.notion.com/p/3a3d3efc56958186a574c1de82845fb7

## What changed

- Summary: Map schema gained `region.border` and `travelRoutes` (track/road/highway); factions gained `emblemPath` / `mapColor` / `mapTypefaceId`. Map Canvas now toggles Cartography vs Top-down, draws borders/roads with kind-aware markers and culture fonts, plus an official-map Reference popup.
- Files: `world_forge_map_asset.*`, `world_forge_factions_asset.*`, `world_forge_editor.*`, `game_fonts.*`, format docs, sample factions/map JSON.
- Schema deltas: travel route kind enum; optional border polyline; faction cartography fields.
- Verification: `engine` Debug rebuild succeeded (MSB3026 PDB retry warnings only).

## Agent notes

Follow-on: Scene prefab snap from anchors/travelRoutes; TICKET-0061 consumes kit.

## Agent notes

Implementing schema + editor dual view in this pass.
