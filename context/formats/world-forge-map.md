# World Forge Map (`map.worldforge.json`)

Status: active (schemaVersion 1) — TICKET-0013 · Epic EPIC-0002

Diffable story geography: **regions**, **POIs**, travel/soft-gate **links**, **hydrology** planning bounds, and **ferry route** polylines. IDs and narrative metadata only — not mesh placement ([DEC-0019](../decisions/index.md#dec-0019-world-forge-editor-home-and-story-canon-split), [DEC-0038](../decisions/index.md#dec-0038-water-swim-and-hydrology-authoring)). Scene entity / prefab refs are optional strings for later wiring.

## Default path

`assets/world-forge/map.worldforge.json`

Helper: `default_world_forge_map_path(project_root)`.

Sample: `samples/open-world-rpg/assets/world-forge/map.worldforge.json`.

## Shape

```json
{
  "schemaVersion": 1,
  "id": "tessera_map",
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

## Editor Map Canvas

- **List** — Regions / POIs / Links / **Hydrology** / **Ferry** inspectors (anchors editable as xyz; hydrology bounds as min/max XZ; ferry routes as POI refs + point list).
- **Canvas** — pan/zoom XZ view (DEC-0027 camera), place/drag markers, draw links between anchored endpoints, **draw hydrology bounding boxes**, **click-append ferry route polylines**, optional greyscale terrain underlay (TICKET-0188).

## Enums

| Field | Values |
| --- | --- |
| region `kind` | `region` \| `fortress` \| `city` \| `wilderness` \| `chaotic` \| `settlement` \| `other` |
| POI `kind` | `landmark` \| `settlement` \| `gate` \| `shrine` \| `camp` \| `other` |
| link `kind` | `travel` \| `soft_gate` \| `story_gate` \| `adjacency` |
| link `fromKind` / `toKind` | `region` \| `poi` |
| hydrology `kind` | `lake` \| `river` \| `sea` |
| `canonStatus` | `established` \| `draft` \| `proposal` \| `open` |

## Validation

- `schemaVersion` must be `1`
- Unique non-empty region / POI / link ids
- Unique non-empty hydrology / ferry route ids
- `parentRegionId` empty or another region id (no self)
- Every POI `regionId` must exist
- Link endpoints resolve to region/POI by `fromKind` / `toKind`; no self-loops
- Ferry `fromPoiId` / `toPoiId` must reference POI ids; distinct endpoints
- Hydrology `acts` and ferry `acts` use the same act tokens as regions/POIs
- When factions file is present, region `factionIds` must match faction entity ids

Error codes: `WORLD-FORGE-MAP-*` (see `WorldForgeMapAsset`).

Project `validate` loads the default path when present and cross-checks faction ids.

## Non-goals

- Do **not** invent city/town names or precise map titles still marked open in story.
- No mesh placement (Scene/MCP).
- No player HUD mini-map rendering (EPIC-0007) — editor Map Canvas is authoring-only.
- MCP mutate: use `engine_world_forge_apply` (TICKET-0014).

## Related

- Act lens: [`world-forge-acts.md`](world-forge-acts.md) (DEC-0036 / TICKET-0189)
- Scene marker overlay: TICKET-0190 (editor-only poles in Scene/Sculpt)
- Editor canvas: TICKET-0187 / TICKET-0188
- [`../features/world-forge-scope.md`](../features/world-forge-scope.md)
- [`world-forge-factions.md`](world-forge-factions.md)
- Header: `include/engine/assets/world_forge_map_asset.h`
