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

## Root motion (DEC-0030 / TICKET-0104)

When an animator controller has `applyRootMotion: true`, call `sync_character_root_motion(controller, animator, entityId, yaw, dt)` instead of wish-velocity `move` for locomotion. That helper ticks the animator and applies clip-space root deltas (rotated by yaw) via `CharacterController::move_root_motion`. Horizontal distance is not max-speed clamped; gravity/jump remain unless `rootMotionY` is enabled.

## Limitations

- Visual in-place root stripping for skinned meshes is not yet applied (capsule sync only).
- No nav-grid snap yet.
- No coyote time, double jump, or air control tuning yet.
- Character-vs-character collision is not registered.
- Navigation grid queries are not yet used for pathing or snap-to-walkable.
- **Swim mode** not implemented — planned under [DEC-0038](../decisions/index.md#dec-0038-water-swim-and-hydrology-authoring) / [`water-hydrology.md`](water-hydrology.md).

## Swim mode (planned — DEC-0038)

When the capsule enters authored water:

- Switch from walk locomotion to **swim** (surface and submerged variants TBD).
- **Shallow** water: wade or reduced swim cost (depth threshold TBD).
- **Deep** water: fatigue drain while swimming; at exhaustion, **health damage over time**.
- Jump/air rules TBD while partially submerged.
- Ships use scripted paths; player aboard uses vehicle state rather than free swim unless overboard.

Implementation tracks [`water-hydrology.md`](water-hydrology.md) and stamina/HUD when fatigue UI ships.
