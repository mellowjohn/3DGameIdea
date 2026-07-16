# Editor MVP

Launch with `engine editor --project <project>`.

The Dear ImGui editor runs on the production SDL3 and Direct3D 12 lifecycle. It provides a dockable offscreen-rendered viewport, scene hierarchy, transform/placement inspector, asset browser, diagnostics console, File/Save, and Edit/Undo/Redo. Placing the sample prefab, moving placed objects, and removing them use the same `CommandHistory` operations tested by automation and report changed UUIDs.

Placed objects without imported meshes render as colored box proxies. They can be selected in the hierarchy or by clicking their projected viewport bounds. Move, rotate, and scale modes use the MIT-licensed ImGuizmo library. A drag maintains a preview matrix and commits one undoable placement command when released.

Prefab assets can be placed from the Asset Browser with the `Place` button, by double-clicking the prefab row, or by dragging the prefab into the viewport. Dropping onto terrain raycasts the cursor position and shows a live placement preview before commit.

The Asset Browser shows one folder at a time under `assets/`. Click a subfolder row to select it, double-click to open it, or use the breadcrumb and **Up** controls to navigate. Folder rows use editor-drawn folder icons and accept dragged asset files to move them into that folder on disk. Prefab rows show a small editor-drawn thumbnail derived from the prefab's loaded primitive parts and colors, with an imported-mesh fallback icon. Visible rows show short names; full project-relative paths remain available in hover tooltips. **+ New Folder** creates a subfolder in the current directory and is also available as the first row inside the asset list. Select a folder row and use **Rename Folder** to rename it on disk; open scene placements and in-memory editor references under moved or renamed paths are remapped automatically.

Compositional v2 prefabs (primitive parts) render as multi-part mesh instances in the viewport. Use **Edit** beside a prefab in the Asset Browser to open the Prefab Editor panel: select a part, adjust local transform fields, add/remove/edit collider and script-binding components (including **Open Script**), and **Save Prefab** to write JSON (propagates to non-overridden scene instances). While editing, a preview of the prefab floats at world `(0, 3, 0)` above the terrain.

Selected placements show **Add Collider** / **Add Script Binding** in the Inspector. Expand a component to edit properties (collider shape, half-extent/radius, offset, trigger; scriptBinding kind and binding id) through `CommandHistory` / `set-entity-component`, which marks instance overrides per DEC-0017. **Open Script** resolves the binding via `assets/scripts/bindings.script.json` and opens the Lua file with the OS default association. The Prefab Editor exposes the same property fields and Open Script control before **Save Prefab**.

Selected entity authored colliders (and Prefab Editor preview colliders at `(0, 3, 0)`) draw as green wireframes in the Scene viewport — box as an AABB outline, sphere as a projected circle — using the same placement × local offset path as physics. This is independent of **Show collision debug**, which still dumps all physics bodies.

Existing placements whose prefabs already define `collision[]` (or script bindings) are **seeded into entity Components** on editor load and on select if missing, so they appear in the Inspector without re-adding them.

Selected placed objects can be dragged across terrain by left-clicking the object and dragging; release commits one undoable move. Clicking empty terrain sets the placement cursor used by browser placement actions. Viewport markers show the placement cursor and active drop preview.

Selected objects draw a gold wireframe bounding box with a name label. Hovered objects draw a cyan wireframe box. Drag-and-drop previews use the same cyan overlay style.

The panel layout is locked to prevent accidental rearrangement and recalculates proportionally from the current window dimensions. The hierarchy and asset browser occupy the left column, the viewport and diagnostics occupy the center, and the inspector occupies the right column.

Camera look begins only when the right mouse button is pressed over the viewport image and ends on release. Keyboard camera movement is likewise scoped to active viewport camera capture. Left-clicking an object proxy selects it on release unless the click becomes a terrain drag.

Gizmo shortcuts avoid the WASD camera controls: `1` selects move, `2` selects rotate, and `3` selects scale. Shortcuts are disabled during right-drag camera capture and text input.

Editor shortcuts: `Ctrl+S` save, `Ctrl+Z` undo, `Ctrl+Y` redo, `Ctrl+D` duplicate selected placement, `Delete` remove selected placement, `Escape` clear selection.

Test sessions (`Play` menu or viewport toolbar): **Start Test** (`F5`) uses a placed **player spawn** in the scene when one exists; otherwise it falls back to `play.session.json` defaults. Select the placed player prefab to edit movement settings in the Inspector (**Apply to Placement** stores them on that entity). Camera settings remain project defaults via `play.session.json` or Asset Browser **Inspect** on `.camera.json`. **Pause** (`F6`) freezes physics and player movement while keeping the session active. **Resume** (`F6`) continues the session. **End Test** (`Shift+F5` or `Escape`) removes the capsule, restores the saved edit camera, resets the player spawn transform to its pre-test position, and returns to placement editing.

During an active test session, gizmos, terrain dragging, viewport picking, and prefab drop placement are disabled on the **Scene** tab. Use the **Game** tab for player controls (**WASD** + right-mouse orbit). The **Scene** tab keeps the edit camera so you can fly around while playtesting.

The center panel exposes **Scene**, **Sculpt**, **Game**, **UI**, and **World Forge** tabs (Unity-style). **Scene** uses the edit camera with gizmos and placement tools. **Sculpt** uses the edit camera with a terrain height brush, brush toolbar (radius, strength, undo/redo), and a viewport brush preview. **Game** shows the player camera during a test session. **UI** hosts the UI Canvas editor (see below). **World Forge** hosts the World Forge editor (see [World Forge editor UI](#world-forge-editor-ui)). Toolbar and tab labels use merged [Font Awesome Free](https://fontawesome.com/license/free) solid glyphs from `assets/editor/fonts/fa-solid-900.ttf`.

### World Forge editor UI

TICKET-0015/TICKET-0016 added a **World Forge** Viewports tab (disabled during play test, like **Sculpt**/**UI**) with **Factions**, **Relationships**, **Map**, **Quests**, and **Dialogues** sub-tabs. Each sub-tab shows a left-hand selectable list (Relationships: **Nodes**/**Edges**/**Graph**; Map: **Regions**/**POIs**/**Links**; Dialogues: **Trees**/**Graph**) and a right-hand detail panel. **Quick-create** TreeNodes (display name → slug id) exist for factions, relationship nodes/edges, regions/POIs/links, quests (+ objectives/forks), and dialogue trees (new tree gets entry node `start`; Twine import remains available). Edits stay in memory on the session assets and mark dirty. **Reload** and **Save** call `engine::apply_world_forge_operation` — the same command MCP agents use — so the editor and MCP never diverge on validation or write paths (DEC-0003). Save applies World Forge assets so cross-references stay consistent; validation failures surface in the toolbar status text and leave the session dirty for another attempt.

The visible editor stores personal docking state under `out/editor/imgui.ini`; hidden tests disable persistence for deterministic captures. World save preserves known world identity and partition metadata and uses the existing atomic-save/backup path.

## Current limitations

- Picking raycasts per-part imported/primitive mesh AABBs and falls back to projected proxy bounds when mesh data is unavailable. Full triangle mesh picking remains deferred.
- Imported mesh thumbnails, viewport gizmos for prefab part editing, and play-state save/resume remain pending.
- Material assets (`.material.json`) support Asset Browser **+ New Material**, **Inspect**, Inspector editing (base color, roughness, metallic, physics), and save. Prefab Editor parts can assign a material or use a direct part color. The **Sculpt** tab picks the active terrain material with a color preview and **Edit Material** shortcut.
- Terrain sculpting lives on the **Sculpt** viewport tab with height raise/lower, **Flatten**, brush radius/strength, undo/redo, and save through **Ctrl+S** to `assets/terrain/terrain-edits.json`.
- Live MCP agents can apply the same height/paint/foliage strokes via `engine_terrain_apply` when **MCP connection** is enabled ([DEC-0018](../decisions/index.md#dec-0018-mcp-terrain-sculpt-and-paint-apply)).
- Renaming asset folders does not rewrite cross-references inside other asset JSON files on disk; save those assets manually after moving content.
- Test sessions render the active character asset during play; changes apply on the next session start. Authored character animation and save/resume of play state remain future work.
- Proxy rendering applies position, quaternion rotation, and scale through the same model-matrix convention used by future mesh instances.
