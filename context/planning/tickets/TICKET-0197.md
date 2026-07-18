# TICKET-0197: Spawn/sync dynamic bodies from Rigidbody + Collider

- Epic: EPIC-0015
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/3a0d3efc569581a38cc9fbbcd3f959c6

## Goal

When a placed prefab/entity has authored **Rigidbody** + **Collider**, spawn and sync a Jolt body in `CollisionWorld` (dynamic or kinematic per component), writing transform back each step — the runtime half of the universal physics component.

## Context links

- [DEC-0038](../../decisions/index.md#dec-0038-authored-rigidbody--dynamic-bodies-for-player-and-entities)
- TICKET-0196 (schema — blocker)
- [`../../architecture/components.md`](../../architecture/components.md)
- `CollisionWorld` / `PlacementCollisionTracker`
- Follow-on: TICKET-0198

## Acceptance criteria

- [x] Placement sync creates a dynamic/kinematic body from Rigidbody + effective Collider volumes.
- [x] Entities without Rigidbody keep today’s static/trigger collider behavior.
- [x] Transform write-back from body → entity (or documented ownership) each physics step while in play/test.
- [x] Partition unload removes bodies with the cell (same ownership rules as other placement bodies).
- [x] Debug overlay can show dynamic Rigidbody capsules/boxes.
- [x] Tests: dynamic body falls / rests; kinematic moves only when driven; no Rigidbody ⇒ static path unchanged.

## Out of scope

- Player WASD locomotion migration (0198)
- CharacterVirtual removal
- Full physics material library
- Multi-solid compound shapes (first solid collider only)

## Dependencies

- Blocked by: TICKET-0196
- Blocks: TICKET-0198

## Verification

- Rebuilt `engine` + `engine_suite_tests` (MSVC Debug).
- `engine_suite_tests --suite collision --json` → 567/567 passed.
- Editor: play/test sets `simulate_dynamics`; idle parks dynamics as kinematic.

## What changed

- Summary: Placements with Rigidbody + Collider now spawn a Dynamic-layer Jolt motion body (mass/damping/gravity/freezeRotation via `CollisionBodySettings`). Play/test writes body pose back to the entity each step; edit mode parks authored dynamics as kinematic so crates do not fall. No Rigidbody keeps the previous static/trigger path.
- Files / surfaces touched: `collision_world.h/.cpp` (settings + rotation/set_transform); `prefab_collision.h/.cpp` (physics-driven sync + write-back); `transform_utils` (inverse); `render_app.cpp` (simulate_dynamics + write-back); docs `components.md`, `collision-debug.md`; collision suite tests.
- Schema / API / format deltas: `CollisionBodySettings`, `CollisionMotionType`; `PlacementCollisionTracker::sync(..., simulate_dynamics)`; `write_back_transforms`; no new MCP tools.
- Seed / sample data: none (crate samples deferred to 0199).
- Tests / verification evidence: collision 567/567; engine rebuild OK.
- Decisions & tradeoffs: first solid collider only for motion body; sensors remain separate; physics-driven placements skip transform-equality respawn so write-back does not fight sync; editor idle uses kinematic park (`simulate_dynamics=false`).
- Leftover risk / follow-ons: player still on CharacterVirtual (0198); multi-solid compounds deferred; cell unload clears Jolt bodies but tracker stale handles cleared on next full sync/clear.

## Agent notes

-
