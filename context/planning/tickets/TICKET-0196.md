# TICKET-0196: Authored rigidbody component schema + Add Component

- Epic: EPIC-0015
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/3a0d3efc569581d4a252eb729d6143b0

## Goal

Add a universal authored **`rigidbody`** component type (prefab + scene entity Add Component) with serialized fields for dynamic/kinematic motion, so any prefab can opt into physics the same way the player will — without yet changing runtime spawn or player locomotion.

## Context links

- [DEC-0038](../../decisions/index.md#dec-0038-authored-rigidbody--dynamic-bodies-for-player-and-entities)
- [DEC-0016](../../decisions/index.md#dec-0016-entity-attached-components-and-dual-mcp-apply-paths) / [DEC-0017](../../decisions/index.md#dec-0017-prefab-and-scene-component-authoring-with-unity-like-inheritance)
- [`../../architecture/components.md`](../../architecture/components.md)
- [`../../formats/prefab-assets.md`](../../formats/prefab-assets.md)
- TICKET-0147 (component authoring path to extend)
- Follow-ons: TICKET-0197 (runtime spawn), TICKET-0198 (player migrate)

## Acceptance criteria

- [x] `AuthoredComponentType` includes `Rigidbody` / JSON `"rigidbody"`.
- [x] Rigidbody payload fields documented and validated: at least `motionType` (`dynamic` \| `kinematic`), `mass`, `linearDamping`, `angularDamping`, `useGravity`, `freezeRotation` (or equivalent axis locks).
- [x] Prefab Editor and Scene Inspector can Add/Remove/edit Rigidbody with inherit/override like Collider/Animator.
- [x] MCP / command path can add or edit Rigidbody on prefab and entity (existing apply tools or documented extension).
- [x] `context/architecture/components.md` + prefab format docs list the type; sample optional (may leave player without Rigidbody until 0198).
- [x] Suites cover parse/validate round-trip and reject invalid mass/damping.

## Out of scope

- Spawning Jolt dynamic bodies from the component (0197)
- Replacing `CharacterController` / player move (0198)
- Physics materials asset system beyond simple damping fields
- Ragdolls / joints

## Dependencies

- Soft: TICKET-0147 component authoring already landed
- Blocks: TICKET-0197

## Verification

Rebuild `engine` + `engine_suite_tests` (Debug). Suites: `animator` 109/109, `assets` 61/61, `world` 54/54. Existing MCP `add_component` / `engine_prefab_apply` paths accept the new JSON type via shared parse (no new tool required).

## What changed

**Summary.** Prefabs and scene entities can author a universal `rigidbody` component (mass, damping, gravity, freeze rotation, dynamic/kinematic). Editor Add Component works in Scene Inspector and Prefab Editor. Runtime physics spawn is still TICKET-0197.

**Files / surfaces touched.**
- `include/engine/world/authored_components.h`, `src/world/authored_components.cpp` — enum, data, validate, JSON, seed, propagate
- `include/engine/assets/prefab_asset.h`, `src/assets/prefab_asset.cpp` — `PrefabRigidbody` load/save under `components[]`
- `src/rendering/render_app.cpp` — Inspector + Prefab Editor UI
- Docs: `components.md`, `prefab-assets.md`
- Tests: `tests/suite_tests.cpp` (animator suite rigidbody parse/validate)

**Schema / API / format deltas.** New type `"rigidbody"`; data fields camelCase (snake_case accepted on read). Errors: `COMPONENT-RIGIDBODY-*`, `PREFAB-RIGIDBODY-*`.

**Seed / sample data.** None on player yet (0198).

**Tests / verification evidence.** See Verification above.

**Decisions & tradeoffs.** MCP uses existing component apply path (Animator pattern). Runtime body spawn deferred to 0197.

**Leftover risk / follow-ons.** Authored Rigidbody does nothing at runtime until 0197; player still CharacterVirtual until 0198.

## Agent notes

Owner accepted DEC-0038; EPIC-0015 created; 0196 implemented 2026-07-17.
