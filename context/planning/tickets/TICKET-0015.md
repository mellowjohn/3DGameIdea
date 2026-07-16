# TICKET-0015: World Forge editor viewport tab (shell)

- Epic: EPIC-0002
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ed3efc5695812095cfef8e0171e79f

## Goal

Add a **World Forge** viewport tab alongside Scene / Sculpt / Game / UI with toolbar and Factions / Relationships / Map sub-tabs that load default `*.worldforge.json` assets.

## Context links

- `context/features/world-forge-scope.md`
- `include/engine/ui/world_forge_editor.h`
- TICKET-0016 (inspectors), TICKET-0017 (graph canvas)

## Acceptance criteria

- [x] `ViewportTab::WorldForge` + Viewports tab item
- [x] Center content replaces 3D image when active
- [x] Sub-tabs for Factions / Relationships / Map
- [x] Reload loads default project World Forge paths
- [x] Disabled during play test (like UI/Sculpt)
- [x] Context indexes / feature docs updated

## Out of scope

Rich field editing (TICKET-0016 ships with this MVP); visual graph canvas (0017).

## Dependencies

TICKET-0011–0014.

## Verification

Rebuild `engine`; manual tab smoke.

## What changed

- Added `include/engine/ui/world_forge_editor.h` + `src/ui/world_forge_editor.cpp` (new CMake source, added to `engine_core` beside `ui_canvas_editor.cpp`) implementing `WorldForgeEditorSession` and `draw_world_forge_viewport(...)`.
- `EditorState::ViewportTab` gained a 5th value `WorldForge`; added `EditorState::world_forge_editor` session member and `world_forge_viewport_active()` helper (`src/rendering/render_app.cpp`).
- Added a **World Forge** Viewports tab item (`ICON_FA_GLOBE " World Forge##ViewportWorldForge"`), disabled during an active play-test session exactly like **Sculpt**/**UI**. When active, the center content branch calls `draw_world_forge_viewport(state.world_forge_editor, state.project_root)` instead of the 3D scene/game image.
- New icon macro `ICON_FA_GLOBE` in `include/engine/editor/editor_icons.h` (Font Awesome codepoint `0xf0ac`); its codepoint was also added to the merged-font `icon_ranges` allowlist in `src/ui/game_fonts.cpp` (otherwise it renders blank — see the new finding below).
- On first draw (`!session.loaded`), the tab lazily calls `session.reload(project_root)`, which loads `factions.worldforge.json` / `relationships.worldforge.json` / `map.worldforge.json` from their default project paths via `apply_world_forge_operation` (action=get), matching the MCP read path (DEC-0003). Missing files fall back to empty in-memory assets rather than erroring.
- Sub-tab bar switches `WorldForgeEditorSession::pane` between Factions / Relationships / Map, resetting the active list and selection on change. (Field editing and Save landed together with TICKET-0016 — see that ticket's "What changed" for the detail/save behavior implemented in the same files.)
- Docs: `context/features/world-forge-scope.md` status line now notes the editor tab MVP is shipped (0017 canvas still pending); `context/features/editor-mvp.md` tab list now mentions **UI**/**World Forge** and gained a "World Forge editor UI" subsection; `context/testing/coverage.md` gained a "World Forge editor UI" row (manual smoke only, no automated GUI test).
- Recorded a new finding in `context/testing/findings.md` about `ICON_FA_*` macros needing a matching `icon_ranges` entry in `game_fonts.cpp`, and flagged that the pre-existing `ICON_FA_MOUNTAIN`/`ICON_FA_DESKTOP` icons are still missing theirs (left unfixed as out of scope for this change).

### Verification evidence

- `MSBuild ... /t:engine_core:Rebuild` succeeded with 0 new warnings/errors (only pre-existing `getenv`/`sscanf`/fastgltf warnings remain).
- `MSBuild ... /t:engine` failed to **link** only: `engine.exe` was locked by an already-running editor process on this machine (`LNK1168`). The `engine_core` library — which contains all of this ticket's and TICKET-0016's new/changed code — built and linked successfully. Manual in-app tab smoke was not re-verified against the rebuilt binary because the running `engine.exe` predates these changes; re-run once the process is closed and the exe can relink.

### Leftover risk

- No automated GUI/headless test exercises the new tab; coverage is manual-only for now.
- `ICON_FA_MOUNTAIN` (Sculpt tab) and `ICON_FA_DESKTOP` (UI tab) icons still render blank due to the pre-existing `icon_ranges` gap (unrelated to this ticket; documented, not fixed).

## Agent notes

2026-07-15: Picked up with 0016 for combined MVP; implemented together in `world_forge_editor.h/.cpp` and wired into `render_app.cpp`.
