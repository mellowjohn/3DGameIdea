# TICKET-0187: World Forge Map spatial canvas

- Epic: EPIC-0002
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39fd3efc569581959456fedc868b5cd0

## Goal

Add a World Forge Map **Canvas** mode that authors region/POI world anchors and draws travel/soft-gate links on an XZ top-down overlay (grid first), without mesh placement.

## Context links

- `context/formats/world-forge-map.md` (existing `anchor` fields)
- `context/features/world-forge-scope.md`, `context/features/editor-mvp.md`
- DEC-0003, DEC-0019, DEC-0027 (shared graph camera)
- Follow-on: TICKET-0188 terrain underlay

## Acceptance criteria

- [x] Map pane List | Canvas view modes
- [x] Canvas pan/zoom (wheel, Alt/middle pan), Fit, minimap
- [x] Place/drag region and POI anchors; persist via Save/`apply_world_forge_operation`
- [x] Links drawn between anchored endpoints; selectable
- [x] Unanchored side strip (“Place on map”)
- [x] Anchor xyz editable in List detail and Canvas detail
- [x] Helper tests for endpoint anchor resolve + marker keys + camera round-trip

## Out of scope

Terrain underlay (0188); polygon region areas; mesh placement; player HUD minimap (EPIC-0007).

## Dependencies

TICKET-0013 map schema (anchors). Soft: DEC-0027 camera helpers.

## Verification

- Rebuild `engine` + `engine_suite_tests`
- `--suite world_forge` includes map anchor resolve helpers
- Manual: World Forge → Map → Canvas place/move anchors, Save/Reload

## What changed

### Summary

World Forge Map gained a spatial Canvas mode: XZ overlay with region/POI markers and link lines, authored through existing `anchor` fields on `map.worldforge.json`.

### Files / surfaces

**Modified:** `world_forge_editor.h/.cpp`, suite tests, format/scope/editor-mvp docs

### Schema / API

No schema version bump — uses existing optional `anchor`. Helpers: `resolve_map_endpoint_anchor`, `map_region_marker_key`, `map_poi_marker_key`.

### Tests / rebuild

world_forge **158/158**; engine rebuilt; MCP restarted.

## Agent notes

Shipped with TICKET-0188 in one change set.
