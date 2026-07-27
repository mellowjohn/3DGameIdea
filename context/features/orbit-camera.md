# Orbit Camera

Third-person RPG orbit camera with collision-aware distance shortening (WoW / Dragon Age–style framing).

## Behavior

- Orbits a shoulder/chest pivot (`pivotHeight` above feet) with yaw/pitch mouse look.
- Optional **shoulder offset** places the eye slightly to camera-right while still aiming at the character (over-the-shoulder).
- Default framing: **~10.5 m** distance, mild look-down pitch, **~65°** vertical FOV.
- Pitch is soft-clamped (`minPitch` / `maxPitch`) so the view stays game-like.
- **Scroll wheel** dollies desired distance between min/max (camera moves closer/farther; look stays on the player).
- Collision shortening uses **StaticWorld only** so the player body does not pin the camera at min distance.
- Collision distance and shoulder scale **smooth** with frame dt: fast pull-in when blocked, slow recover when clear (reduces look-around pop as the probe grazes props/terrain). Pivot soft-follows the gameplay feet so rigidbody micro-jitter does not shake the lens.
- `apply_look` refreshes the eye immediately from the current resolved distance so yaw/pitch are not delayed until the next collision update.

## Debug / play

`engine run --debug-world` and editor **Game** tab test sessions use `OrbitCamera` with character + camera assets from `play.session.json`.

- **WASD** moves relative to camera yaw.
- **Right-drag** orbits.
- **Scroll** zooms.

## Configuration

See [`../formats/camera-assets.md`](../formats/camera-assets.md). Sample: `samples/open-world-rpg/assets/cameras/game.camera.json`.

`OrbitCamera::update(pivot, collision_world, delta_seconds)` must run after the pivot moves and before reading view matrices.
