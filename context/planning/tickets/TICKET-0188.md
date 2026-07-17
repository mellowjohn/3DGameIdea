# TICKET-0188: Map canvas terrain height underlay

- Epic: EPIC-0002
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39fd3efc56958149b38be7ec4a30e919

## Goal

Show a greyscale terrain height underlay under the World Forge Map Canvas when editor terrain is available, without mutating terrain from World Forge.

## Context links

- TICKET-0187 Map spatial canvas
- `sample_terrain_height` / `TerrainEditStore`
- DEC-0019 (World Forge does not own Sculpt)

## Acceptance criteria

- [x] `WorldForgeViewportDrawContext` passes terrain edits + revision into Map Canvas
- [x] Low-res greyscale height bake from `sample_terrain_height` (with active sculpt edits)
- [x] Rebake when terrain revision bumps or anchors change extent
- [x] Canvas still usable with grid-only if sampling unavailable

## Out of scope

Live 3D orthographic Scene viewport; paint/foliage underlay; player HUD minimap.

## Dependencies

TICKET-0187 canvas.

## Verification

- Rebuild `engine`
- Manual: sculpt then open Map → Canvas; underlay reflects heights

## What changed

### Summary

Map Canvas draws a cached greyscale height field sampled from procedural + sculpt deltas under markers/links.

### Files / surfaces

**Modified:** `world_forge_editor.h/.cpp`, `render_app.cpp` (`terrain_height_revision`, draw context)

### Tests / rebuild

world_forge **158/158**; engine rebuilt; MCP restarted.

## Agent notes

Shipped with TICKET-0187.
