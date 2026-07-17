# Architecture Overview

Status: Foundation implementation active.

The product is a Windows-first C++20 and Direct3D 12 engine for a single-player third-person action RPG in a seamless 4×4 km world. The runtime uses a hybrid ECS and authored hierarchy, with Lua for content-heavy gameplay logic.

Module boundaries are platform, core, rendering, world/ECS, assets, physics/collision, audio, animation, dialogue, particles/VFX, gameplay, scripting, editor, automation, diagnostics, and testing. Milestone 1 implements core, automation, and diagnostics. Other modules remain planned and must not be represented as complete.

Entity component catalog (core ECS + authored collider / scriptBinding / animator, inherit/override, authoring matrix): [components.md](components.md).

For routing feature work between C++ engine changes and MCP-driven Lua, prefab, and scene edits, see [content-vs-engine-workflows.md](content-vs-engine-workflows.md).

## Foundation Contracts

- Recoverable APIs return `Result<T>` containing either a value or `EngineError`.
- CLI responses use schema version 1, stable exit codes, summaries, changed object IDs, diagnostics, numeric metrics, metadata, and artifact paths.
- Structured logs use JSON Lines. Serious failures create a local diagnostic bundle; unhandled Windows exceptions also attempt a minidump.
- Authoritative project data is versioned and text-based. Generated runtime data is replaceable.

## Content and Collision Contracts

- Animation supports skeletal clips, blend trees, layered state machines, root motion, events, inverse-kinematics hooks, retargeting metadata, and previews. **C++ owns** the animator backend (controller graph, playback, blending); **Lua drives** parameters/state requests from gameplay scripts and reacts to animation events ([DEC-0022](../decisions/index.md#dec-0022-c-animator-backend-with-lua-drive-api), [`features/animator.md`](../features/animator.md)). Missing clips or invalid transitions fall back safely with entity-aware diagnostics.
- Dialogue uses versioned graph assets with speakers, conditions, choices, events, localization keys, voice references, and resumable state. Validation detects unreachable nodes, broken references, and missing localization.
- Particle effects use reusable emitter graphs, CPU/GPU simulation, deterministic test seeds, pooling, LOD, bounds, collision hooks, and budgets. Missing effects degrade to diagnostic placeholders without interrupting gameplay.
- Collision covers static and dynamic world geometry, character controllers, triggers, ray/shape queries, combat hit/hurt volumes, animation-driven shapes, particle collision, navigation obstacles, filtering layers, continuous collision, and editor visualization.
- All edits use the shared command/undo interface and support headless JSON automation.

## Open-World Contracts

- Authoritative positions use double-precision world coordinates; runtime transforms are relative to a rebased local origin.
- The 4×4 km world is partitioned into 128 m cells. Cell addressing uses mathematical floor semantics so negative coordinates map correctly.
- Streaming requests carry generations and cancellation tokens. Completed work is committed only if it still belongs to the active generation.
- Loaded, simulation-active, and visible sets are distinct. Persistence stores stable entity state by cell without serializing transient runtime handles.
- Collision queries use explicit object layers, filters, bounded results, and correlation-aware errors. Streaming never exposes partially constructed collision state.
