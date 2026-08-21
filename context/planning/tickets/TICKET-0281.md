# TICKET-0281: Responsive sculpt controls and terrain grid bulk editing

- Epic: EPIC-0009
- Status: active
- Agent: cursor-agent
- Priority: P1
- Notion: pending mirror — no Notion connector is available in this session

## Goal

Make height sculpting feel immediate and predictable, and let world builders make precise, undoable bulk terrain changes through a snapped rectangular grid in both the Sculpt UI and live MCP.

## Context links

- `context/features/terrain-authoring.md`
- `context/features/mcp-live-editor.md`
- `context/formats/terrain-edits.md`
- `context/features/agent-build-coordination.md`
- DEC-0003, DEC-0006, DEC-0010, DEC-0018
- TICKET-0274 — existing smart-sculpt and MCP batch operations
- TICKET-0279 — frame-chunked jobs for very large agent recipes

## Acceptance criteria

- [ ] A continuous Height or Flatten drag coalesces terrain mesh/collision reloads instead of synchronously rebuilding every loaded affected cell on every mouse sample; releasing the pointer performs one final reload for all touched loaded cells.
- [ ] The Sculpt Height UI offers explicit **Raise** and **Lower** direction controls while retaining Shift as a temporary inverse; the active direction is visible before a stroke starts.
- [ ] Sculpt can show the authoritative 1.25 m terrain-sample grid and snap the brush/selection to it without changing the 33×33 samples per 40 m terrain-cell format.
- [ ] A rectangular grid selection supports Raise, Lower, Flatten, Smooth, and Terrace as one previewed, undoable height operation; an invalid or empty region fails without mutating terrain.
- [ ] `engine_terrain_apply` exposes equivalent bounded rectangular region actions with documented arguments, one coalesced height undo entry, and one reload callback per successful request; invalid bounds return a stable diagnostic.
- [ ] Diagnostics → Performance identifies Sculpt strokes and reports raw brush-edit time, final terrain mesh + collision reload time, pending/touched/reloaded cell counts, and the last height action; **Copy Sculpt Report** places those facts and a likely cause on the clipboard.
- [ ] The terrain and automation suites cover coalesced-region action semantics and invalid-region rejection; `engine` rebuilds successfully.
- [ ] `context/features/terrain-authoring.md`, `context/features/mcp-live-editor.md`, and `context/formats/terrain-edits.md` describe the delivered UI/API behavior and retain the live-editor ownership rule.

## Out of scope

- Lasso/freeform selection, imported heightmaps, erosion, biome generators, or terrain-director tooling.
- Changing terrain resolution, cell size, persisted terrain-edit schema, streaming radius, or collision representation.
- Replacing the existing async MCP job runner; oversized agent recipes continue through TICKET-0279's job path.

## Dependencies

- Builds on the existing `TerrainEditStore`, `TerrainEditHistory`, and `engine_terrain_apply` paths.
- TICKET-0274 and TICKET-0279 remain compatible; neither blocks this work.
- Requires the shared build lease before rebuilding `engine`.

## Verification

- Rebuild `engine` under the shared build lease.
- Run `engine_suite_tests --suite terrain` and `engine_suite_tests --suite automation`.
- Desktop QA: hold a wide raise and lower stroke over loaded cells; confirm the editor remains responsive, the terrain catches up on release, and Undo restores the full stroke.
- Live MCP: apply one valid region action and one malformed region request; confirm the former is one undoable operation and the latter returns the documented error without a reload.

## What changed

Not implemented. This section will be completed before `needs-approval`.

## Agent notes

Owner selected rectangle-only grid editing and reported noticeable raise/lower lag on 2026-08-10. First implementation step is drag-reload coalescing because it is independent of the new region-selection interaction.
