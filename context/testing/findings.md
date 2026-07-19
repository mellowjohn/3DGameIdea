# Engineering Findings

Record material defects or constraints that can prevent recurrence. Newest entries go first.

## 2026-07-19 — Water refraction read the live lit RT (shoreline fuzz / highlight feedback)

- Reproduction: Game/editor view with authored water; look at shorelines and bright specular areas (sun).
- Impact: Grainy shimmering edges, ghosting around trunks, and blocky yellow/white blotches on the water surface.
- Cause: `draw_water_pass` wrote into `lit_color_` while the water pixel shader sampled that same resource as `sceneColor` for refraction (read-while-write / RT+SRV hazard). Wave UV offsets made the feedback move every frame.
- Resolution: Copy `lit_color_` into `water_scene_color_` before the water draw; bind that stable copy as `t0`. Remove unused depth SRV sampling so depth can stay `DEPTH_WRITE` for the water DSV test.
- Verification: Rebuild `engine`, relaunch editor; water edges and specular areas should no longer feedback-smear.
- Remaining risk: Soft depth-based shoreline foam still needs a separate depth copy (or reconstructed depth) before sampling depth in the water PS.

## 2026-07-16 — Dialogue New Node forced a full graph relayout

- Reproduction: Dialogues → Graph, open a laid-out tree, click **New Node** (or Duplicate).
- Impact: All nodes jumped into a single vertical BFS column stack (orphans share one depth), wiping any dragged layout — looked “squished into one line.”
- Cause: `dialogue_graph_needs_layout` always called `layout_dialogue_graph(..., preserve_existing=false)`, clearing positions. New unlinked nodes all land at `max_depth+1`, so they stack in one column.
- Resolution: Rename to `dialogue_graph_full_relayout` (tree switch / import / Auto Layout only). New/Duplicate place the node beside the selection via incremental layout; Delete/Undo no longer full-relayout.
- Verification: Rebuild `engine`; New Node keeps existing layout and offsets the new card to the right of the selection.
- Remaining risk: Auto Layout still packs orphans into one column by design; use links or Auto Layout intentionally.

## 2026-07-15 — New toolbar icon macros silently render blank without a matching font glyph range

- Reproduction: add an `ICON_FA_*` macro to `include/engine/editor/editor_icons.h` and use it in a tab/button label without also adding its codepoint to `icon_ranges` in `src/ui/game_fonts.cpp`.
- Impact: the merged Font Awesome font only rasterizes the explicit codepoint pairs listed in `icon_ranges`; any icon macro whose codepoint is missing from that allowlist renders as a blank/tofu glyph instead of the intended icon, with no build or runtime error.
- Cause: `ICON_FA_MOUNTAIN` (`0xf6fc`, Sculpt tab) and `ICON_FA_DESKTOP` (`0xf108`, UI tab) were added to `editor_icons.h` after the `icon_ranges` allowlist was last updated, and neither codepoint was added to the range list.
- Resolution: when adding a new `ICON_FA_*` macro for editor UI (e.g. `ICON_FA_GLOBE` for the World Forge tab, TICKET-0015), also add its codepoint as a `{cp, cp}` pair to `icon_ranges` in `game_fonts.cpp`.
- Verification: `ICON_FA_GLOBE` (`0xf0ac`) renders correctly on the World Forge tab after adding it to `icon_ranges`; `ICON_FA_MOUNTAIN`/`ICON_FA_DESKTOP` were left as pre-existing, out-of-scope gaps for this change.
- Remaining risk: `ICON_FA_MOUNTAIN` (Sculpt tab) and `ICON_FA_DESKTOP` (UI tab) still render blank until their codepoints are added to `icon_ranges` in a follow-up fix.

## 2026-07-03 — MCP-created prefab primitives did not render until editor restart

- Reproduction: create a new compositional prefab through `engine_prefab_apply` while the editor is running, place instances, keep the session open without restarting.
- Impact: placed props used catalog/bounds data but drew nothing; older prefabs from launch (tree, campfire) still rendered.
- Cause: `prefab_meshes_dirty` regenerated CPU primitive meshes in `imported_meshes` but never re-uploaded them into the renderer `mesh_ranges_` map built only at `initialize()`.
- Resolution: `Renderer::sync_imported_meshes()` rebuilds the prop vertex buffer when the catalog changes; render loop calls it after `ensure_prefab_primitive_meshes()`.
- Verification: create `bush.prefab.json` via MCP, place bushes live, confirm green shrub meshes appear without restarting the editor.
- Remaining risk: very large mesh uploads may hitch one frame while the GPU buffer is rebuilt.

## 2026-07-03 — MCP could not reach editor: relative vs absolute project path pipe mismatch

- Reproduction: launch editor with `--project samples/open-world-rpg`, enable MCP connection; Cursor MCP uses absolute `--project` from `.cursor/mcp.json`.
- Impact: `engine_editor_status` reported disconnected while the editor had MCP enabled; live `engine_scene_apply` could not reach the bridge.
- Cause: named-pipe identity hashed `lexically_normal()` path strings only, so `samples/open-world-rpg` and `C:/.../samples/open-world-rpg` produced different pipe names.
- Resolution: normalize project roots with `absolute` + `weakly_canonical` before hashing in `editor_bridge.cpp`.
- Verification: toggle MCP connection off/on after rebuild (or restart editor); `engine_editor_status` should report `editorRunning: true`.
- Remaining risk: editor and MCP must point at the same project directory on disk, not two different copies.

## 2026-07-03 — Cursor MCP timeout: NDJSON stdio vs Content-Length framing

- Reproduction: enable `ai-rpg-engine` in Cursor; MCP logs show `createClient` then `MCP IPC timeout` after 30s; `mcp-trace.jsonl` shows `frame_skip` with raw `{"method":"initialize"...` lines at the same timestamps.
- Impact: Cursor stayed yellow; `createClient` timed out; no tools loaded.
- Cause: Cursor sends newline-delimited JSON on stdio, not `Content-Length` framed messages. The server skipped those lines as unknown framing.
- Resolution: accept line-delimited JSON when a line starts with `{`; mirror the same format on stdout for responses when that mode is detected. Keep Content-Length support for scripted tests.
- Verification: reload MCP in Cursor; trace should show `frame_ndjson`, `request`/`initialize`, and `tools/list` within seconds.
- Remaining risk: very large single-line JSON payloads are uncommon for MCP but would need framed reads if introduced.

## 2026-07-03 — Cursor MCP yellow: case-sensitive Content-Length header

- Reproduction: enable `ai-rpg-engine` in Cursor MCP; `mcp-trace.jsonl` shows repeated `server_start` / `server_stop` with no `request` events.
- Impact: Cursor stayed yellow; tools never loaded; editor bridge saw no `client_connected` because MCP never reached tool calls.
- Cause: `read_message()` only accepted `Content-Length:` (exact case). Cursor sends lowercase `content-length:` per HTTP-style headers.
- Resolution: case-insensitive `content-length` parsing in `mcp_server.cpp`; log skipped framing lines as `frame_skip` in the trace.
- Verification: reload MCP in Cursor; `mcp-trace.jsonl` should show `request` for `initialize` and `tools/list` within seconds.
- Remaining risk: none known for header casing; other Cursor UI yellow states may still appear briefly during reconnect.

## 2026-07-03 — Cursor MCP hung on “loading tools” before the editor starts

- Reproduction: enable `ai-rpg-engine` in Cursor MCP without launching the editor.
- Impact: Cursor stayed on step 2 (“loading tools”) for a long time or showed a yellow/warning state.
- Cause: Windows text-mode stdio could break MCP `Content-Length` framing; Cursor also needs reliable process launch paths and handlers for `ping` / empty `resources/list` / `prompts/list` probes during connect.
- Resolution: binary stdin/stdout in `run_mcp_server`, robust framed reads/writes, extra MCP handlers, and `tools/mcp-server.cmd` wired from `.cursor/mcp.json`.
- Verification: reload MCP in Cursor; tools should appear within a few seconds even when the editor is not running.
- Remaining risk: first `engine.exe` cold start can still take a couple seconds while Windows loads linked DLLs.

## 2026-07-03 — Live bridge pipe I/O blocked the editor render thread

- Reproduction: enable MCP connection in the editor while Cursor MCP (`ai-rpg-engine`) is active.
- Impact: Cursor MCP showed yellow/warning; the editor became not responding and sometimes crashed.
- Cause: accept/read/write on the named pipe ran on the render thread and could deadlock with the MCP client waiting for a response.
- Resolution: move all pipe accept/read/write to a dedicated bridge worker thread; the render thread only dispatches one non-blocking `poll_pending()` handler call per frame.
- Verification: enable MCP in Diagnostics with Cursor connected; editor UI should stay responsive and `engine_editor_status` should return within about one second.
- Remaining risk: very slow scene commands still stall the UI while they execute on the render thread.

## 2026-07-03 — MCP status probes blocked for five seconds without live automation

- Reproduction: open Cursor MCP while the editor is running but **Enable MCP connection** is off.
- Impact: Cursor showed tools loading for several seconds; `engine_editor_status` appeared hung.
- Cause: `forward_to_editor()` called `WaitNamedPipeW` with a 5000 ms timeout even when no bridge was listening.
- Resolution: probe with `is_editor_running()` first (50 ms cap), return unavailable immediately, and lower live request timeouts.
- Verification: reload MCP in Cursor without enabling the editor bridge; tools should list quickly and status should report disconnected.
- Remaining risk: the first `engine.exe mcp` cold start can still take a second while Windows loads the binary.

## 2026-07-03 — Disabling live automation could freeze the editor

- Reproduction: enable MCP connection in Diagnostics, let Cursor connect, then uncheck the box.
- Impact: the editor became not responding and sometimes crashed.
- Cause: pipe teardown closed the listening handle while a client could still be connected; response `WriteFile` could block the render thread.
- Resolution: poll before teardown each frame, call `DisconnectNamedPipe` before `CloseHandle`, and use timed overlapped writes with shorter read/write budgets.
- Verification: toggle MCP connection on/off repeatedly while Cursor MCP is enabled.
- Remaining risk: heavy concurrent MCP traffic while disabling may still drop one in-flight request.

## 2026-07-03 — Live automation bridge is opt-in inside the editor

- Reproduction: launch `engine editor` with Cursor MCP configured; the window stayed black or not responding while the bridge listened on every frame.
- Impact: normal editing was blocked by pipe probes and render-thread bridge I/O even when MCP was not intended.
- Cause: the named pipe started automatically at editor launch; Cursor MCP and probe clients could connect before the user was ready.
- Resolution: add **Enable MCP connection** in the Diagnostics panel (off by default); start/stop the bridge only while enabled; remove blocking `FlushFileBuffers` on disconnect; cap bridge reads with `PeekNamedPipe` timeouts.
- Verification: rebuild `engine`, launch editor without enabling MCP (UI should respond), then enable MCP and validate tools from Cursor.
- Remaining risk: a misbehaving client can still stall the render thread briefly while live automation is enabled.

## 2026-07-03 — Per-frame full asset scan froze the editor

- Reproduction: launch `engine editor` after the MCP/Lua slice landed.
- Impact: Windows reported the editor as not responding; the viewport stayed black or updated only intermittently.
- Cause: `reload_changed_lua_scripts()` called `AssetMonitor::poll()` every frame, which recursively scanned and content-hashed every file under `assets/`.
- Resolution: replace script hot reload with a lightweight `ScriptFileMonitor` that watches only `assets/scripts/*.lua` write times every 30 frames; throttle MCP bridge polling and remove blocking sleeps on the render thread.
- Verification: rebuild `engine`, relaunch the editor, and confirm the UI renders while MCP remains available.
- Remaining risk: very large script trees still add periodic directory walk cost, but it is no longer proportional to total project asset size.

## 2026-07-03 — MCP editor-status probes froze the live editor bridge

- Reproduction: launch `engine editor` while the Cursor MCP server is enabled (`.cursor/mcp.json`) or while `is_editor_running()` probes the named pipe.
- Impact: the editor window stayed black or appeared frozen because the render thread blocked inside the bridge on every pipe connection.
- Cause: `is_editor_running()` opened and immediately closed the pipe without sending a framed request, and `poll_pending()` blocked up to 2 seconds waiting for `Content-Length` data on the render thread.
- Resolution: `is_editor_running()` now uses `WaitNamedPipeW` only; `poll_pending()` rejects empty probe connections quickly and caps request reads at 100 ms.
- Verification: `automation` suite passes; rebuild `engine` and relaunch the editor.
- Remaining risk: a misbehaving external client that connects without sending a request can still stall the editor for ~20 ms per connection.

## 2026-07-02 — Parallel MSVC rebuild collided on the program database

- Reproduction: trigger a full multi-source rebuild after changing the vcpkg dependency graph.
- Impact: `cl.exe` intermittently failed with C1041 while multiple compiler processes wrote `engine_core.pdb`.
- Cause: the Visual Studio 2019 toolchain did not serialize shared PDB writes for the target.
- Resolution: add MSVC `/FS` to the engine target.
- Verification: the subsequent full fastgltf rebuild completed successfully.
- Remaining risk: none known for the current generator; compile throughput may be slightly lower during PDB writes.

## 2026-07-02 — Gizmo rotation committed but proxy rendering ignored it

- Reproduction: rotate the selected placement and release the gizmo.
- Impact: scene data changed, but the box proxy appeared unchanged, making rotation look nonfunctional.
- Cause: placement proxy model matrices applied translation and scale only; gizmo initialization also assumed identity rotation.
- Resolution: proxy and gizmo matrices now compose scale, the stored quaternion, and translation using DirectXMath.
- Verification: rotation survives scene commands and feeds the rendered proxy model matrix; editor GPU smoke and world command tests pass.
- Remaining risk: proxy boxes are only placeholders and cannot demonstrate the orientation of visually symmetric assets as clearly as imported meshes.

## 2026-07-02 — Editor camera and gizmo interaction escaped viewport scope

- Reproduction: move the mouse outside the viewport after right-click interaction, or drag a gizmo while watching the placement proxy.
- Impact: camera rotation could continue outside the viewport and the gizmo moved without visibly moving the rendered proxy until commit.
- Cause: raw SDL relative motion was accumulated globally, and rendering read only committed scene transforms rather than the active gizmo preview.
- Resolution: camera capture starts only from a right press over the viewport image and ends on release; keyboard movement is gated with it. Active gizmo preview transforms now feed proxy rendering and commit once on release. Terrain clicks establish the next asset placement position.
- Verification: the responsive 900×600 capture preserves the locked panel geometry and visible proxy/gizmo; GPU editor smoke suites pass at two resolutions.
- Remaining risk: native pointer drag semantics still require manual testing until Windows UI automation is added.

## 2026-07-02 — ImGuizmo matrix convention obscured the editor viewport

- Reproduction: pass transposed DirectX camera matrices to `ImGuizmo::Manipulate` while drawing over the offscreen viewport image.
- Impact: the gizmo disappeared; an earlier inconsistent combination also produced a black viewport/panel capture.
- Cause: the renderer's DirectXMath row-major storage already matches the interpretation required by the integrated ImGuizmo build. Applying another transpose was incorrect.
- Resolution: pass the camera view/projection storage directly and keep gizmo model matrices in ImGuizmo's native layout.
- Verification: `out/captures/editor-gizmo.png` shows terrain, physics body, placement proxy, panels, and translation gizmo together; `editor_smoke` covers initialization and rendering.
- Remaining risk: interactive drag and click behavior still require user-input automation beyond the current hidden lifecycle smoke test.

## 2026-07-02 — Editor windows collapsed in the initial docking layout

- Reproduction: capture the first hidden editor frame after creating an unconstrained root dockspace.
- Impact: only the menu bar was visible; hierarchy, viewport, inspector, assets, and diagnostics appeared missing despite being submitted.
- Cause: ImGui persisted 32×35 first-use window sizes while the host dockspace occupied the viewport.
- Resolution: hidden runs disable layout persistence, visible runs store layout under generated output, and every core panel receives a deterministic usable first-run position and size.
- Verification: `out/captures/editor-mvp.png` shows all five panels; `editor_smoke` validates lifecycle behavior.
- Remaining risk: the central viewport is currently a transparent runtime overlay rather than a dedicated render texture.

## 2026-07-02 — Terrain source duplication removed

- Reproduction: the visual debug world rendered a hardcoded flat plane while physics independently used a box floor.
- Impact: future terrain could appear correct while collision remained flat or misaligned.
- Cause: the early renderer and collision walking skeletons had separate placeholder geometry.
- Resolution: one deterministic terrain generator now supplies both render triangles and Jolt heightfield samples. Adjacent cells sample world-space coordinates to share exact borders.
- Verification: terrain topology, deterministic output, border equality, heightfield ray casts, cell unloading, and the D3D12 debug-world smoke path are automated.
- Remaining risk: only one cell is rendered at runtime; streamed multi-cell ownership and LOD seams remain future work.

## 2026-07-02 — Diagnostics test deleted an actively open Windows log

- Reproduction: Remove the JSONL file before the process-owned logger closes its stream.
- Impact: The diagnostics suite fails even though the runtime event is flushed correctly.
- Cause: Windows prevents deletion of an open file handle.
- Resolution: Use a unique temporary log per test process and retain it as a diagnostic artifact until normal temporary cleanup.
- Verification: Named diagnostics suite completes and validates severity/priority JSON.
- Regression coverage: `diagnostics`.

## 2026-07-02 — Initial 3D model translation used the wrong matrix convention

- Reproduction: Upload translation in row-vector slots while HLSL evaluates `mul(matrix, vector)`.
- Impact: The first perspective capture flattened and misplaced cube geometry near the horizon.
- Cause: CPU model layout did not match the column-vector shader convention already used by the camera matrix.
- Resolution: Store translation in the final matrix column and visually recapture the scene.
- Verification: Depth-buffered ground and cube remain visible under perspective after correction.
- Regression coverage: `debug_world_smoke` plus captured-image review.

The same review found and removed a redundant CPU camera transpose: DirectXMath row-major data uploaded to HLSL column-major constants already produces the required column-vector interpretation.

## 2026-07-02 — Streaming suite used scheduler-dependent iteration waits

- Reproduction: Run the focused `streaming` suite when its async worker is not scheduled during 100 immediate polling iterations.
- Impact: The suite reports `cell committed` as failed even though the loader completes normally afterward.
- Cause: Iteration counts do not represent elapsed time and `yield` does not guarantee another thread runs.
- Resolution: Poll against a two-second steady-clock deadline with short sleeps; keep the production result-generation checks unchanged.
- Verification: Repeated focused streaming runs and the complete CTest suite pass.
- Regression coverage: Focused streaming and broad regression suites use bounded deadline waits.

## 2026-07-02 — MSVC 19.27 future requires default-constructible payload

- Reproduction: Instantiate `std::future<Result<CellData>>` where `Result` intentionally has no default state.
- Impact: Asynchronous streaming would not compile on the installed toolchain.
- Cause: The older standard-library future implementation default-constructs internal payload storage.
- Resolution: Transport an owned `Result<CellData>` pointer through the future instead of adding an invalid default result state.
- Verification: Streaming compilation and cancellation/validation tests pass.
- Remaining risk: Reevaluate this compatibility layer after upgrading MSVC.

## 2026-07-02 — Hot reload must validate before replacing the accepted snapshot

- Reproduction: Poll assets after introducing a missing or circular dependency.
- Impact: Applying the changed snapshot could leave runtime content partially updated and internally inconsistent.
- Cause: File changes are observable before the entire dependency graph is known to be valid.
- Resolution: Build and validate a candidate registry, emit no changes on failure, and replace the accepted snapshot only after validation succeeds.
- Verification: Monitor tests reject a cyclic snapshot, then recover and emit exactly one modified asset after correction.
- Regression coverage: Foundation asset-monitor tests.

## 2026-07-02 — EnTT registry ownership blocked scene result transport

- Reproduction: Return a `Scene` containing an inline EnTT registry through `Result<Scene>` on MSVC 19.27.
- Impact: Scene loading could not compile because the registry did not satisfy the required move path.
- Cause: The registry ownership model and older standard-library variant implementation made implicit scene transport fragile.
- Resolution: Give `Scene` unique ownership of a stable registry allocation, define explicit move operations, prohibit copies, and construct result variants in place.
- Verification: Deterministic scene round-trip and CLI world loading pass.
- Regression coverage: Foundation tests load, move, serialize, and compare scenes.

## 2026-07-02 — vcpkg release tag object is not a valid builtin baseline

- Reproduction: Use the annotated tag object hash for release `2026.01.16` as `builtin-baseline`.
- Impact: Manifest resolution is not guaranteed to address the intended versions commit.
- Cause: Annotated Git tags reference a tag object, which then references the commit required by vcpkg.
- Resolution: Pin underlying commit `66c0373dc7fca549e5803087b9487edfe3aca0a1`.
- Verification: Clean manifest configure resolved and built SDL 3.4.0.
- Regression coverage: CMake preset configuration invokes manifest resolution.

## 2026-07-02 — CLI JSON help initially contained raw newlines

- Reproduction: Request `engine help --json` before JSON escaping was centralized.
- Impact: Automation consumers could not parse valid JSON reliably.
- Cause: The initial string encoder escaped quotes and slashes but not control characters.
- Resolution: Escape newline, carriage return, tab, and other control characters.
- Verification: Foundation test checks escaped help output; PowerShell JSON parsing passes.
- Regression coverage: `foundation_tests.cpp`.

## 2026-07-02 — Current Windows SDK lacks modern DRED interfaces

- Reproduction: Inspect Windows SDK 10.0.18362.0 `d3d12.h` for current DRED interfaces.
- Impact: Device-removal diagnostics provide HRESULT causes but not modern breadcrumb/page-fault detail.
- Cause: Installed SDK predates those interfaces.
- Resolution: Preserve base device-removal reporting; schedule richer DRED data after a toolchain upgrade.
- Verification: Device error paths query `GetDeviceRemovedReason`.
- Remaining risk: Complex GPU hangs will be harder to diagnose until the SDK is upgraded.
