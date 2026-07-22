# Official World Map (Tessera)

Status: **established** — owner-designated official game map (imported 2026-07-19; clean backdrop + multi-LOD tiles 2026-07-20).

Asset: [`official-world-map.png`](official-world-map.png) (overview) · tiles: [`../art/cartography/world-map-tiles/`](../art/cartography/world-map-tiles/)

Related: [DEC-0034](../decisions/index.md#dec-0034-tessera-is-the-worlds-primary-land) · [story-vision.md](story-vision.md) · [map-design-language.md](../features/map-design-language.md) · [cartography-design.md](../art/cartography-design.md) · [world-forge-map.md](../formats/world-forge-map.md)

## Role

This illustration is the **canonical overworld geography** for Tessera (the primary land) and a **huge five-act campaign reference** — not a 1:1 heightmap and not the World Forge Map Canvas underlay for the v1 seamless **4×4 km** playable slice. The slice is authored *inside* Tessera and should eventually be located as a marked window on this map ([DEC-0034](../decisions/index.md#dec-0034-tessera-is-the-worlds-primary-land)).

Use it for story placement, faction theaters, act pacing, World Forge region planning, and travel fantasy. Map Canvas **Cartography** mode can show this PNG as a pan/zoom **backdrop** under markers. When `cartographyPlate` is authored on `map.worldforge.json` (typically **4 km** wide for the v1 slice), the backdrop locks to that world-meter window; otherwise it aspect-fits around authored content. It is still **not** a geo-locked heightmap. **Top-down** mode still uses the terrain underlay for XZ alignment with the playable slice.

## Layout (unlabeled)

Place names are **not** on the art; do not invent region titles here until the owner names them.

| Mass | Rough position | Notable features on the art |
| --- | --- | --- |
| Northern / western C-shaped continent | Wraps north and west of the interior sea | Western mountain spine; northern forested highlands; rivers into the interior sea |
| Southern continent | South of the interior sea | Diagonal mountain range; forests west/central-north; more open center-south; **reddish / scorched southeastern coast** (chaos/corruption candidate) |
| Central island | Middle of the interior sea | Hills/mountains, forest wash |
| Northeastern island | Top-right ocean | Central mountains, forest wash |

Backdrop art is **geography only** (coasts, landmass, soft terrain washes). Towns, roads, highways, and landmarks are authored in World Forge Map Canvas — not painted into the PNG.

## Clean backdrop + discrete zoom layers (2026-07-20)

- Overview PNG stays unlabeled continent/ocean art (no settlements/roads).
- Cartography mode draws **discrete zoom plates** (`world-map-layers/manifest.json`) cropped from a continuous 4096-wide master — continent → theaters → local (fog swap on threshold). See [DEC-0040](../decisions/index.md#dec-0040-discrete-cartography-zoom-layers-with-fog-and-frame).
- Ornate parchment **frame** and **fog veil** are separate chrome under `cartography/frame/` and `cartography/fog/`.
- Rebuild plates with `python tools/build_world_map_layers.py`. Legacy tiled LOD (`world-map-tiles/`) is fallback only.
- Continuous master rebuild (no AI quadrant inject): `python tools/build_world_map_tiles.py`.

## Authoring rules

1. Prefer this map over ad-hoc continent sketches for Tessera-scale geography.
2. Keep the **Kingdom of Tessera** as one polity *on* this land — not the whole map ([DEC-0034](../decisions/index.md#dec-0034-tessera-is-the-worlds-primary-land)).
3. World Forge regions/POIs and the 4×4 km sample should cite or overlay this map when geography is locked; meshes stay Scene/Sculpt-owned.
4. Labeling, faction color overlays, and playable-slice footprint remain **open**; **draft theater placement** from 2026-07-20 is recorded below and may guide Cartography callouts without locking borders.
5. Cartography may use this PNG as a **campaign backdrop** (`World map` checkbox). Prefer an authored `cartographyPlate` sized to the v1 **4×4 km** slice when aligning marker distances to playable meters. Do **not** treat that backdrop as the slice heightmap — Top-down terrain underlay remains the alignment source for sculpt/terrain.

## Draft faction theaters (2026-07-20)

Status: **draft** — owner + Dom world-design session. Rough theaters on this art for story/cartography planning; **not** locked borders, place names, or the v1 4×4 km footprint. See also [factions.md](factions.md#design-session-2026-07-20-faction-structure--theaters-draft).

| Theater / site | Rough placement on the official map | Notes |
| --- | --- | --- |
| The Cristallo | **Central island** in the interior sea | Seat tied to crater / world-origination (Pangea-break allegory); theology open |
| Kingdom of Tessera | **Western** landmass / theater | Dominant human polity planted west — not the whole continent |
| Chaotic Imperium | **Southern** theater | Pressure from the south; reddish/scorched southeast remains a chaos candidate, not locked as Imperium-only |
| Calrenoth (Act 0 — **Landfall**) | **Western peninsula tip** (confirmed) facing Imperium pressure from the **south** | Tessera-built landing / fortress **outside** the western core; **landlocked** player approach + **moat-scale drawbridge** to another land spur (not a long sea bridge) |
| Arrotrebae presence (Act 0 vicinity) | Hugging the **south of that peninsula** | **The Thalassar** (`thalassar`) — regional clan under Arrotrebae umbrella, not a painted kingdom border |
| Minor orc warband (Act 0 vicinity) | Near **mountains** inland / east of the peninsula corridor | **The Underflow** (`underflow`) — minor warband; subsurface-water holds; same false **Sea of Whispers** cult as Thalassar |
| **Ledgeport** (Act 1 hub) | Nearby coastal marker on the Act 0 retreat path from Calrenoth — **Act 1 focus region** (confirmed) | Market **free town** (`ledgeport`) — mayor/market politics; merchant guild + undermarket; **neutral**; ferry toward Cristallo (wiring open); A1-05 coastal politics contact — named evergreen wake deprecated as competing geography |
| Coastal Arrotrebae tribe (Act 1) | Coastal / seafarer presence near that trade port and southwest theater | **The Thalassar** — same clan as Act 0 south-of-peninsula marker; soft heraldry callout via `heraldry-thalassar.png` |

**Authoring rules for this draft:**

1. Show theaters as soft influence / heraldry callouts — **not** hard political border strokes until borders are authored.
2. Do **not** invent tribe, warband, or port names on the map art or in World Forge seeds until Dom/owner names them — **exceptions:** **The Thalassar** (`thalassar`), **The Underflow** (`underflow`) ([factions.md](factions.md#the-thalassar), [factions.md](factions.md#the-underflow)).
3. Prioritize Act 0 corridor (Calrenoth + neighbors) before filling distant theaters; Act 1 coastal tribe + neutral port are the next planning priority after that corridor.
4. Arrotrebae and orc markers are **regional culture presence**, not single-capital kingdoms. **The Thalassar** may use the **Arrotrebae umbrella emblem** until a tribe shield exists.
5. Transcript spellings (“Aerotropy”, “Atrobia”, “Karanoth”) map to **Arrotrebae** / **Calrenoth** — do not paint alternate labels.

## Open

- Named continents / seas / major cities on this art
- Exact coordinates for Calrenoth, the peninsula tip, mountain warband, **The Thalassar** presence, and **Ledgeport**
- ~~Where O’hlundian evergreens (wake) sit relative to Calrenoth / Ledgeport~~ — named evergreen wake deprecated as Act 1 geography competitor; DEC-0032 camp/wake reconcile still open
- Exact footprint of the v1 4×4 km slice on this map
- Whether the reddish southeast is canonically Imperium heartland, corrupted fringe, or other
- Locked political borders; ferry wiring from **Ledgeport** to Cristallo (**Ledgeport** name confirmed + **The Thalassar** + **The Underflow** + Calrenoth tip locked)
