# World Forge Factions (`factions.worldforge.json`)

Status: active (schemaVersion 1) — TICKET-0011 · Epic EPIC-0002

Diffable faction / culture / clan / warband registry keyed to story IDs. Human narrative canon stays in [`../story/factions.md`](../story/factions.md); this file is the engine/integration layer ([DEC-0019](../decisions/index.md#dec-0019-world-forge-editor-home-and-story-canon-split)).

## Default path

`assets/world-forge/factions.worldforge.json`

Helper: `default_world_forge_factions_path(project_root)`.

Sample: `samples/open-world-rpg/assets/world-forge/factions.worldforge.json`.

## Shape

```json
{
  "schemaVersion": 1,
  "id": "tessera_factions",
  "entities": [
    {
      "id": "kingdom_tessera",
      "kind": "faction",
      "displayName": "Kingdom of Tessera",
      "canonStatus": "draft",
      "summary": "...from story...",
      "storyRef": "context/story/factions.md#kingdom-of-tessera",
      "tags": ["human"],
      "politicalRole": "unknown",
      "parentId": "",
      "openQuestions": ["Whether kingdom is playable faction choice or political arena"],
      "standing": {
        "tracksPlayer": true,
        "min": -100,
        "max": 100,
        "ranks": [
          { "id": "hostile", "minScore": -100, "displayName": "Hostile" },
          { "id": "neutral", "minScore": 0, "displayName": "Neutral" },
          { "id": "friendly", "minScore": 25, "displayName": "Friendly" }
        ],
        "lockIn": {
          "threshold": 80,
          "exclusiveFactionIds": ["rival_faction"]
        }
      }
    }
  ]
}
```

Optional `standing` ([DEC-0029](../decisions/index.md#dec-0029-continuous-faction-standing-with-hostility-transfer)): when `tracksPlayer` is true, session `StandingRuntime` tracks a continuous score (default 0). `ranks` must be ordered by non-decreasing `minScore`. `lockIn` is optional; when score ≥ `threshold`, `lock_in_faction()` returns this faction id. Do **not** invent Cristallo/Arrotrebae thresholds in sample data until owner fills numbers.

## Enums

| Field | Values |
| --- | --- |
| `kind` | `faction` \| `culture` \| `clan` \| `warband` |
| `canonStatus` | `established` \| `draft` \| `proposal` \| `open` |
| `politicalRole` (optional) | `arena` \| `faction` \| `unknown` |

`parentId` is empty for roots. When set, it must match another entity `id` in the same file (clan/warband children under a coalition later).

## Validation

- `schemaVersion` must be `1`
- Entity `id` required, unique, non-empty
- `kind` / `canonStatus` must be known enums
- `politicalRole`, when present, must be a known enum
- Non-empty `parentId` must exist in the same asset
- When `standing` is present: `min` ≤ `max`; rank ids unique; ranks ordered by non-decreasing `minScore`; `lockIn.exclusiveFactionIds` must reference other entities in the file

Error codes: `WORLD-FORGE-FACTION-*` (see `WorldForgeFactionsAsset`), including `WORLD-FORGE-FACTION-STANDING-*`.

Project `validate` loads the default path when present (`WorldForgeFactionsAsset::validate_file`).

## Seed entities (v1 sample)

| id | kind | canonStatus | Notes |
| --- | --- | --- | --- |
| `kingdom_tessera` | faction | draft | `politicalRole: unknown` |
| `chaotic_imperium` | faction | established | Existence + leader established; openQuestions for Shroud links |
| `cristallo` | faction | draft | openQuestions for theology / influence |
| `arrotrebae` | faction | draft | Later tribes/clans as children via `parentId`; none invented here |
| `orc_warbands` | faction | draft | Multi-warband container tags; no invented warband names |

## Non-goals

- Do **not** invent answers to open canon questions from `factions.md` (theology, council rules, orc warband names, influence thresholds).
- Omit undecided optional fields or capture them only as `openQuestions` / `canonStatus`.
- No World Forge UI or MCP mutation commands (see TICKET-0014 / `engine_world_forge_apply`).
- No relationship graph edges (see [`world-forge-relationships.md`](world-forge-relationships.md) / TICKET-0012).
- No regions/POIs (see [`world-forge-map.md`](world-forge-map.md) / TICKET-0013).

## Related

- [`../features/world-forge-scope.md`](../features/world-forge-scope.md)
- [`world-forge-relationships.md`](world-forge-relationships.md)
- Header: `include/engine/assets/world_forge_factions_asset.h`
