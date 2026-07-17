# Test Coverage Matrix

| Area | Status | Automated evidence |
| --- | --- | --- |
| Core errors/results | tested | `core`, `regression_all` |
| ECS/scenes/prefabs/commands | tested | `world`, `regression_all` |
| World placement editing | active | `world` validates placement serialization, cell derivation/reassignment, invalid prefab paths, changed UUID reporting, and place/move/remove undo/redo |
| Assets/dependencies/hot reload | tested | `assets`, `regression_all`, `project_validation` |
| glTF mesh import | active | `assets` validates embedded-buffer indexed triangles, the skeletal/skin subset (joints/weights/IBM), and malformed skinning input; `project_validation` imports project meshes; editor GPU suites render prefab-selected mesh ranges |
| Animation clip import + hot reload | active | `assets` validates glTF TRS clip import, LINEAR sampling, rejected CUBICSPLINE/bad targets, and `AnimationClipLibrary` validate-then-replace hot reload (keeps prior clips on failure) |
| Material assets | active | `assets` validates deterministic round trips, malformed input, scalar bounds, and opacity consistency; `project_validation` validates project materials; `collision` and `debug_world_smoke` cover runtime integration |
| World Forge factions | active | `world_forge` loads sample `factions.worldforge.json`, round-trips JSON, rejects empty/duplicate id, bad kind, unknown parentId; project validate loads when present |
| World Forge pantheon | active | `world_forge` loads sample `pantheon.worldforge.json`, round-trips JSON, rejects unknown parentId and parent cycles; MCP validate pantheon |
| World Forge archetypes | active | `world_forge` loads sample `archetypes.worldforge.json`, round-trips JSON, rejects bad kind and unknown unlock.factionId when factions known; MCP validate archetypes |
| World Forge relationships | active | `world_forge` loads sample `relationships.worldforge.json`, round-trips JSON, rejects bad node/edge ids, unknown node refs, self-loops, unknown node parentId / parent cycles, unknown faction refs when factions known; project validate cross-checks |
| World Forge map | active | `world_forge` loads sample `map.worldforge.json`, round-trips JSON, rejects empty region id, unknown POI regionId, bad link refs, self-links, unknown factionIds when known; resolve_map_endpoint_anchor + marker keys + camera XZ round-trip; project validate cross-checks |
| World Forge MCP apply | active | `automation` classifies `world_forge`; `apply_world_forge_operation` get/validate factions+pantheon+map; apply rejects invalid payload |
| World Forge editor UI | active (manual) | Viewports **World Forge** tab: Hierarchy + Archetypes + Relationships/Map (**List** + **Canvas** anchors/links + terrain underlay) + Relationships **Graph**; toolbar **Act** lens (DEC-0036); reload/save via `apply_world_forge_operation`; no automated GUI test |
| World Forge Act lens | active | `world_forge` validates `acts` ids (`WORLD-FORGE-ACT-ID`); filter helpers treat empty acts as campaign-wide; sample quests expose `acts=[act0]` |
| World Forge Scene markers | active (manual) | Diagnostics **Show World Forge map markers** draws poles/labels in Scene/Sculpt; focus button; no automated GUI test |
| World partition/streaming | tested | `streaming`, `regression_all` |
| Terrain metadata/cell persistence | tested | `terrain`, `regression_all` |
| Low-poly terrain generation and collision | active | `terrain` validates topology, determinism, invalid inputs, seamless borders, cell addressing, material regions, neighborhood loading, 16 km² stress bounds, and streamed collision ownership; `collision` validates heightfield ray hits and cell unloading; `debug_world_smoke` validates GPU integration |
| Direct3D 12 walking skeleton | smoke tested on real GPU | `renderer_smoke` |
| Jolt collision integration | active | `collision` validates shapes, bodies, ray/overlap/sweep queries, prefab triggers, streamed ownership, unloading, and step bounds |
| Prefab collision and editor debug | active | `collision` and `world` cover authored volumes; editor steps physics for placement collision and drains trigger contact points into the collision debug overlay |
| Navigation grid | active | `navigation` validates grid build, streaming, nearest-walkable, and cross-cell line-of-walk queries |
| Character controller | active | `character` validates capsule creation, terrain landing/walking, slope limits, streamed terrain coexistence, cell ownership, and debug body metadata; `debug_world_smoke` exercises GPU integration |
| Interaction volumes | active | `interaction` validates overlap enter/exit, prefab-authored bindings, and placement spawn integration |
| Combat hit/hurt volumes | active | `combat` validates hurt overlap queries, hit-body queries, prefab-authored bindings, and placement spawn integration |
| Viewport mesh picking | active | `world` validates ray/AABB picking helpers and compositional prefab part bounds |
| Physics visual bridge | active | `debug_world_smoke` steps Jolt and feeds live body transforms into D3D12 |
| Free debug camera and 3D bridge | tested and visually captured | `camera` validates free and orbit cameras; `debug_world_smoke` validates depth-buffered D3D12/Jolt integration |
| Runtime diagnostics | tested | `diagnostics` validates error counts, bounded feed, and JSONL persistence |
| Editor MVP | active | `editor_smoke` validates SDL3/ImGui/ImGuizmo/D3D12 initialization and offscreen rendering; `editor_responsive_smoke` covers a 900×600 layout; deterministic captures verify panels, terrain, proxy, and gizmo composition; collision debug overlay shows placement volumes and recent trigger contacts |
| Navigation, animation, dialogue, VFX, gameplay | partial | `navigation` covers walkability grids; `character` covers capsule traversal; `assets` covers animation clip import/hot reload; `animator` covers controller graphs / blend trees / Lua drive / root-motion capsule sync; dialogue, VFX, and gameplay remain untested |

CTest entries represent suites or integrations. Each named unit suite prints JSON containing assertion, pass, and failure counts. `regression_all` remains a broad backstop while assertions migrate into focused suites.
