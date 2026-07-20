# Map Design Language

Status: active (TICKET-0031) — biomes, landmarks, and density guidance for Tessera’s dark-fantasy 4×4 km world. Aligns with [visual-direction.md](../art/visual-direction.md) and [story-vision.md](../story/story-vision.md). Pair with [open-world-navigation.md](open-world-navigation.md) (TICKET-0030).

Authoring later maps these concepts onto World Forge region/POI IDs ([world-forge-map.md](../formats/world-forge-map.md)) — this doc does not invent schema.

## Design goals

1. **Readable at distance** — silhouettes and value separate hubs, threats, and traversal edges.
2. **Muted earth default** — saturated color reserved for corruption, magic, and gameplay feedback.
3. **Sparse-but-intentional density** — 16 km² should feel large; content clusters around landmarks, not uniform noise.
4. **Practical production** — coarse stylized forms, reusable biome kits, procedural scatter OK ([visual-direction.md](../art/visual-direction.md) production constraint).

## Biome / region types

Use a small set of material/atmosphere bands (matches terrain region materials: grass, dirt, rock, snow, corrupted). Map each band to World Forge `region.kind` when authored.

| Biome band | Tone | Typical World Forge `kind` | Gameplay feel |
| --- | --- | --- | --- |
| Farmland / prairie | Worn brown soil, desaturated green, low hedges | `wilderness` / `settlement` fringe | Safe-ish early travel, side farms |
| Temperate woods | Cool charcoal trunks, muted canopy | `wilderness` | Ambush cover, trails, shrines |
| Evergreen / highland forest | Cold green-gray (O’hlundian-adjacent) | `wilderness` | Retreat camps, cooler fog |
| Rocky hills / mountains | Charcoal rock, snow caps optional | `wilderness` | Hard barriers, overlooks, passes |
| Settled lowlands | Timber, warm window light at night | `settlement` / `city` | Hubs, services, FT anchors |
| Fortress / ruin | Iron, ash, broken masonry | `fortress` | Story set pieces (e.g. Calrenoth) |
| Chaotic / corrupted | Saturated wrong-color ground, warped props | `chaotic` | Soft-gated danger, Shroud pressure |

**Rules:**

- Prefer **broad color shapes** over photoreal texture detail.
- Corruption is a **contrast band**, not the default overland look.
- Islands, docks, markets, groves, battlefields remain **POI/location types** inside these bands ([story-vision.md](../story/story-vision.md)), not separate biomes.

## Landmark rules

Landmarks teach navigation without a full mini-map (TICKET-0061 later).

| Rule | Guidance |
| --- | --- |
| Settlement lights | Warm fire / lantern / window light = safety and hub identity at dusk/night |
| Silhouette first | Keeps, towers, mountain teeth, giant dead trees — readable as black cutouts against sky/fog |
| Value separation | Characters, interactables, and path edges stay lighter/darker than surrounding terrain |
| One hero shape per cluster | Each major POI cluster gets one dominant vertical or mass; avoid competing skyline spikes every 100 m |
| Road approach | Primary roads should reveal the landmark before the player arrives (crest, clearing, or bend) |
| Corruption tells | Saturated accent + wrong geometry only on chaotic landmarks — never decorate safe hubs with the same cues |
| Combat clarity | Atmosphere (fog, dusk) must not hide traversal drop-offs or hit timing ([visual-direction.md](../art/visual-direction.md)) |

**Landmark classes** (map to POI `kind` later):

- `settlement` — hubs, villages, markets
- `gate` — bridges, passes, soft-gate thresholds
- `shrine` / spiritual — knowledge / Creotar-adjacent beats
- `camp` — survivor / companion staging
- `landmark` — pure wayfinding props (standing stones, overlook crosses)
- `fortress` regions may host multiple POIs under one silhouette

## Density guidance (4×4 km)

Qualitative bands for a single-player offline RPG. Not hard spawn budgets (those are TICKET-0032).

| Band | Approx. feel | Use |
| --- | --- | --- |
| Hub core | High interactivity in ~200–400 m | Vendors, quests, dialogue, FT anchor |
| Hub apron | Medium — farms, patrols, tutorials | Soft teach combat / traversal |
| Road corridor | Low–medium beads every ~400–800 m | Vista, minor camp, story breadcrumb |
| Wilderness weave | Low — long quiet stretches | Atmosphere, rare ambush, gatherables |
| Landmark bloom | Medium spike around a named POI | Side quests ([side-quest-catalog.md](../story/side-quest-catalog.md)), shrines |
| Chaotic pocket | Medium–high threat, soft-gated | Act 2+ pressure; avoid carpeting the map |

**Rough authoring targets (guidance, not quotas) — [DEC-0032](../decisions/index.md#dec-0032-open-world-travel-discovery-map-and-dual-soft-gates):**

- Major hubs: about **one per campaign act**, placed where the story needs them (not a uniform capital grid).
- Named wilderness landmarks players should remember: on the order of **1 per 1–2 km²** on average, clustered rather than gridded.
- Side-quest hooks: dense near hubs/aprons; sparse in deep wilderness.
- Snow / alpine bands only when climate and story justify them.
- Avoid uniform POI lattices — empty space is a feature for scale and fog.
- Tavern / carriage-post POIs at hubs (and other discoverable towns) support gold fast travel.

## Mapping to World Forge IDs

| Design concept | World Forge field | Notes |
| --- | --- | --- |
| Biome / region band | `regions[].id`, `kind`, `tags`, `summary` | Prefer snake_case ids from display names |
| Soft / story lock | `regions[].softGate`, `links[]` `soft_gate` / `story_gate` | Align with open-world navigation |
| Landmark / POI | `pois[].id`, `kind`, `regionId` | Optional `anchor` when scene placement exists |
| Travel adjacency | `links[]` `travel` / `adjacency` | Roads are narrative + visual; links record intent |
| Discovery / FT | Future runtime flags on POI/region ids | Mini-map TICKET-0061; not authored as mesh |

Do not invent new schema enums here. If a biome needs a tag (`biome:woods`), use `tags[]` until a later schema ticket promotes it.

## Out of scope

- Final art, heightmap sculpting, foliage paint passes
- Streaming / LOD acceptance authored in [`streaming-lod-budgets.md`](streaming-lod-budgets.md) (TICKET-0032); numeric FPS still TICKET-0139
- Mini-map chrome (TICKET-0061)
- Exact faction city layouts (blocked on remaining faction canon gaps)

## Acceptance criteria for later tickets

- [ ] World Forge region kit list covers the biome bands above (or documents intentional omissions).
- [ ] Landmark POIs use silhouette + light rules in art briefs / prefab kits.
- [ ] TICKET-0032 budgets reference these density bands when setting resident-cell / draw limits.
- [ ] Mini-map icons distinguish settlement / gate / shrine / camp / chaos without color-only coding.
- [ ] Side-quest placement reviews check hub apron vs wilderness weave density.

## Related

- Navigation: [open-world-navigation.md](open-world-navigation.md)
- Visual direction: [visual-direction.md](../art/visual-direction.md)
- Fantasy cartography (Map Canvas + overworld reference): [cartography-design.md](../art/cartography-design.md)
- Map format: [world-forge-map.md](../formats/world-forge-map.md)
- Ticket: [TICKET-0031](../planning/tickets/TICKET-0031.md)
