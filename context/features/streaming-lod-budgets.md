# Streaming, LOD, and Budget Acceptance (Authored Regions)

Status: active (TICKET-0032) — acceptance-oriented design for how Tessera’s 4×4 km world stays bounded while authored density varies by region band ([map-design-language.md](map-design-language.md), [open-world-navigation.md](open-world-navigation.md)).

Not an implementation ticket. Hard FPS/GPU gates remain [TICKET-0139](../planning/tickets/TICKET-0139.md). Instance camp enter/exit is [DEC-0033](../decisions/index.md#dec-0033-anywhere-player-camp-as-editable-instance-dao-style).

## Current engine baselines (shipped)

| Layer | Cell / neighborhood | Notes |
| --- | --- | --- |
| World partition | **128 m** cells | Ownership, placement, nav grids ([roadmap.md](../roadmap.md)) |
| Terrain / foliage paint | **40 m** cells, 33×33 samples | Stream radius **2** → camera-centered **5×5** resident terrain tiles ([debug-world.md](debug-world.md)) |
| Navigation field | 128 m grids | Streamed with focus; queries fail if cell not resident ([navigation-grid.md](navigation-grid.md)) |
| Collision | Streamed bodies per cell | Unload with cells; character not owned by a streamed cell |
| Stress check | 16 km² walk | Resident terrain cells stay **bounded** (suite / debug-world) |

LOD today is primarily **distance fog + unload** (and foliage impostor intent in visual direction). Multi-tier mesh LOD and GPU budget meters are follow-on engineering.

## What “authored region” means here

A World Forge **region** (hub apron, wilderness weave, chaotic pocket, etc.) is a **density + soft-gate intent** overlay — not a separate streamed “level.” Partition/terrain cells still stream by camera/player focus. Regions must be authorable so that:

1. Hub spikes do not force the whole 4×4 km into memory.
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

**Authoring rule:** Prefer clustering content inside a small set of partition cells around anchors. Do not carpet the 16 km² with hub-density prefabs.

## Resident-set policy (acceptance targets)

These are product acceptance targets for later engineering tickets (numbers may be tuned under TICKET-0139):

### Overland (default play)

1. **Terrain/foliage resident set** stays within a fixed camera/player neighborhood (today: radius 2 / 5×5 of 40 m cells). Expanding radius requires an explicit budget ticket, not silent growth.
2. **Collision + nav** resident sets track the same focus; queries must fail closed with diagnostics when a cell is missing (already true for nav).
3. **Placement/prefab bodies** unload with their owner cells; no permanent residency for distant hubs.
4. **Point lights:** keep a small active set (debug world already favors nearest placed lights). Hub cores may author more lights, but only nearest N affect shading.
5. After **fast travel** ([DEC-0032](../decisions/index.md#dec-0032-open-world-travel-discovery-map-and-dual-soft-gates)), the destination neighborhood becomes the new focus; previous cells unload within a bounded number of frames/ticks (no dual-hub residency).

### Instances (camp, dungeon, vision)

1. Entering an instance may **unload or freeze** overland streaming pressure; camp is a bounded scene ([DEC-0033](../decisions/index.md#dec-0033-anywhere-player-camp-as-editable-instance-dao-style)).
2. Exit restores overland focus at the pitch/return anchor and rebuilds the neighborhood without retaining instance meshes.
3. Nested instances are denied (already: no camp while inside another instance).

## LOD ladder (design intent)

Until mesh LOD pipelines exist, accept this ladder:

| Distance | Presentation |
| --- | --- |
| Near (resident cells) | Full terrain, foliage instances, collision, interactables |
| Mid (edge of stream / just unloaded) | Pop via stream load/unload; fog softens seams |
| Far | Silhouette landmarks + fog; no collision/sim |
| Map UI | Fog-of-war / discovery dust — not a 3D LOD tier ([DEC-0032](../decisions/index.md#dec-0032-open-world-travel-discovery-map-and-dual-soft-gates)) |

Future mesh LOD / impostors must preserve **value separation** and combat readability ([visual-direction.md](../art/visual-direction.md)).

## Budget categories (for TICKET-0139 + content review)

Acceptance reviews for authored regions should report against these categories (metrics TBD at 1440p/60):

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

## Out of scope (this ticket)

- Implementing new stream radii, mesh LOD, or GPU profilers
- Locking numeric FPS budgets (TICKET-0139)
- Mini-map rendering (TICKET-0061)
- Recast/detour (TICKET-0109)
- Final region layouts / art production

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
