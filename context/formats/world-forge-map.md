# World Forge Map (`map.worldforge.json`)

Status: active (schemaVersion 1) — TICKET-0013 · Epic EPIC-0002

Diffable story geography: **regions**, **POIs**, travel/soft-gate **links**, **hydrology** planning bounds, **ferry route** polylines, and land **travel routes** (track/road/highway). IDs and narrative metadata only — not mesh placement ([DEC-0019](../decisions/index.md#dec-0019-world-forge-editor-home-and-story-canon-split), [DEC-0038](../decisions/index.md#dec-0039-water-swim-and-hydrology-authoring)). Scene entity / prefab refs are optional strings for later wiring. Cartography presentation: [cartography-design.md](../art/cartography-design.md).

## Default path

`assets/world-forge/map.worldforge.json`

Helper: `default_world_forge_map_path(project_root)`.

Sample: `samples/open-world-rpg/assets/world-forge/map.worldforge.json`.

## Shape

```json
{
  "schemaVersion": 1,
  "id": "tessera_map",
  "cartographyPlate": {
    "centerX": 0,
    "centerZ": 0,
    "widthMeters": 4000,
    "heightMeters": 2250
  },
  "regions": [
    {
      "id": "calrenoth",
      "kind": "fortress",
      "displayName": "Calrenoth",
      "canonStatus": "draft",
      "summary": "...",
      "storyRef": "...",
      "parentRegionId": "tessera_overland",
      "factionIds": ["kingdom_tessera"],
      "tags": ["act0"],
      "softGate": { "enabled": true, "notes": "..." },
      "openQuestions": []
    }
  ],
  "pois": [
    {
      "id": "calrenoth_drawbridge",
      "kind": "gate",
      "displayName": "Calrenoth Drawbridge",
      "canonStatus": "draft",
      "regionId": "calrenoth",
      "summary": "...",
      "storyRef": "...",
      "sceneEntityId": "",
      "prefabId": "",
      "tags": [],
      "openQuestions": []
    }
  ],
  "links": [
    {
      "id": "calrenoth_soft_to_overland",
      "kind": "soft_gate",
      "fromKind": "region",
      "fromId": "calrenoth",
      "toKind": "region",
      "toId": "tessera_overland",
      "canonStatus": "draft",
      "bidirectional": false,
      "softGate": { "enabled": true, "notes": "..." },
      "summary": "...",
      "storyRef": "...",
      "openQuestions": []
    }
  ]
}
```

Optional `anchor: { "x", "y", "z" }` on regions/POIs when a world-space hint exists. The World Forge Map **Canvas** (TICKET-0187) authors these anchors on an XZ top-down overlay; Scene/Sculpt still own mesh placement.

### `cartographyPlate` (optional)

Locks Cartography’s official Tessera backdrop to a fixed world-meter AABB instead of fitting around marker content. Typical v1 values match the playable **4×4 km** slice (`widthMeters: 4000`); `heightMeters` follows map aspect (often 16:9 under the framed stage).

| Field | Type | Notes |
| --- | --- | --- |
| `centerX`, `centerZ` | number | Plate center in world XZ |
| `widthMeters`, `heightMeters` | number | Positive finite extents (meters) |

When absent, Cartography keeps fit-to-content behavior. Map Canvas can **Apply plate + rescale** to write this field and uniformly scale anchors/borders/hydrology/routes about the plate center.

Optional `border: [{ "x", "z" }, …]` on regions — political / region outline polyline (open or closed) for Cartography mode.

### `hydrologyRegions[]`

| Field | Type | Notes |
| --- | --- | --- |
| `id` | string | Unique slug |
| `kind` | enum | `lake` \| `river` \| `sea` |
| `minX`, `maxX`, `minZ`, `maxZ` | number | World-space XZ bounding box |
| `acts` | string[] | Optional act lens (`act0`..`act4`); empty = campaign-wide |
| `summary` | string | Author notes |

### `ferryRoutes[]`

| Field | Type | Notes |
| --- | --- | --- |
| `id` | string | Unique slug |
| `fromPoiId`, `toPoiId` | string | Must reference POI ids in `pois[]` |
| `points` | `{x,z}[]` | Optional world XZ polyline between docks |
| `acts` | string[] | Optional act lens |
| `summary` | string | Author notes |

### `travelRoutes[]`

Land travel geometry (narrative adjacency stays in `links[]`).

| Field | Type | Notes |
| --- | --- | --- |
| `id` | string | Unique slug |
| `kind` | enum | `track` \| `road` \| `highway` |
| `fromPoiId`, `toPoiId` | string | Optional POI refs (validated when non-empty) |
| `points` | `{x,z}[]` | World XZ polyline |
| `acts` | string[] | Optional act lens |
| `summary` | string | Author notes |

## Editor Map Canvas

- **List** — Regions / POIs / Links / **Hydrology** / **Ferry** / **Travel** inspectors (anchors editable as xyz; hydrology bounds as min/max XZ; ferry/travel routes as POI refs + point list; region borders as point list).
- **Canvas** — pan/zoom XZ view (DEC-0027 camera), place/drag markers, draw links between anchored endpoints, **draw hydrology bounding boxes**, **click-append ferry/travel polylines**, optional greyscale terrain underlay (TICKET-0188).
- **View modes** — **Cartography** (parchment chrome, kind icons, borders, road grades, culture labels) vs **Top-down** (terrain underlay for aligning the same XZ to the playable world). Official Tessera map is a campaign backdrop; when `cartographyPlate` is set it is sized to that world-meter window (typically the 4 km slice), not a heightmap.
- **Stroke presentation** — Cartography stamps transparent stroke tiles along `region.border`, `travelRoutes`, and `ferryRoutes` (`assets/ui/cartography/strokes/`). Mountain / coast silhouettes stay in discrete plates; political strokes do not paint over sea or across ridges.

## Enums

| Field | Values |
| --- | --- |
| region `kind` | `region` \| `fortress` \| `city` \| `wilderness` \| `chaotic` \| `settlement` \| `other` |
| POI `kind` | `landmark` \| `settlement` \| `gate` \| `shrine` \| `camp` \| `other` |
| link `kind` | `travel` \| `soft_gate` \| `story_gate` \| `adjacency` |
| link `fromKind` / `toKind` | `region` \| `poi` |
| hydrology `kind` | `lake` \| `river` \| `sea` |
| travel route `kind` | `track` \| `road` \| `highway` |
| `canonStatus` | `established` \| `draft` \| `proposal` \| `open` |

## Validation

- `schemaVersion` must be `1`
- Unique non-empty region / POI / link ids
- Unique non-empty hydrology / ferry / travel route ids
- `parentRegionId` empty or another region id (no self)
- Every POI `regionId` must exist
- Link endpoints resolve to region/POI by `fromKind` / `toKind`; no self-loops
- Ferry `fromPoiId` / `toPoiId` must reference POI ids; distinct endpoints
- Travel route optional POI refs must exist when set; distinct when both set
- Hydrology / ferry / travel `acts` use the same act tokens as regions/POIs
- When factions file is present, region `factionIds` must match faction entity ids

Error codes: `WORLD-FORGE-MAP-*` (see `WorldForgeMapAsset`).

Project `validate` loads the default path when present and cross-checks faction ids.

## Non-goals

- Do **not** invent city/town names or precise map titles still marked open in story.
- No mesh placement (Scene/MCP).
- No player HUD mini-map rendering (EPIC-0007) — editor Map Canvas is authoring-only (consumes cartography kit later via TICKET-0061).
- MCP mutate: use `engine_world_forge_apply` (TICKET-0014).

## Related

- Cartography language: [`../art/cartography-design.md`](../art/cartography-design.md)
- Act lens: [`world-forge-acts.md`](world-forge-acts.md) (DEC-0036 / TICKET-0189)
- Scene marker overlay: TICKET-0190 (editor-only poles in Scene/Sculpt)
- Editor canvas: TICKET-0187 / TICKET-0188
- [`../features/world-forge-scope.md`](../features/world-forge-scope.md)
- [`world-forge-factions.md`](world-forge-factions.md)
- Header: `include/engine/assets/world_forge_map_asset.h`
