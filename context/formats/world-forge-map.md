# World Forge Map (`map.worldforge.json`)

Status: active (schemaVersion 1) — TICKET-0013 · Epic EPIC-0002

Diffable story geography: **regions**, **POIs**, and travel/soft-gate **links**. IDs and narrative metadata only — not mesh placement ([DEC-0019](../decisions/index.md#dec-0019-world-forge-editor-home-and-story-canon-split)). Scene entity / prefab refs are optional strings for later wiring.

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

Optional `anchor: { "x", "y", "z" }` on regions/POIs when a world-space hint exists; omit until Scene placement exists.

## Enums

| Field | Values |
| --- | --- |
| region `kind` | `region` \| `fortress` \| `city` \| `wilderness` \| `chaotic` \| `settlement` \| `other` |
| POI `kind` | `landmark` \| `settlement` \| `gate` \| `shrine` \| `camp` \| `other` |
| link `kind` | `travel` \| `soft_gate` \| `story_gate` \| `adjacency` |
| link `fromKind` / `toKind` | `region` \| `poi` |
| `canonStatus` | `established` \| `draft` \| `proposal` \| `open` |

## Validation

- `schemaVersion` must be `1`
- Unique non-empty region / POI / link ids
- `parentRegionId` empty or another region id (no self)
- Every POI `regionId` must exist
- Link endpoints resolve to region/POI by `fromKind` / `toKind`; no self-loops
- When factions file is present, region `factionIds` must match faction entity ids

Error codes: `WORLD-FORGE-MAP-*` (see `WorldForgeMapAsset`).

Project `validate` loads the default path when present and cross-checks faction ids.

## Non-goals

- Do **not** invent city/town names or precise map titles still marked open in story.
- No mesh placement (Scene/MCP).
- No mini-map rendering (EPIC-0007).
- No World Forge editor panels in this ticket.
- MCP mutate: use `engine_world_forge_apply` (TICKET-0014).

## Related

- [`../features/world-forge-scope.md`](../features/world-forge-scope.md)
- [`world-forge-factions.md`](world-forge-factions.md)
- Header: `include/engine/assets/world_forge_map_asset.h`
