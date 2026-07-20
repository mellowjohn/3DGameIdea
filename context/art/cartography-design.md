# Cartography Design Language

Status: active — fantasy map language for Tessera overworld reference art, World Forge Map Canvas (Cartography mode), and the future player discovery map ([TICKET-0061](../planning/tickets/TICKET-0061.md)).

Aligns with [visual-direction.md](visual-direction.md), [theme-palette.md](theme-palette.md), [map-design-language.md](../features/map-design-language.md), and [official-world-map.md](../story/official-world-map.md). Format fields: [world-forge-map.md](../formats/world-forge-map.md).

## Design goals

1. **Readable fantasy cartography** — parchment, ink coasts, muted watercolor sea; not a tactical HUD chrome.
2. **Same XZ as the playable world** — Map Canvas anchors, borders, and travel routes are the authoritative placement guide for later Scene/Sculpt assets.
3. **Dual authoring modes** — Cartography for story planning; Top-down terrain underlay for lining markers up with the slice.
4. **Culture without color-only cues** — typeface + heraldry + icons; never typeface or tint alone.
5. **Campaign reference vs slice** — the official Tessera map scopes all five acts as a huge reference; the Map Canvas authors the playable window inside that land.

## Aged aesthetic

| Layer | Guidance |
| --- | --- |
| Ground | Warm parchment / vellum (`#c9b896`–`#a8906e`); slight stain variation OK |
| Sea | Cool desaturated watercolor (`#6a7a82`–`#4a5860`); soft shore wash |
| Ink | Charcoal / sepia coasts and labels (`#2a2420`); never pure black neon |
| Accent | Reserved for Nefarium / chaos — [theme-palette.md](theme-palette.md) crimson / vein green |
| Atmosphere | Cool overcast wash; avoid bright fantasy postcard saturation |

Editor chrome stays Roboto on dark ImGui panels (Pencil tokens: chrome `#151719`, panel `#202326`, gold `#D5B978`, text `#F1EEE8`). Cartography mode tints the spatial canvas fill toward parchment; Top-down keeps the terrain greyscale underlay.

### Map Canvas chrome (TICKET-0208)

Authoritative layout exploration: [`../design/world-forge-map-canvas.pen`](../design/world-forge-map-canvas.pen).

| Surface | Guidance |
| --- | --- |
| Shell | Branded header + left nav + workspace; Act lens lives in the nav footer |
| Tools | Select · Anchor · Route · Border · Water (mutually exclusive draw modes) |
| On-map panels | Parchment fill + border overlay (`panel/panel-parchment.png`, `panel-border.png`) for Map Layers legend, heraldry legend, draft badge, hover title chips |
| Labels | Icon markers on the map; **titles on hover** (selected marker may stay labeled); culture faces for short labels only |
| Scale | Ink scale bar on the cartography stage |
| Zoom plates | Discrete layer swap with fog; “return to continent” when viewing a detail plate |

Frames **02 Asset Kit**, **04 Culture Typography**, **05 Chrome & Stickers**, and **06 Stroke Tile Kit** in the `.pen` are design reference — not separate runtime screens.

## Settlement hierarchy (icons)

Distinct silhouettes — not color-only coding:

| Class | Icon cue | Typical World Forge |
| --- | --- | --- |
| Village | Small clustered houses | `settlement` / POI `settlement` |
| Town | Larger cluster + road bead | `settlement` / `city` fringe |
| City | Walls / multi-tower mass | region `city` |
| Fortress | Keep / battlements | region `fortress` |
| Ruin | Broken tower / crumbled wall | tags + `landmark` / `fortress` |

Runtime icons: `samples/open-world-rpg/assets/ui/cartography/icon-*.png` (transparent 128×128; install via `tools/install_cartography_icons.py`).

## Landmark classes (icons)

| Class | Maps to | Cue |
| --- | --- | --- |
| Gate | POI `gate` | Arch / bridge mark |
| Shrine | POI `shrine` | Standing stone / stele |
| Camp | POI `camp` | Tent / fire ring |
| Landmark | POI `landmark` | Pillar / overlook cross |
| Dock | POI `other` + tag `dock` (or ferry endpoint) | Pier / anchor |

Runtime icons: same `icon-*.png` set (gate / shrine / camp / landmark / dock).

## Heraldry

One emblem per faction **sphere** (draft-safe; do not invent place names on shields).

**Umbrella vs polity:** Kingdom of Tessera, Chaotic Imperium, and The Cristallo are territorial / political spheres for map callouts. **Arrotrebae** and **orc warbands** are **umbrella cultures** of regional tribes / warbands — use the shared emblem for the sphere until named sub-groups get their own shields ([factions.md](../story/factions.md#design-session-2026-07-20-faction-structure--theaters-draft)). Do not paint them as single contiguous kingdom borders.

| Faction | Emblem tone | File |
| --- | --- | --- |
| Kingdom of Tessera | Heroic medieval — crown / tree / sunburst (restrained) | `heraldry-kingdom_tessera.png` |
| Chaotic Imperium | Dark crusade / fractured-creator — cracked oxblood field, spiked halo + jagged starburst (Roman eagle / crown retired) | `heraldry-chaotic_imperium.png` |
| The Cristallo | Refined crystal / liturgical star | `heraldry-cristallo.png` |
| Arrotrebae | Woodland antler / leaf spiral — umbrella until tribes are named | `heraldry-arrotrebae.png` |
| The Thalassar | Coastal seafarer — teal field, bronze wave spiral + trident, shell knot at tip | `heraldry-thalassar.png` |
| Orc warbands | Shared heavy fang / tusks until warbands are named | `heraldry-orc_warbands.png` |
| The Underflow | Ridge-orc warband — spiked iron border, tusks framing underground black-water cleft into sinkhole vortex | `heraldry-underflow.png` |

Faction asset fields `emblemPath` / `mapColor` / `mapTypefaceId` resolve these at edit time ([world-forge-factions.md](../formats/world-forge-factions.md)).

**Theater callouts:** Cartography may show soft heraldry chips for draft theaters (west Tessera, central Cristallo, south Imperium) without hard political border strokes — see [official-world-map.md](../story/official-world-map.md#draft-faction-theaters-2026-07-20).

**Coastal Arrotrebae / Ledgeport (draft 2026-07-20):** soft coastal-presence callouts near **Ledgeport** may use **The Thalassar** chip (`heraldry-thalassar.png`) when the callout is that clan; otherwise the Arrotrebae umbrella emblem. Mark Ledgeport with a dock/settlement icon and **no** faction heraldry ownership chip (neutral = neither Arrotrebae nor Cristallo). Ferry geometry toward Cristallo uses existing ferry stroke language.

## Culture typography (map labels)

Separate from engine chrome / in-scene HUD rules in [visual-direction.md](visual-direction.md).

| Role | Face | Use |
| --- | --- | --- |
| Editor chrome | Roboto (OFL) | World Forge panels, toolbars |
| Default / Kingdom of Tessera | Cinzel (OFL) | Place names when faction is Tessera or unset; future player map chrome |
| Chaotic Imperium | Cinzel Decorative or Trajan-like OFL substitute packaged as Imperium map face | Short imperial labels |
| Cristallo | High-serif liturgical OFL display | Oligarchic / temple labels |
| Arrotrebae | Uncial / organic OFL display | Woodland labels |
| Orc warbands | Heavy angular OFL display | Shared warband labels until named |
| Ancient / draconian | MedievalSharp (OFL) | Short established ancient-site labels and inscriptions |

**Rules:**

- Culture face applies when the region/POI primary `factionIds[0]` (or faction `mapTypefaceId`) matches; otherwise Cinzel.
- Ancient / draconian face applies only when an authored place is explicitly classified as ancient or draconian; it does not infer canon from a decorative style.
- Never communicate culture by typeface alone — pair with heraldry chip and/or settlement icon.
- Ornamental faces only for short map labels; gameplay body text stays Cinzel/Roboto.
- Package only SIL OFL 1.1 (or equally permissive) fonts; record in [resources/index.md](../resources/index.md).

## Travel network

Narrative `links[]` (`travel` / soft_gate / story_gate / adjacency) stay story adjacency. **Geometry** lives in `travelRoutes[]` (and existing `ferryRoutes[]`).

| Grade | Stroke | Use |
| --- | --- | --- |
| Track / path | Thin dashed ink tile (`stroke-track.png`) | Wilderness trails |
| Road | Solid single stroke tile (`stroke-road.png`) | Secondary settlement routes |
| Highway | Double stroke + mile-post tile (`stroke-highway.png`) | Primary corridors / carriage routes |
| Ferry | Water-dashed tile (`stroke-ferry.png`) | Dock-to-dock polylines |

Stroke samples: `context/art/cartography/travel-strokes-reference.png`. Runtime tiles: `samples/open-world-rpg/assets/ui/cartography/strokes/`.

## Borders

| Kind | Source | Presentation |
| --- | --- | --- |
| Natural | Coasts and mountain spines in discrete map plates; rivers/lakes/seas (`hydrologyRegions`) | Plate art / hydrology fill — **not** stroke overlays (mountains stay in the plate) |
| Political | `region.border` `{x,z}[]` polylines | Image-stamped dashed oxblood tile (`stroke-political-border.png`), tinted by primary faction `mapColor`; always pair with heraldry / label |

Border stroke samples: `context/art/cartography/border-strokes-reference.png`.

### Image-stamp stroke rendering (Map Canvas)

Authored XZ polylines remain the placement geometry. Cartography mode stamps transparent RGBA tiles along those paths (`engine::build_cartography_stroke_stamps` / `CartographyStrokeStyle`). Top-down falls back to immediate-mode lines when tiles are unavailable. River hydrology boxes may draw a centerline river stamp when the `stroke-river` tile is loaded.

Generate / refresh tiles with `tools/generate_cartography_strokes.py`. Provenance: `context/art/cartography/strokes/PROVENANCE.md`.

## Scale layers

| Layer | Role |
| --- | --- |
| Official Tessera world map | Huge five-act **geography reference** (no painted towns/roads); discrete zoom plates + fog/frame for Cartography |
| Map Canvas Top-down | Terrain underlay + markers + travel polylines — prove XZ = world |
| Map Canvas Cartography | Same data + discrete official-map plates + fog swap + **16:9 letterboxed stage** + ornate frame (no stretch) + icons/heraldry; zoom clamped ~0.2–2.5 with world map |
| Player map (TICKET-0061) | Same icon / type / road / plate / frame language + discovery fog later |

## World-placement bridge

Anchors and travel route points are world XZ. Cartography is readable planning; Top-down verifies against terrain. Later Scene/Sculpt placement consumes these as “put the asset here.” This language does not spawn meshes.

## Related assets

- Concept / reference: `context/art/cartography/`
- Runtime UI: `samples/open-world-rpg/assets/ui/cartography/`
- Location panel chrome: `panel/panel-parchment.png` + `panel/panel-border(-wide).png` (aged nameplates)
- Fonts: `assets/ui/fonts/` (Cinzel + culture map faces)
