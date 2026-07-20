# TICKET-0208: World Forge Pencil Map Canvas chrome revamp

- Epic: EPIC-0002
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/3a3d3efc569581239422e0e26cf70f72

## Goal

Implement the Pencil redesign in [`context/design/world-forge-map-canvas.pen`](../../design/world-forge-map-canvas.pen) as World Forge editor chrome: branded header, left navigation (including Overview), Map workspace title bar + tool rail, dedicated selection inspector, parchment on-map overlays, hover titles, and discrete-layer zoom affordances — without changing Scene/Sculpt ownership or map schema fundamentals.

## Context links

- `context/design/world-forge-map-canvas.pen` (frames 01–06)
- `context/art/cartography-design.md`
- `context/features/editor-mvp.md`
- `context/features/world-forge-scope.md`
- Related: TICKET-0015, TICKET-0187, TICKET-0205, TICKET-0207; follow-on TICKET-0061

## Acceptance criteria

- [x] World Forge shell: branded header + left nav (Overview, Hierarchy, Relationships, Map, Quests, Dialogues, Archetypes) + Act lens in nav; Reload/Save/status in header
- [x] Map Canvas: title bar with Cartography|Top-down; tool rail Select/Anchor/Route/Border/Water; cursor XZ + zoom controls; dedicated right inspector (320px)
- [x] Cartography overlays: parchment Map Layers legend, heraldry legend, draft geography badge, scale bar; marker titles on hover (selected always labeled)
- [x] Layer zoom: return-to-continent / active plate badge when not on continent plate
- [x] Culture fonts remain short map labels only; editor chrome stays Roboto
- [x] MCP hotspots updated for pane nav + map tools; rebuild `engine`

## Out of scope

Player mini-map (TICKET-0061); Scene prefab snap from canvas; redesign of Hierarchy/Quest/Dialogue graph card aesthetics; Resources pane; new map JSON schema fields.

## Dependencies

Soft: TICKET-0205 art kit (panel parchment), TICKET-0207 dual view.

## Verification

Rebuild `engine`; Map Canvas smoke vs Pencil frame 01; world_forge suite / project validate as applicable.

## What changed

- Summary: World Forge now matches the Pencil Map Canvas chrome — branded header with Reload/Save/status, left navigation (Overview + existing panes) with Act lens in the nav footer, Map title bar (Cartography|Top-down), tool rail (Select/Anchor/Route/Border/Water) with XZ readout and zoom, dedicated 320px inspector, parchment on-map legends/heraldry/draft/plate badges, hover title chips, scale bar, and Continent return control. Map Canvas is the default Map view.
- Files / surfaces: `include/engine/ui/world_forge_editor.h`, `src/ui/world_forge_editor.cpp`, `src/rendering/render_app.cpp` (panel parchment SRVs); docs `editor-mvp.md`, `cartography-design.md`, `mcp-live-editor.md`, `epics.md`, this stub.
- Schema / API / format deltas: new `WorldForgeEditorPane::Overview`, `WorldForgeMapTool` enum; session fields `map_tool`, `map_labels_on_hover`, legend/badge flags, cursor XZ; MCP hotspots `WorldForge.Pane.Overview`, `WorldForge.Map.Tool.*`, `WorldForge.Map.ReturnContinent`, `WorldForge.Header.*`.
- Seed / sample data: none (UI only); panel textures loaded from existing `assets/ui/cartography/panel/`.
- Tests / verification evidence: `engine` Debug rebuild succeeded (C4996 getenv/sscanf pre-existing; C4100 session unused in stroke helper). `world_forge` suite 172/177 — 5 failures are sample seed-count assertions unrelated to this UI change. Live MCP screenshot blocked (Cursor MCP namespace discovery error after relaunch).
- Decisions & tradeoffs: Kept ImGui; other panes reuse the new shell with existing body layouts; Overview is a lightweight status dashboard; Resources remains unwired.
- Leftover risk / follow-ons: Player mini-map (0061); Scene snap; polish Hierarchy/Dialogue aesthetics to parchment; live visual MCP verify when bridge reconnects.

## Agent notes

Pencil frames 02–06 remain design reference. Owner should smoke Map Canvas in the running editor against frame 01.
