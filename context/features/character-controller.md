# Character Controller

Jolt `CharacterVirtual` capsule controller for third-person traversal over streamed terrain and static collision.

## Capsule and movement

- Default capsule: **0.35 m** radius, **0.85 m** cylindrical half-height (feet at the controller origin).
- `CharacterController::move(wish_velocity, yaw_radians, seconds)` applies camera-relative horizontal input, gravity, stick-to-floor, and stair stepping through Jolt `ExtendedUpdate`.
- Default max walk slope ratio **0.45** matches the navigation grid walkability threshold (`atan(0.45)` radians internally).
- Default step height **0.35 m**, max horizontal speed **6 m/s**, gravity **9.81 m/s²**, jump velocity **5 m/s**.
- `CharacterController::jump()` requests a grounded vertical impulse applied on the next `move()` call. Airborne jumps are ignored.

## Partition ownership

`owner_cell(partition)` maps the controller feet position through `WorldPartition::cell_for`. The controller is not owned by streamed collision cells; terrain and placement bodies unload independently while the capsule continues querying resident narrow-phase geometry.

## Debug integration

- **Debug world** (`engine run --debug-world`): spawns a controller above origin terrain; **WASD** walks relative to camera yaw; **Space** jumps when the character controller is active; **right-drag** mouse look orbits the collision-aware follow camera.
- **Editor play test** (Game tab): **WASD** moves the player; **Space** jumps. Space remains free-camera vertical movement outside the Game tab.
- **Editor**: collision debug overlay draws the capsule wireframe when a controller instance is supplied and **Show collision debug** is enabled.

## API

`CharacterController::create(collision_world, spawn, config)` requires an initialized `CollisionWorld`. The controller uses the world's physics system directly and does not consume a dynamic body slot in `body_count()`.

`jump()` returns `true` when a grounded jump is queued for the next `move()`; `false` when airborne or not ready.

`debug_body()` returns a `CollisionDebugShape::Capsule` record for visualization.

## Limitations

- No root motion, animation sync, or nav-grid snap yet.
- No coyote time, double jump, or air control tuning yet.
- Character-vs-character collision is not registered.
- Navigation grid queries are not yet used for pathing or snap-to-walkable.
