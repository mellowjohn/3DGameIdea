# TICKET-0209: Editor + player UI Pencil chrome from rpg-engine-ui.pen

- Epic: EPIC-0009
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: (mirror after stub)

## Goal

Ship the Pencil redesign in [`context/design/rpg-engine-ui.pen`](../../design/rpg-engine-ui.pen) into the live editor and sample player canvases: shared dark/gold ImGui chrome, branded application header, and parchment-styled HUD/modals — without reworking World Forge Map Canvas (TICKET-0208 owns that surface).

## Context links

- `context/design/rpg-engine-ui.pen`
- `context/design/world-forge-map-canvas.pen` (token sibling)
- `context/features/editor-mvp.md`
- `context/features/ui-canvas.md`
- Related: TICKET-0208, TICKET-0155–0164

## Acceptance criteria

- [x] Editor ImGui style uses Pencil tokens (chrome/panel/gold/bronze/text)
- [x] Branded **RPG ENGINE** header strip under the menu bar with project / active viewport / Save
- [x] Sample player canvases (`player`, `pause`, `main_menu`, `settings`, `dialogue`, `inventory`) use parchment/gold colors
- [x] HUD runtime defaults match the same language when widgets omit `color`
- [x] Rebuild `engine`; docs updated

## Out of scope

Full layout rebuild of Hierarchy/Inspector into new shells; World Forge Map Canvas; new uicanvas schema fields; GPU textures for button images.

## Dependencies

Soft: TICKET-0208 chrome tokens (already landed).

## Verification

`engine` Debug rebuild succeeded (C4996 getenv/sscanf pre-existing). MCP relaunched against `samples/open-world-rpg`.

## What changed

- Summary: Live editor now uses Pencil chrome from `rpg-engine-ui.pen` — shared ImGui palette, branded header (project / active tab / Save), and parchment/gold sample HUD + modal canvases. HUD defaults follow the same tokens when widgets omit `color`.
- Files / surfaces: `include/engine/ui/editor_chrome.h`, `src/ui/editor_chrome.cpp`, `src/rendering/render_app.cpp`, `src/ui/hud_runtime.cpp`, `CMakeLists.txt`, sample `assets/ui/*.uicanvas.json`, `editor-mvp.md`, `epics.md`.
- Schema / API / format deltas: hotspot `Editor.Header.Save`; no schema change.
- Seed / sample data: player/pause/main_menu/settings/dialogue/inventory canvas colors updated.
- Tests / verification evidence: MSBuild `engine` Debug OK.
- Decisions & tradeoffs: Did not rebuild Hierarchy/Inspector into new shells — style + header first; World Forge keeps its own header.
- Leftover risk / follow-ons: Visual smoke of play HUD/pause; optional Notion mirror; deeper shell layout later.

## Agent notes

Status → needs-approval after owner smoke.
