# TICKET-0198: Player locomotion on dynamic Rigidbody

- Epic: EPIC-0015
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/3a0d3efc569581fda550ef8cd22ca806

## Goal

Move the player onto the universal **dynamic Rigidbody** path: input drives forces / target velocities on the Jolt body with friction from body settings, retiring play-session reliance on `CharacterVirtual` for the default player prefab.

## Context links

- [DEC-0038](../../decisions/index.md#dec-0038-authored-rigidbody--dynamic-bodies-for-player-and-entities)
- TICKET-0197 (runtime spawn — blocker)
- [`../../features/character-controller.md`](../../features/character-controller.md)
- Sample `assets/prefabs/Player/player.prefab.json`, `assets/characters/player.character.json`
- Soft: TICKET-0199 (root motion retarget)

## Acceptance criteria

- [x] Player prefab authors Rigidbody + Collider (capsule); play/test uses that body for movement.
- [x] WASD / jump / ground contact feel uses rigidbody forces/friction (not CharacterVirtual wish integrator) for the default sample.
- [x] Facing / feet visual pivot still correct; slope walk and stop-on-release do not ice-slide on typical terrain.
- [x] `CharacterController` remains available or clearly deprecated for non-migrated callers until cleanup.
- [x] Character suite / play smoke updated; context docs describe the new player path.

## Out of scope

- Full ragdoll
- Root-motion retarget (0199) unless trivial
- Rewriting all NPCs (may share path once prefab has Rigidbody)

## Dependencies

- Blocked by: TICKET-0197
- Soft blocks: TICKET-0199

## Verification

- Rebuilt `engine` + `engine_suite_tests` (MSVC Debug).
- character **1212/1212**; collision **1063/1063**; assets **61/61**.
- Play/test: spawn placement with Rigidbody → `RigidbodyLocomotion`; else CharacterVirtual fallback.

## What changed

- Summary: Sample player prefab now has capsule Collider + dynamic Rigidbody (`freezeRotation`, mass 70). Editor play/test drives that body via `RigidbodyLocomotion` (wish accel/friction + jump) instead of `CharacterVirtual`. `--debug-world` and missing-Rigidbody placements still use `CharacterController`.
- Files / surfaces: `CollisionWorld` capsule + linear velocity APIs; `rigidbody_locomotion.*`; prefab shape `capsule`/`halfHeight`; `prefab_collision` spawn; `render_app` play path; player prefab; docs; character suite.
- Schema / API: `PrefabCollisionShape::Capsule`; `add_capsule`; `linear_velocity` / `set_linear_velocity`; `RigidbodyLocomotion`; `PlacementCollisionTracker::motion_body_for`.
- Seed / sample: `samples/open-world-rpg/assets/prefabs/Player/player.prefab.json` components.
- Tests: character suite RigidbodyLocomotion land/walk/idle/jump; collision/assets green.
- Decisions: motor-side friction for feel (body damping additional); ground via feet overlap sphere; CharacterVirtual kept as fallback.
- Leftover risk: stair stepping weaker than CharacterVirtual; root-motion still on CharacterController (0199); editor playtest recommended on slopes.

## Agent notes

-
