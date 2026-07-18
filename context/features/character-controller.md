# Character Controller

**Default play path ([DEC-0038](../decisions/index.md#dec-0038-authored-rigidbody--dynamic-bodies-for-player-and-entities) / TICKET-0198):** the sample player prefab uses authored **Rigidbody** + **capsule Collider**. Play/test drives that dynamic Jolt body through `RigidbodyLocomotion` (wish velocity, ground friction, jump) — not `CharacterVirtual`.

`CharacterController` (Jolt `CharacterVirtual`) remains available for `--debug-world`, suites, and placements that lack a Rigidbody (fallback).

## Capsule and movement (RigidbodyLocomotion)

- Sample capsule: **0.35 m** radius, **0.85 m** cylindrical half-height; collider local Y offset = `radius + halfHeight` so the entity transform is at the feet.
- `RigidbodyLocomotion::move(wish, yaw, dt)` sets target horizontal velocity on the body (accel/friction from `CharacterControllerConfig`); idle ground friction zeroes horizontal slide.
- Gravity comes from the dynamic body (`useGravity`); jump sets upward linear velocity when grounded (feet overlap).
- Facing / feet visuals: entity write-back from the motion body, then yaw from horizontal velocity (+π model offset for `player.gltf`).

## CharacterVirtual (transitional / fallback)

- `CharacterController::move` / `jump` / `ExtendedUpdate` — same accel/friction defaults as above.
- Debug world (`engine run --debug-world`) still spawns `CharacterController` above origin terrain.
- Editor play/test uses RigidbodyLocomotion when the spawn placement has Rigidbody + Collider; otherwise falls back to `CharacterController`.

## Partition ownership

Placement motion bodies are owned by the placement partition cell and unload with `CollisionWorld::unload_cell()`. The controller/locomotion handle is session-local.

## Debug integration

- **Editor play test** (Game tab): **WASD** / **Space** on the Rigidbody path when the player prefab has Rigidbody.
- Collision debug draws dynamic capsules/boxes (orange) for Rigidbody motion bodies; CharacterVirtual capsule wireframe when a controller instance is supplied.

## API

- `RigidbodyLocomotion(world, body, config, capsule_radius, capsule_half_height)`
- `CharacterController::create(collision_world, spawn, config)` — transitional

## Root motion (DEC-0030 / TICKET-0104 / TICKET-0199)

- Prefer `sync_rigidbody_root_motion(CollisionWorld, CollisionBody, …)` / `apply_rigidbody_root_motion` for Rigidbody-backed entities.
- `sync_character_root_motion` remains for `CharacterController` callers.
- When `applyRootMotion: true`, clip root deltas drive locomotion; WASD wish is not used for walk distance.

## Limitations

- Stair stepping is weaker than CharacterVirtual `ExtendedUpdate` (deferred polish).
- No coyote time / double jump yet.
- Visual in-place root stripping for skinned meshes is not yet applied (capsule sync only).
- No nav-grid snap yet.
- Character-vs-character collision is not registered.
- Navigation grid queries are not yet used for pathing or snap-to-walkable.
- **Swim mode** transitional CharacterVirtual path includes swim hooks; Rigidbody swim remains follow-on — [DEC-0039](../decisions/index.md#dec-0039-water-swim-and-hydrology-authoring) / [`water-hydrology.md`](water-hydrology.md).

## Swim mode (planned — DEC-0039)

When the capsule enters authored water:

- Switch from walk locomotion to **swim** (surface and submerged variants TBD).
- **Shallow** water: wade or reduced swim cost (depth threshold TBD).
- **Deep** water: fatigue drain while swimming; at exhaustion, **health damage over time**.
- Jump/air rules TBD while partially submerged.
- Ships use scripted paths; player aboard uses vehicle state rather than free swim unless overboard.

Implementation tracks [`water-hydrology.md`](water-hydrology.md) and stamina/HUD when fatigue UI ships.
