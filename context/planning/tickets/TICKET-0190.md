# TICKET-0190: Scene overlay for World Forge map markers

- Epic: EPIC-0002
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39fd3efc569581978f98ea19b973602d

## Goal

Show World Forge region/POI anchors as editor-only markers in the Scene (and Sculpt) viewport, with an optional camera focus on the selected marker.

## Context links

- `context/formats/world-forge-map.md` (anchors)
- `context/features/world-forge-scope.md`, `context/features/editor-mvp.md`
- TICKET-0187 Map Canvas; DEC-0036 Act lens
- Pattern: collision debug overlay in Diagnostics

## Acceptance criteria

- [x] Diagnostics toggle: **Show World Forge map markers** (default on)
- [x] Scene/Sculpt draw poles + labels for anchored regions (square) and POIs (circle)
- [x] Respects World Forge Act lens filter
- [x] Auto-loads map asset if not yet loaded
- [x] **Focus selected WF marker** button orbits Scene camera to selected anchored marker
- [x] No mesh placement; editor-only (not Game / player mini-map)

## Out of scope

Player HUD mini-map (TICKET-0061); spawning scene entities from markers; dragging anchors in Scene.

## Dependencies

Map anchors (TICKET-0013/0187). Soft: Act lens (TICKET-0189).

## Verification

- Rebuild `engine` (MSBuild Debug) — passed (C4996 warnings in render_app only).
- Manual: Scene viewport with sample anchors; Diagnostics toggle + focus button.

## What changed

### Summary

Scene and Sculpt viewports can show World Forge map anchors as labeled poles (regions = squares, POIs = circles). Diagnostics has a toggle (on by default) and a **Focus selected WF marker** button that frames the edit camera on the selected anchored region/POI.

### Files / surfaces

Modified `src/rendering/render_app.cpp`; docs: editor-mvp, world-forge-scope, world-forge-map, coverage, epics, this stub.

### Schema / API

No asset schema change. Session flags: `show_world_forge_map_markers`, `request_focus_world_forge_marker`.

### Seed / sample

Uses existing sample map anchors.

### Tests / rebuild

engine rebuilt; editor restarted. No new automated suite (draw overlay is manual).

### Decisions & leftovers

Editor-only overlay; Act lens still applies. Scene click-to-select and Scene drag of anchors deferred.

## Agent notes

Owner chose Scene overlay (option 1).
