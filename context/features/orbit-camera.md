# Orbit Camera

Third-person orbit camera with collision-aware distance shortening (play-test **Game** camera).

## Behavior

- Orbits a shoulder/chest pivot (`pivotHeight` above feet) with yaw/pitch mouse look.
- **Orientation** (yaw/pitch) is the world look / aim axis. View uses that direction (`LookTo`), not `LookAt` the character.
- **Position** (distance + optional **shoulder offset**) frames the eye — over-the-shoulder keeps the body left of center without aiming at the head.
- Default framing (open-world `game.camera.json`): **~5.25 m** distance, max zoom-out **6 m**, min **1.5 m**, mild look-down pitch, **~65°** vertical FOV. Engine `OrbitCameraConfig` / `CameraAsset` struct defaults remain looser for other samples — play uses the project camera asset.
- Pitch is soft-clamped (`minPitch` / `maxPitch`) so the view stays game-like.
- **Scroll wheel** dollies desired distance between min/max.
- Collision shortening uses **StaticWorld only** so the player body does not pin the camera at min distance.
- Collision distance and shoulder scale **smooth** with frame dt: fast pull-in when blocked, slow recover when clear. Pivot soft-follows the gameplay feet so rigidbody micro-jitter does not shake the lens.
- `apply_look` refreshes the eye immediately from the current resolved distance so yaw/pitch are not delayed until the next collision update.
- `forward()` returns pure yaw/pitch orientation (matches bow aim / reticle / projectiles). Scene edit free-cam (`DebugCamera`) is separate and unchanged.

## Debug / play

`engine run --debug-world` and editor **Game** tab test sessions use `OrbitCamera` with character + camera assets from `play.session.json`.

- **WASD** moves relative to camera yaw.
- **Right-drag** orbits.
- **Scroll** zooms.
- **Bow draw (play-test):** LMB on a ranged hotbar weapon blends shoulder offset higher and desired distance closer (hip rest → ~**4.35 m**, ~0.15 s) for over-the-shoulder framing — see [`bow-draw-aim-release.md`](bow-draw-aim-release.md). Scene free-cam (`DebugCamera`) is unchanged and still uses freecam zoom ranges.

## Configuration

See [`../formats/camera-assets.md`](../formats/camera-assets.md). Sample: `samples/open-world-rpg/assets/cameras/game.camera.json`.

`OrbitCamera::update(pivot, collision_world, delta_seconds)` must run after the pivot moves and before reading view matrices.
