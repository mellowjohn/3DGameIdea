# Character Assets

Character assets use the `.character.json` suffix and schema version 1. They define the test-session player controller and visual prefab.

## Fields

- `visualPrefab`: project-relative compositional prefab used for the visible player body during playtests.
- `rig` (optional): project-relative `*.rig.json` with IK hooks + retarget bone roles ([`rig-assets.md`](rig-assets.md), TICKET-0106).
- `capsuleRadius`, `capsuleHalfHeight`: Jolt capsule controller dimensions in meters.
- `maxSlopeRatio`, `stepHeight`, `maxSpeed`, `gravity`, `jumpVelocity`: movement tuning passed to `CharacterController`.
- `maxSpeed`: maximum horizontal travel speed in meters per second. Input with magnitude `<= 1` is treated as a direction (optionally scaled for analog sticks). Magnitudes above `1` (for example keyboard diagonals before normalization) are normalized to this speed.
- `appearance` (optional): creation / preview sockets, **separate from inventory armor**.
  - `hairMesh`: project-relative skinned glTF on the hair-cap (empty = shaved). Test mesh: `assets/models/test_hair_spikes.gltf` (`keepMeshes: kit_test_hair_spikes`, `matchPlayerBake`).
  - `hairTint` / `skinTint` / `eyeTint`: RGB multipliers (skin/eye tints apply on the body atlas; hair tint is stored for follow-on shader multiply).
  - Option tables (`short` / `shaved`, `warm_tan` / `fair` / `olive` / `deep_brown`, `brown` / `green` / `blue` / `hazel`) live in `CharacterAsset` helpers. Animation Studio **Appearance sockets** and MCP `set_appearance` cycle those ids. Creation Lua writes `appearance.hair` / `appearance.skin` / `appearance.eyes` blackboard keys during A0-02.

## Project binding

`play.session.json` at the project root references the active character asset and camera asset for editor test sessions.

## Editor

Select a placed **player spawn** in the scene to edit movement and visual settings in the Inspector. Use **Apply to Placement** to store overrides on that entity (saved with the world). **Reset from Asset File** reloads the linked `.character.json`.

When nothing is selected, the Inspector only shows default test-session asset bindings (`play.session.json`). Use Asset Browser **Inspect** to edit the underlying `.character.json` or `.camera.json` files directly.

Changes apply on the next test session start.

## NPC humanoids (shared player mesh)

Humanoid NPCs reuse GoodPlayerModel (`assets/models/player.gltf`) and the player rig — do not fork a second body mesh ([character-direction.md](../art/character-direction.md)). Prefabs omit `characterAsset` so they are not player spawns. Play-test spawn resolution also skips any placement whose prefab has `npcAi` (even if a stale `characterAsset` was saved). Editor sync strips `characterAsset` from those placements and only reverse-matches the play-session character’s `visualPrefab`. Play-test attaches a **separate animator instance per entity**, so NPC Idle does not share the player’s locomotion pose.

| Asset | Prefab | Role |
| --- | --- | --- |
| `npc_humanoid` | `assets/prefabs/NPC/npc_humanoid.prefab.json` | AI-ready clone of the player prefab: same mesh/scale, same `player.animator.json` (default Idle), capsule collider + dynamic rigidbody, `combatHurt: npc_body`, `npcAi.kind=hostile`, `heldItemId` Ashfell. Combat sandbox placement `npc_humanoid`. |
| `npc_test` | `assets/prefabs/NPC/npc_test.prefab.json` | Sandbox talk stand-in (`talk_sandbox`). Capsule + talk trigger; no rigidbody. |
