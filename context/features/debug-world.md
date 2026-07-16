# 3D Debug World

The debug world renders deterministic 33×33 smooth low-poly heightfield tiles across a camera-centered 5×5 neighborhood of 40 m terrain cells (stream radius 2). The same height samples create the D3D12 triangle meshes and matching Jolt heightfield collision surfaces.

Triangle colors come from stylized material regions—grass, dirt, rock, snow, and corrupted ground—using a muted dark-fantasy palette without texture assets. A cool overcast directional light, layered distance fog, and a procedural fullscreen sky gradient establish readable daylight atmosphere. Placed `campfire.prefab.json` instances emit a warm point light for local lighting tests.

As the free camera moves, adjacent cells load and distant cells unload while preserving seamless shared borders between generated meshes. A unit test walks a coarse grid across the accepted 4×4 km (16 km²) world extent and verifies resident terrain cells stay bounded.

Launch with `engine run --project <project> --debug-world` or through the editor, which enables the same terrain field.

Controls:

- W/S: move forward/backward
- A/D: strafe
- Space/Left Ctrl: move up/down
- Left Shift: speed boost
- Hold right mouse and move: camera look

The current scene also renders a colored physics cube, editor placement proxies, and imported mesh instances. The editor viewport status line shows loaded terrain cell count and focus coordinates. Grid lines, ray markers, and on-screen text remain planned enhancements.
