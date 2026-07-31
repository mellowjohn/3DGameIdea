# Character Assets

Character assets use the `.character.json` suffix and schema version 1. They define the test-session player controller and visual prefab.

## Fields

- `visualPrefab`: project-relative compositional prefab used for the visible player body during playtests.
- `rig` (optional): project-relative `*.rig.json` with IK hooks + retarget bone roles ([`rig-assets.md`](rig-assets.md), TICKET-0106).
- `capsuleRadius`, `capsuleHalfHeight`: Jolt capsule controller dimensions in meters.
- `maxSlopeRatio`, `stepHeight`, `maxSpeed`, `gravity`, `jumpVelocity`: movement tuning passed to `CharacterController`.
- `maxSpeed`: maximum horizontal travel speed in meters per second. Input with magnitude `<= 1` is treated as a direction (optionally scaled for analog sticks). Magnitudes above `1` (for example keyboard diagonals before normalization) are normalized to this speed.

## Project binding

`play.session.json` at the project root references the active character asset and camera asset for editor test sessions.

## Editor

Select a placed **player spawn** in the scene to edit movement and visual settings in the Inspector. Use **Apply to Placement** to store overrides on that entity (saved with the world). **Reset from Asset File** reloads the linked `.character.json`.

When nothing is selected, the Inspector only shows default test-session asset bindings (`play.session.json`). Use Asset Browser **Inspect** to edit the underlying `.character.json` or `.camera.json` files directly.

Changes apply on the next test session start.

## NPC test stand-in

`assets/characters/npc_test.character.json` + `assets/prefabs/NPC/npc_test.prefab.json` reuse the Player_V2 `player.gltf` mesh (finger/thumb joints included) for temporary NPC placement. The NPC prefab omits `characterAsset` so it does not register as a player spawn, and includes the same `player.animator.json` controller (default Idle). Play-test attaches a **separate animator instance per entity**, so NPC Idle does not share the player’s locomotion pose. Editor sync only reverse-matches the play-session character’s `visualPrefab` (not every `.character.json`), so NPC stand-ins stay non-spawn.
