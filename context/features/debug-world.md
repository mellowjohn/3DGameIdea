# 3D Debug World

The debug world renders deterministic 33×33 smooth low-poly heightfield tiles across a camera-centered neighborhood of 40 m terrain cells (stream radius 2). Runtime streaming uses a **view-biased** wanted set (full collision support disc of radius 1, outer ring toward camera forward). Support loads **before** unload; walk fringe support is **amortized** (default ≤1 cell/frame) while prior cells are held until the new disc is complete. Normal walk-fringe and outer-ring mesh generation run on a worker with immutable terrain/paint/material snapshots; Jolt collision bodies commit on the main thread only after generation finishes. Bootstrap/teleport still fills support immediately, including waiting for any already-queued support task, so the player never loses a floor. Outer ring amortizes at ≤2 cells/frame after support is ready. GPU terrain is uploaded **per cell** with fence-deferred buffer retirement. The same height samples create the D3D12 triangle meshes and matching Jolt heightfield collision surfaces.

Triangle colors come from stylized material regions—grass, dirt, rock, snow, and corrupted ground—using a muted dark-fantasy palette without texture assets. A cool overcast directional light, layered distance fog, and a procedural fullscreen sky gradient establish readable daylight atmosphere. Placed `campfire.prefab.json` instances emit a warm point light for local lighting tests.

As the free camera moves, adjacent cells load and distant cells unload while preserving seamless shared borders between generated meshes. A unit test walks a coarse grid across the accepted 4×4 km (16 km²) world extent and verifies resident terrain cells stay bounded. Amortized + view-biased stream behavior is covered in the `terrain` suite.

Launch with `engine run --project <project> --debug-world` or through the editor, which enables the same terrain field.

Launch with `engine run --project <project> --debug-world` or through the editor, which enables the same terrain field.

Controls:

- W/S: move forward/backward
- A/D: strafe
- Space/Left Ctrl: move up/down
- Left Shift: speed boost
- Hold right mouse and move: camera look

The current scene also renders a colored physics cube, editor placement proxies, and imported mesh instances. Expanded static prefab instances and their point lights are cached and drawn by pointer (no per-frame deep copy); the cache rebuilds after scene/prefab/mesh edits or live gizmo preview, not merely because the scene is unsaved. Active player, drag/drop, and prefab-edit previews remain dynamic. Player skinned visuals use **GPU LBS** (bind-pose VB + CPU skin matrices into a ringed bone CB). Hot per-frame upload CBs (frame/water/shadow/SSAO/composite/bones) are a **2-slot ring** keyed to the swapchain index so Present no longer drains the GPU fence ([DEC-0047](../decisions/index.md#dec-0047-frame-upload-ring-and-gpu-lbs-skinning)). The editor renders only the active Scene or Game viewport; Game play-tests do not pay for an unseen Scene pass. The Diagnostics panel has a Performance tab with raw wall time, estimated CPU work, Present and GPU-fence wait, GPU frame time, process CPU/RAM, adapter-local GPU-memory budget, draws, instances, terrain cells, plus Render prep sub-rows for skin matrices and cache rebuild. The editor viewport status line shows loaded terrain cell count and focus coordinates. Grid lines, ray markers, and on-screen text remain planned enhancements.
