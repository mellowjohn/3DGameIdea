# Streaming, LOD, and Budget Acceptance (Authored Regions)

Status: active (TICKET-0032 / DEC-0054) — acceptance-oriented design for how Tessera’s **16×16 km** seamless world stays bounded while authored density varies by region band ([map-design-language.md](map-design-language.md), [open-world-navigation.md](open-world-navigation.md)).

Not an implementation ticket. Hard FPS/GPU gates remain [TICKET-0139](../planning/tickets/TICKET-0139.md). Instance camp enter/exit is [DEC-0033](../decisions/index.md#dec-0033-anywhere-player-camp-as-editable-instance-dao-style).

## Current engine baselines (shipped)

| Layer | Cell / neighborhood | Notes |
| --- | --- | --- |
| World partition | **128 m** cells; **16×16 km** AABB (half-extent 8 km) | Ownership, placement, nav grids ([DEC-0054](../decisions/index.md#dec-0054-continent-scale-seamless-world--stream-budget)); cartography plate **16000×10667 m** matches official map aspect |
| Terrain / foliage paint | **40 m** cells, 33×33 samples | Play stream radius **4** with **view bias** (support radius **2**, outer ring toward look) + **amortized** loads; Scene/Sculpt edit uses full-disc editor radius **6** / support **3**; main-menu preview uses radius **5** / support **2** (DEC-0054 floor for establishing shots); look-gate; collision commit / GPU upload / foliage scatter split across frames; bootstrap/teleport still fills support immediately ([debug-world.md](debug-world.md)) |
| Navigation field | 128 m grids | Streamed with focus; queries fail if cell not resident ([navigation-grid.md](navigation-grid.md)) |
| Collision | Streamed bodies per cell | Unload with cells; character not owned by a streamed cell |
| Stress check | **256 km²** walk | Resident terrain cells stay **bounded** (suite / debug-world) |

LOD today is **distance fog + unload**, plus the view-distance ladder below (TICKET-0220 / DEC-0054). Impostor atlases remain design intent. Numeric 1440p/60 capture + provisional budgets: [`../benchmarks/open-world-1440p.md`](../benchmarks/open-world-1440p.md) (TICKET-0139).

**Frustum culling (2026-07-22 / 0220 + 0276):** `draw_world_pass` skips draw calls whose world-space AABB is fully outside the camera frustum (`engine::frustum_from_view_projection` / `frustum_intersects_aabb`, `src/rendering/viewport_picking.cpp`). Foliage **Scene / menu** passes also run a per-instance GPU sphere cull + `ExecuteIndirect` ([`gpu-instance-cull.md`](gpu-instance-cull.md)).

| Geometry | Bounds source |
| --- | --- |
| Terrain cells | Baked cell vertices at GPU upload |
| Placed objects | Mesh-bounds catalog (same as pick/gizmo) |
| Foliage draws | Per **40 m cell × mesh** AABB (cell footprint + blade height); draw units come from `StreamedFoliageField::cell_instances()` |

Conservative — never culls partially visible work. Scene and Game tabs each cull against their own camera. Per-instance CPU culling (no spatial index) may need revisiting if placed-object counts grow much further.

**Mesh distance LOD ladder (TICKET-0220 / TICKET-0277 / DEC-0054):** Placed-mesh path uses distance bands with hysteresis (`include/engine/rendering/mesh_distance_lod.h`). Prefabs may author optional `lod1Mesh` / `lod2Mesh`; missing fields keep full mesh until far cull.

| Band | Threshold | Behavior |
| --- | --- | --- |
| LOD0 (full) | distance < **140 m** (`k_lod1_start_m`) when LODs exist; else through near-full | Draw prefab mesh |
| LOD1 | ≥ **140 m** when `lod1Mesh` set | Swap to mid mesh (hysteresis ±20 m) |
| LOD2 | ≥ **240 m** when `lod2Mesh` set | Swap to far / landmark stand-in |
| Near full (no LODs) | distance ≤ **280 m** (`k_near_full_end_m`) | Always draw full mesh |
| Far cull enter | distance ≥ **360 m** (`k_far_cull_start_m`) | Skip draw; sticky cull level |
| Far cull exit | distance ≤ **280 m** (`k_far_cull_exit_m`) | Leave cull and redraw |

This is **view-distance** only — streaming unload radii are unchanged. Billboard impostor atlases remain a follow-on. Foliage uses scatter falloff (**200–340 m**) plus frustum cull on padded cell×mesh AABBs for Scene / menu free-cam; orbit play-test keeps foliage uncullable (historical bush false-cull). GPU instance cull (TICKET-0278) is enabled for Scene / menu / `engine run` frustum passes.

## What “authored region” means here

A World Forge **region** (hub apron, wilderness weave, chaotic pocket, etc.) is a **density + soft-gate intent** overlay — not a separate streamed “level.” Partition/terrain cells still stream by camera/player focus. Regions must be authorable so that:

1. Hub spikes do not force the whole **16×16 km** into memory.
2. Wilderness empty space stays cheap.
3. Chaotic / landmark blooms are local spikes with unload when the player leaves.
4. Camp / dungeon / vision **instances** swap out overland neighborhood pressure ([DEC-0021](../decisions/index.md#dec-0021-soft-gates-with-rare-optional-instances), [DEC-0033](../decisions/index.md#dec-0033-anywhere-player-camp-as-editable-instance-dao-style)).

## Density bands → streaming expectations

Map language bands ([map-design-language.md](map-design-language.md)) imply budget *shapes*, not fixed draw-call caps yet:

| Band | Streaming expectation | LOD / far field |
| --- | --- | --- |
| Hub core | Highest local props, lights, interactables inside ~few terrain cells | Strong silhouettes; far hubs reduce to landmark mass + warm light cue |
| Hub apron | Medium props/NPCs; farms, roads | Impostor/fog OK beyond stream ring |
| Road corridor | Sparse beads; path props only | Keep road readable in mid distance |
| Wilderness weave | Low entity count; foliage + terrain dominate | Prefer unload over dense LODs |
| Landmark bloom | Medium spike around one POI | One hero silhouette retained farther than clutter |
| Chaotic pocket | Medium–high threat FX/props but **localized** | Must unload cleanly; no world-wide corruption cost |

**Authoring rule:** Prefer clustering content inside a small set of partition cells around anchors. Do not carpet the **256 km²** with hub-density prefabs.

## Resident-set policy (acceptance targets)

These are product acceptance targets (numbers tuned under [TICKET-0139](../planning/tickets/TICKET-0139.md) / [`../benchmarks/open-world-1440p.md`](../benchmarks/open-world-1440p.md)):

### Overland (default play)

1. **Terrain/foliage resident set** stays within a fixed camera/player neighborhood (today: play radius **4** with view bias + support radius **2**; amortize ≤1 support fringe + ≤2 outer cells/frame; hold unload until support complete). Expanding play radius further requires an explicit budget ticket, not silent growth. **Scene/Sculpt edit** uses full-disc radius **6**; **main-menu preview** uses radius **5** so establishing shots stay continuous under Calrenoth without the full 169-cell Scene edit set.
2. **Collision + nav** resident sets track the same focus; queries must fail closed with diagnostics when a cell is missing (already true for nav).
3. **Placement/prefab bodies** unload with their owner cells; no permanent residency for distant hubs.
4. **Point lights:** keep a small active set (debug world already favors nearest placed lights). Hub cores may author more lights, but only nearest N affect shading.
5. After **fast travel** ([DEC-0032](../decisions/index.md#dec-0032-open-world-travel-discovery-map-and-dual-soft-gates)), the destination neighborhood becomes the new focus; previous cells unload within a bounded number of frames/ticks (no dual-hub residency).

### Instances (camp, dungeon, vision)

1. Entering an instance may **unload or freeze** overland streaming pressure; camp is a bounded scene ([DEC-0033](../decisions/index.md#dec-0033-anywhere-player-camp-as-editable-instance-dao-style)).
2. Exit restores overland focus at the pitch/return anchor and rebuilds the neighborhood without retaining instance meshes.
3. Nested instances are denied (already: no camp while inside another instance).

## LOD ladder (shipped + design intent)

| Distance | Presentation |
| --- | --- |
| Near (≤280 m placed; resident foliage cells in frustum) | Full terrain, foliage instances, collision, interactables |
| Mid (280–360 m placed sticky band; stream edge) | Placed meshes may enter sticky far-cull; fog + stream load/unload soften seams |
| Far (>360 m placed, or outside frustum / unloaded) | Culled or unloaded; silhouette landmarks + fog; no collision/sim |
| Map UI | Fog-of-war / discovery dust — not a 3D LOD tier ([DEC-0032](../decisions/index.md#dec-0032-open-world-travel-discovery-map-and-dual-soft-gates)) |

Future impostors / reduced meshes must preserve **value separation** and combat readability ([visual-direction.md](../art/visual-direction.md)). Constants live in `mesh_distance_lod.h` (TICKET-0220).

## Budget categories (for TICKET-0139 + content review)

Acceptance reviews for authored regions should report against these categories (see measured 1440p baselines in [`../benchmarks/open-world-1440p.md`](../benchmarks/open-world-1440p.md)):

| Category | Owned by | Region risk |
| --- | --- | --- |
| Terrain mesh tris | Streaming terrain | Uniform across world if cell res fixed |
| Foliage instances | Foliage density paint | Hub apron / woods spikes |
| Prefab / mesh draws | Scene placements | Hub core / landmark bloom |
| Collision bodies | Prefab colliders | Hub / fortifications |
| Active lights | Prefab point lights | Settlements at night |
| Particles / VFX | Later M8 | Chaotic pockets |
| UI / map | Mini-map ticket | Not overland stream |

**Content gate (qualitative until 0139):** a hub core + apron must remain playable when the player stands in the hub with default stream radius; a chaotic pocket must not permanently raise the global resident budget after the player leaves.

## Validation scenarios (acceptance checklist)

Use these as test/scenario seeds for engineering:

- [ ] **Wilderness traverse:** walk 2+ km of wilderness weave; resident terrain cell count stays within the designed neighborhood bound (suite already approximates world extent).
- [ ] **Hub spike:** stand in Act-1 village density; frame stays interactive; leaving the hub unloads apron cells (no sticky residency).
- [ ] **Landmark bloom:** approach a single wilderness landmark; spike is local; leave and unload.
- [ ] **Chaotic pocket:** enter soft-gated hostile frontier; budgets rise locally; leave and return to wilderness baseline.
- [ ] **Carriage FT:** FT between two discovered posts; only destination neighborhood resident shortly after arrival.
- [ ] **Camp instance:** pitch camp from overland → camp instance loaded, overland pressure dropped/frozen → exit at pitch point → overland neighborhood restores ([DEC-0033](../decisions/index.md#dec-0033-anywhere-player-camp-as-editable-instance-dao-style)).
- [ ] **Nav fail-closed:** query into unloaded partition cell returns structured error (existing nav behavior).
- [ ] **Seam readability:** streamed borders remain seamless; fog hides pop (visual direction).

## Out of scope (this doc / TICKET-0032)

- Implementing new stream radii or GPU profilers (mesh LOD ladder + foliage frustum: TICKET-0220)
- Locking final owner FPS budgets (provisional table lives in [`../benchmarks/open-world-1440p.md`](../benchmarks/open-world-1440p.md); TICKET-0139)
- Mini-map rendering (TICKET-0061)
- Recast/detour (TICKET-0109)
- Final region layouts / art production
- Nanite / Hi-Z occlusion / impostor atlases

## Open preferences

- Exact stream radius for shipping vertical slice vs debug-world (keep 2 unless measured need).
- Whether camp unload is full teardown vs cached hibernate of last overland ring.
- Per-platform budget tables (Windows desktop first per DEC-0001).

## Related

- Map density: [map-design-language.md](map-design-language.md)
- Travel / FT / camp: [open-world-navigation.md](open-world-navigation.md)
- Debug stream demo: [debug-world.md](debug-world.md)
- Terrain/foliage stream: [terrain-authoring.md](terrain-authoring.md)
- Ticket: [TICKET-0032](../planning/tickets/TICKET-0032.md)
- 1440p/60 gate: [`../benchmarks/open-world-1440p.md`](../benchmarks/open-world-1440p.md) (TICKET-0139)
