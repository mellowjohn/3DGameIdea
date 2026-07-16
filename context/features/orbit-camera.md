# Orbit Camera

Third-person orbit camera with collision-aware distance shortening.

## Behavior

- Orbits a world pivot (character feet plus configurable shoulder height) using yaw/pitch mouse look.
- Default distance **5 m** (min **1.5 m**, max **8 m**), pivot height **1.6 m**.
- Each update sweeps a **0.2 m** probe sphere from the pivot toward the desired eye position against static and dynamic collision. The nearest hit shortens the resolved distance with **0.15 m** padding.
- `collision_shortened()` reports when geometry pulled the camera closer than the desired distance.

## Debug world integration

`engine run --debug-world` pairs the capsule character controller with `OrbitCamera`, loading character and camera assets from `play.session.json` when present.

- **WASD** moves the character relative to camera yaw.
- **Right-drag** mouse look orbits the camera around the character.
- Terrain streaming follows the resolved camera position.

The editor viewport uses the free `DebugCamera` for placement work. Editor test sessions use orbit settings from the project's camera asset.

## Editor play session

Configure character and camera assets in the Inspector (no entity selected) or inspect `.character.json` / `.camera.json` files from the Asset Browser. Save asset JSON or `play.session.json` to persist bindings.

## API

`OrbitCamera::update(pivot, collision_world)` must run after the pivot moves and before reading `view_matrix()` / `view_projection()`.

## Limitations

- No scroll-wheel distance control yet.
- Camera and character asset edits apply on the next test session start, not mid-session.
- Camera probe does not ignore the followed character inner body (open space and wall tests use distant blockers).
