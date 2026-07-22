# Roadmap

Cross-cutting product epics and tickets (World Forge, narrative planning, shaders, quests/dialogue, UI) live in [`planning/epics.md`](planning/epics.md) under [DEC-0015](decisions/index.md#dec-0015-hybrid-project-tracking). Milestone exit criteria below remain the engineering delivery gates.

## M0 — Product and Architecture Contract (complete)

- Complete the focused owner interview.
- Record target demo, platforms, dependency policy, AI workflows, and quality targets.
- Accept an architecture and executable milestone plan.

Exit criterion met: decisions DEC-0001 through DEC-0004 define the product, stack, automation, diagnostics, and performance target.

## M1 — Foundation (complete)

- Complete: C++20 layout, CMake/vcpkg manifests, typed errors, structured logger, crash bundle, minidump handler, unified CLI contracts, foundation tests, and sample project.
- Verified: direct MSVC compilation, foundation tests, JSON validation, asset dry-run, and stable unavailable-feature response.
- Complete: portable CMake and pinned vcpkg bootstrap now configure and build successfully. Catch2 integration remains scheduled through the optional testing dependency feature.

## M2 — Direct3D 12 Walking Skeleton (complete)

- Complete: SDL3 window, D3D12 device/swap chain, shader compilation, debug triangle, resize handling, device-removal diagnostics, synchronization, PPM capture, CPU/GPU timing, structured CLI metrics, hardware run, benchmark, and automated GPU smoke test.
- Verified: hidden render and capture on an NVIDIA RTX 2080 SUPER; see `context/benchmarks/milestone2.md`.
- Note: the installed Windows SDK predates current DRED interfaces. Device-removal HRESULT diagnostics are implemented; richer DRED breadcrumbs remain a toolchain-upgrade enhancement rather than an M2 blocker.

## M3 — World and Asset Model (complete)

- Complete: EnTT-backed UUID entities, hierarchy, schema-v1 scenes, atomic saves, command history, prefab instantiation with UUID remapping and rollback, content-hashed assets, dependency/cycle validation, incremental registry builds, validated change monitoring, and CLI metrics/artifacts.
- Added initial real-mesh path: fastgltf validation and triangle expansion, project-wide glTF discovery, prefab mesh references, shared D3D12 mesh uploads, per-instance mesh ranges, and diagnostic proxy fallback.
- Verified: malformed IDs/JSON, duplicates, hierarchy/dependency cycles, self-parenting, missing entities/assets, invalid transforms, deterministic round trips, history branching, parent destruction, atomic backups, prefab remapping, modified assets, no-op rebuilds, and rejected invalid hot-reload snapshots.
- Added editor foundation: placed prefab references, transform-derived partition ownership, undoable place/move/remove commands, deterministic changed-object summaries, and placement serialization.

## M4 — Open-World Runtime and Collision (complete)

- Complete foundation: double-precision world positions, 128 m cells, correct negative-coordinate mapping, bounded extent validation, local origin rebasing, generation-tagged async loads, cancellation tokens, stale-completion rejection, bounded radii, and cell schema/identity validation.
- Verified: boundary coordinates, non-finite/out-of-world positions, rebase thresholds, radius safety, nine-cell loading, rapid traversal unloading, and corrupt cell rejection.
- Complete additional slice: versioned persistent cell state with atomic backups, deterministic simulation bubbles, and validated terrain/foliage tile metadata.
- Complete navigation foundation: partition-aligned 128 m walkability grids sampled from terrain height, streamed load/unload, nearest-walkable queries, cross-cell line-of-walk validation, and `navigation` suite coverage.
- Complete collision foundation: Jolt lifecycle, object/broad-phase layers, static/dynamic boxes and spheres, sensors, bounded stepping, ray casts, sphere/box overlap queries, swept sphere casts, contact/trigger events, prefab-authored collision volumes with cell ownership, and transactional streamed-cell body unloading.
- Complete editor interaction slice: per-part mesh AABB viewport picking with proxy fallback, collision debug overlay for terrain and placement volumes, and trigger contact-point visualization.
- Complete visual foundation: depth-buffered D3D12 vertex pipeline, perspective camera, ground plane, physics-driven 3D cube, WASD/vertical movement, boost, and captured mouse look.
- Complete initial terrain slice: deterministic smooth low-poly heightfield mesh, seamless adjacent-cell samples, muted dark-fantasy facet colors, and matching Jolt heightfield collision in the debug world.
- Complete initial material slice: schema-v1 diffable material assets, strict project validation, deterministic serialization, terrain color controls, and Jolt friction/restitution controls. Opaque PBR lighting is implemented; masked/blended transparency remains pending dedicated pipelines.
- Deferred polish: full triangle mesh picking, Recast/detour integration, and combat volumes (tracked under M5).

Exit criterion met for the collision and navigation slice: named `collision`, `navigation`, and `world` suites pass; editor hidden smoke passes with placement collision and debug overlays enabled.

## M5 — Character, Animation, and Interaction

Add character controllers, camera collision, combat hit/hurt volumes, skeletal import, blend trees, layered state machines, root motion, events, animation-driven collision, IK hooks, retargeting, audio, and interaction. Exit requires automated transition/collision tests and editor/CLI previews.

- Complete initial character slice: Jolt capsule `CharacterVirtual`, terrain walking with slope limits and stair stepping, partition cell ownership tracking, debug-world WASD traversal, editor collision debug capsule, and `character` suite coverage.
- Complete orbit camera slice: third-person follow with swept-sphere collision shortening, debug-world integration, and `camera` suite coverage.
- Complete interaction volume slice: prefab `interaction` fields, overlap enter/exit events, editor debug highlighting, and `interaction` suite coverage.
- Complete combat hit/hurt slice: prefab `combatHit`/`combatHurt` fields, overlap hit queries, editor debug highlighting, and `combat` suite coverage.
- Complete authoring-foundation slice (paused M5 animation work): material asset inspector/save, persisted terrain height edits, editor sculpt brush with undo/redo, streamed render/collision refresh, and validation/tests.
- Complete animation engineering slice (TICKET-0101–0106): glTF clips, hot reload, blend trees/state machines, root motion, timeline events → Lua, rig metadata.
- **M5 exit gate (TICKET-0110):** `engine test --suite m5-exit` + `engine animation-preview` provide headless verification; see [`features/animator.md`](features/animator.md).
- **Audio (TICKET-0107):** miniaudio integration **needs-approval** — spatial listener, Lua play, `audio` suite; see [`features/audio.md`](features/audio.md). Remaining before M6: editor Animation tools panel (TICKET-0135); GPU skinning polish deferred.

## M6 — Lua and RPG Data

Add sandboxed Lua APIs, stats, items, inventory, quests, abilities, atomic versioned saves, migrations, and safe collision-query bindings.

## M7 — Dialogue and Narrative Tools

Add branching dialogue graphs, conditions, choices, gameplay events, localization keys, voice references, save/resume state, validation, headless traversal tests, and a command-backed graph editor.

## M8 — Particles and Visual Effects

Add CPU/GPU emitters, effect graphs, pooling, deterministic seeds, collision hooks, LOD, bounds, budgets, hot reload, CLI captures, and editor previews. Integrate effects with animation events, combat, abilities, and interactions.

## M9 — Melee Vertical Slice

Deliver exploration, lock-on, attacks, dodge, stamina, damage reactions, one weapon, one enemy, interaction, inventory, quests, dialogue, save/load, animation, audio, collision feedback, and combat VFX in one streamed region.

## M10 — Integrated Editor

Deliver the command-backed viewport, hierarchy, inspector, asset browser, console, profiler, world partition, collision visualization, animation, dialogue, and particle tools with play mode and undo/redo.

Active initial slice: Dear ImGui/SDL3/D3D12 lifecycle, locked responsive panel layout, offscreen viewport texture, imported mesh instances, proxy fallback, click picking, terrain placement cursor, ImGuizmo move/rotate/scale controls with non-conflicting `1`/`2`/`3` shortcuts, hierarchy, transform/placement inspector, asset browser, diagnostics, atomic world save, prefab placement, and undo/redo. Mesh-accurate picking, thumbnails, drag-and-drop, play mode, and specialized tools remain pending.

## M11 — Profiling and Packaging

Meet the 1440p/60 FPS target, harden failure paths, run the complete suite, package the playable slice, and publish reproducible benchmarks.
