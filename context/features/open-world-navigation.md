# Open-World Navigation Design

Status: active (TICKET-0030) — design notes for traversal in the seamless 4×4 km world ([DEC-0001](../decisions/index.md#dec-0001-product-and-platform-target), [DEC-0021](../decisions/index.md#dec-0021-soft-gates-with-rare-optional-instances)).

Not an implementation ticket. Recast/detour remains deferred ([TICKET-0109](../planning/epics.md)). Pair with [map-design-language.md](map-design-language.md) (TICKET-0031).

## Intended travel loop

| Phase | Player experience | Engine / content role |
| --- | --- | --- |
| On foot (default) | Continuous overland walk/run across streamed terrain | Character controller + streamed collision; navigation grid for AI / assist queries |
| Guided paths | Roads and trails bias exploration toward hubs, story beats, and readable landmarks | Authored path ribbons + POI chain; not a separate movement mode |
| Soft-gated frontier | Dangerous or late-story regions are reachable in world space but blocked by story/quest pressure until unlocked | World Forge `soft_gate` / `story_gate` links + quest flags ([world-forge-map.md](../formats/world-forge-map.md)) |
| Fast travel (unlocked) | Jump between discovered safe anchors without replaying roads | Discrete teleport to authored FT points; short fade OK; not a chapter load |
| Player camp | Pitch camp from overland → party management space | Camp **instance** ([DEC-0033](../decisions/index.md#dec-0033-anywhere-player-camp-as-editable-instance-dao-style)); exit to pitch point |
| Rare instances | Dungeons, siege density pockets, vision spaces | Optional instance handoff ([DEC-0021](../decisions/index.md#dec-0021-soft-gates-with-rare-optional-instances)); exit returns to world anchor |

**Loop summary:** discover on foot → evergreen camp tutorial → find first hub → open surrounding soft gates → use roads for readability → camp anywhere for party prep → carriage FT between discovered posts → reserve other instances for isolation/density.

Campaign acts are narrative arcs on this loop ([campaign-beat-sheet.md](../story/campaign-beat-sheet.md)), not separate loaded maps.

## Relationship to the navigation grid

Shipped M4 grid ([navigation-grid.md](navigation-grid.md)):

- 128 m partition cells, 4 m walkability samples, slope ≤ 0.45 traversable
- Streams with camera focus; `nearest_walkable_point` / `line_of_walk` on resident cells
- Height from the same analytic terrain function as collision — not mesh nav

**v1 navigation policy:**

| Concern | Owner | Notes |
| --- | --- | --- |
| Player locomotion | Character controller + collision | Grid is not the player’s pathfinder |
| AI / companion assist / “is this walkable?” | Navigation grid | Prefer grid queries over ad-hoc ray hacks |
| Roads | Content + visual language | May later bias AI cost or FT discovery; not required for player move |
| Soft / story gates | World Forge + quest runtime | Logic gates; do not carve the heightfield |
| Hard barriers | Terrain slope, collision volumes, water/cliffs | Physics / authored colliders; **deep water** via swim fatigue + damage ([DEC-0038](../decisions/index.md#dec-0038-water-swim-and-hydrology-authoring)) |

Grid limitations that design must accept until TICKET-0109: no character radius carving, no dynamic obstacles, queries fail for unloaded cells.

## Roads and paths

**Role:** readability and pacing, not a mandatory rail.

- Primary roads connect hubs, keeps, and major landmarks at a glance (value + silhouette; see map design language).
- Secondary trails lead to side content, camps, and shrines without promising safety.
- Roads should usually sit on walkable slope bands so the grid and capsule agree with the visual path.
- Do not require unique road collision; prefer terrain + light props (ruts, markers, lantern posts).

## Hard barriers vs soft gates

| Kind | Player feels | Typical tools | Example |
| --- | --- | --- | --- |
| Hard barrier | Cannot pass without a different route or ability | Steep slope (> walkability), cliffs, walls, locked collision, **deep water** (swim fatigue → damage) | Mountain wall around a high pass; ocean/deep lake without shore escape |
| Soft gate (checkpoint) | World exists beyond; systems refuse or warn | Dialogue, guards, quest flag, border crossing | Act boundary roadblock |
| Soft gate (hostile frontier) | You can walk in; survival is the gate | Extreme enemies, disease/affliction, item/key requirements | Chaos hinterland |
| Story gate | Narrative-critical lock (often one-way or beat-tied) | Quest objective + link `kind: story_gate` | Act 0 tutorial corridor exit |
| Instance portal | Leaves overland briefly | Instance entry POI | Realm of Darkness vision |

**Rules ([DEC-0032](../decisions/index.md#dec-0032-open-world-travel-discovery-map-and-dual-soft-gates)):**

1. Prefer soft gates over hard walls for campaign pacing ([DEC-0021](../decisions/index.md#dec-0021-soft-gates-with-rare-optional-instances)).
2. Hard barriers exist for geography and combat readability — never as the only way to enforce act structure across the whole map.
3. Soft-gate denial is **dual-mode by region/link tag**: checkpoint dialogue **or** hostile frontier (lethality / affliction / item gates). No silent invisible walls.
4. Hostile frontiers still live on the seamless map when possible.

## Fast-travel policy ([DEC-0032](../decisions/index.md#dec-0032-open-world-travel-discovery-map-and-dual-soft-gates))

1. **System:** Always available as a feature once carriage travel exists — not a late skill unlock. Act 0 tutorial corridor does not expose overland FT.
2. **Anchors:** Only **discovered tavern / carriage-post** POIs. No wilderness FT without a post.
3. **Cost:** Spend **gold** at the post (immersion) to travel to other discovered towns/POIs.
4. **Map UI:** Player map shows fog-of-war on unseen areas and a dust/reveal as areas are discovered; FT picks from known posts on that map.
5. **Safety:** Do not complete FT into active enemy combat volumes.
6. **Blocked states:** Deny while in combat, falling, rare instances, or story-flag locked.
7. **Presentation:** Short fade / stream destination cells OK; avoid chapter-load UX.

**Mounts (near-term):** horses only if any; account for player + up to three companions later. Boats/other vehicles deferred as **travel modes**, but **scripted ferries/ships that float on water** are in scope ([DEC-0038](../decisions/index.md#dec-0038-water-swim-and-hydrology-authoring)).

## Water and swim ([DEC-0038](../decisions/index.md#dec-0038-water-swim-and-hydrology-authoring))

Gameplay water — not decorative-only. See [`water-hydrology.md`](water-hydrology.md).

| Rule | Detail |
| --- | --- |
| Sea level | One world-wide constant; terrain sculpt sets land height relative to it |
| Authoring | Sculpt places water surfaces; World Forge Map plans rivers/lakes/seas and ferry routes |
| Swim | Character controller swim mode when in water |
| Shallow | Wading or low-cost swim (depth band TBD) |
| Deep | Sustained swim drains fatigue; exhaustion causes damage over time — hard barrier for ocean/deep lakes |
| Vessels | Scripted motion; hulls float on water surface (SQ-10 ferry reference) |
| Open sea | Bounded authored regions; map-edge fog-of-war beyond |
| Dry basins | No auto-fill unless terrain + placement justify it |
| Nav grid | Underwater / deep samples unwalkable for AI assist |
| Foliage | Suppressed underwater |

## Player camp ([DEC-0033](../decisions/index.md#dec-0033-anywhere-player-camp-as-editable-instance-dao-style))

DAO-style party camp: a persistent, editable camp **instance** entered from the open world.

| Rule | Detail |
| --- | --- |
| Tutorial | Act 1 O’hlundian evergreens — story-tied teach: setup, talk to camp NPCs/companions, leave camp |
| Anywhere pitch | After unlock, player may set up camp from overland (not only authored POIs) |
| Persistence | Same camp “home” (layout edits, companion staging, camp services) across pitches; world entry point changes |
| Not a hub | Towns still own vendors/quests/carriage FT; camp is party/rest/management |
| Deny (hard) | **No combat escape:** refuse pitch in an active combat situation/zone/encounter so camp cannot negate fight mechanics ([DEC-0033](../decisions/index.md#dec-0033-anywhere-player-camp-as-editable-instance-dao-style)) |
| Deny (also) | Already inside another instance |

Quiet overland (even dangerous areas) may allow camp when **not** in an active fight. Optional later tags may still block interiors / story-locked beats — see open-questions.

## Mini-map and World Forge handoff

- Region / POI / link IDs live in `map.worldforge.json` ([world-forge-map.md](../formats/world-forge-map.md)). Carriage/tavern posts should be discoverable POIs (`kind` / tags TBD at schema time).
- Soft-gate links need a denial-mode tag (`checkpoint` vs `hostile_frontier` or equivalent) when schema is extended.
- Mini-map (TICKET-0061) inherits fog + discovery dust UX from DEC-0032.
- Streaming/LOD acceptance for authored regions: [`streaming-lod-budgets.md`](streaming-lod-budgets.md) (TICKET-0032).

## Out of scope (v1 / this ticket)

- Recast/detour navmesh (TICKET-0109)
- New streaming or partition systems
- Mini-map UI implementation (TICKET-0061)
- Final region layout authorship
- Boats / non-horse mounts
- Auto pathfinding “click to walk” for the player

## Acceptance criteria for later engineering tickets

Use these as seeds when promoting implementation work:

- [ ] Soft-gate evaluation supports dual denial modes (checkpoint dialogue vs hostile frontier pressure) from World Forge tags + quest flags.
- [ ] Carriage-post FT is discovery-gated, gold-costed, and lands on a walkable point near the destination post.
- [ ] Player map fog / discovery reveal feeds FT destination list.
- [ ] FT denied in combat / instance / blocked-flag states with structured diagnostics for agents.
- [ ] Roads are optional content; player locomotion never requires a road mesh.
- [ ] Navigation grid remains the AI/assist walkability source until TICKET-0109 replaces or wraps it.
- [ ] Instance enter/exit preserves a world return anchor (DEC-0021).
- [ ] Design regression: Act 0 can remain a soft-gated corridor without loading a separate “chapter world.”
- [ ] Act 1 wake places the player in O’hlundian evergreens; first village is found by exploration.
- [ ] Camp enter/exit instance with persistent layout; evergreen tutorial beat teaches setup + camp NPC talk; deny pitch in active combat situation/zone (no fight escape).

## Related

- Map language: [map-design-language.md](map-design-language.md)
- Navigation grid: [navigation-grid.md](navigation-grid.md)
- Character controller: [character-controller.md](character-controller.md)
- Story gating: [campaign-beat-sheet.md](../story/campaign-beat-sheet.md)
- Ticket: [TICKET-0030](../planning/tickets/TICKET-0030.md)
