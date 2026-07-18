# TICKET-0199: Root-motion retarget + sample physics prefabs

- Epic: EPIC-0015
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/3a0d3efc5695817fa55af1b0c50bf90e

## Goal

Retarget [DEC-0030](../../decisions/index.md#dec-0030-animation-driven-root-motion) root-motion sync from `CharacterController` onto Rigidbody-backed entities, and ship at least one non-player sample prefab (e.g. pushable crate) using Rigidbody + Collider to prove universal Apply Component reuse.

## Context links

- [DEC-0038](../../decisions/index.md#dec-0038-authored-rigidbody--dynamic-bodies-for-player-and-entities)
- [DEC-0030](../../decisions/index.md#dec-0030-animation-driven-root-motion)
- TICKET-0198 (player path — soft prerequisite)
- [`../../features/animator.md`](../../features/animator.md)

## Acceptance criteria

- [x] Root-motion helper applies deltas to a Rigidbody-backed entity (documented API).
- [x] At least one sample non-player prefab uses Rigidbody + Collider and validates.
- [x] Animator / character-controller docs updated for the new sync target.
- [x] Regression coverage for root-motion + rigidbody path (or documented skip if animator fixtures unavailable).

## Out of scope

- New animation clips
- Full physics materials catalog

## Dependencies

- Soft blocked by: TICKET-0198
- Soft: TICKET-0104 done

## Verification

- Rebuilt `engine` + `engine_suite_tests`.
- animator **354/354**; camera **17/17**; assets **61/61**.
- `engine validate --project samples/open-world-rpg` → exit 0 (132 entities).

## What changed

- Summary: Added `sync_rigidbody_root_motion` / `apply_rigidbody_root_motion` for DEC-0030 on dynamic CollisionBodies. Shipped `crate.prefab.json` (box + Rigidbody) and placed a Physics Crate near the player in the sample world. Play camera default distance raised 5→8 m (max 8→14) to reduce zoom-in.
- Files / surfaces: `root_motion.h`; `crate.prefab.json`; `vertical-slice.world.json`; `game.camera.json` + CameraAsset/OrbitCamera defaults; animator/character-controller docs; animator suite.
- Schema / API: `apply_rigidbody_root_motion`, `sync_rigidbody_root_motion` (CharacterVirtual helper retained).
- Seed / sample: Scene Assets crate prefab + world placement.
- Tests / verification: animator suite covers rigidbody root-motion forward move; project validate OK.
- Decisions & tradeoffs: root motion sets horizontal linear velocity (Y left to gravity unless `rootMotionY`); camera zoom is a sample-asset tweak alongside this ticket.
- Leftover risk: play loop does not yet auto-switch WASD vs root-motion from animator flags (API + suite only); crate angular free tumble intentional.

## Agent notes

- Also addressed owner note: play orbit camera felt too zoomed in.
