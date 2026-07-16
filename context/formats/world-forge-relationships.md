# World Forge Relationships (`relationships.worldforge.json`)

Status: active (schemaVersion 1) — TICKET-0012 · Epic EPIC-0002

Diffable relationship graph for people, deities, artifacts, and typed edges to other nodes or to faction IDs from [`world-forge-factions.md`](world-forge-factions.md). Narrative essays stay in `context/story/`; this file is the engine/integration layer ([DEC-0019](../decisions/index.md#dec-0019-world-forge-editor-home-and-story-canon-split)).

## Default path

`assets/world-forge/relationships.worldforge.json`

Helper: `default_world_forge_relationships_path(project_root)`.

Sample: `samples/open-world-rpg/assets/world-forge/relationships.worldforge.json`.

## Shape

```json
{
  "schemaVersion": 1,
  "id": "tessera_relationships",
  "nodes": [
    {
      "id": "luceran_the_hollow",
      "kind": "person",
      "displayName": "Luceran the Hollow",
      "canonStatus": "established",
      "summary": "...",
      "storyRef": "context/story/factions.md#chaotic-imperium",
      "tags": ["imperium"],
      "openQuestions": []
    }
  ],
  "edges": [
    {
      "id": "luceran_leads_imperium",
      "from": { "target": "node", "id": "luceran_the_hollow" },
      "to": { "target": "faction", "id": "chaotic_imperium" },
      "kind": "leads",
      "canonStatus": "established",
      "bidirectional": false,
      "standingTransfer": 0.5,
      "summary": "...",
      "storyRef": "...",
      "openQuestions": []
    }
  ]
}
```

Optional `standingTransfer` ([DEC-0029](../decisions/index.md#dec-0029-continuous-faction-standing-with-hostility-transfer), default `0`): when both endpoints are `faction` and `kind` is `rival` or `opposes`, a primary standing delta `+D` applies `-D * standingTransfer` to the other faction (clamped). Must be ≥ 0.

## Enums

| Field | Values |
| --- | --- |
| node `kind` | `person` \| `deity` \| `artifact` \| `organization` |
| edge `kind` | `ally` \| `rival` \| `member_of` \| `leads` \| `kin` \| `serves` \| `opposes` \| `influences` \| `related` |
| `canonStatus` | `established` \| `draft` \| `proposal` \| `open` |
| endpoint `target` | `node` \| `faction` |

Endpoints with `target: "node"` must match a node `id` in the same file. Endpoints with `target: "faction"` reference `entities[].id` in `factions.worldforge.json` (checked when that file is present during project validate).

Self-loops (same target type + same id on both ends) are rejected.

## Validation

- `schemaVersion` must be `1`
- Node and edge `id` required, unique, non-empty
- Known enums for kinds / canonStatus / endpoint targets
- Node endpoint refs resolve; optional faction-id cross-check via `validate_faction_refs`
- `standingTransfer` ≥ 0

Error codes: `WORLD-FORGE-REL-*` (see `WorldForgeRelationshipsAsset`), including `WORLD-FORGE-REL-STANDING-TRANSFER`.

Project `validate` loads the default path when present and, if factions load, cross-checks faction endpoints.

## Non-goals

- Do **not** invent answers to open canon questions (kinship confirmation, Luceran agency, orc warband names from Twine, romance rules).
- No graph **editor** UI (World Forge UI follow-on after this format).
- MCP mutate: use `engine_world_forge_apply` (TICKET-0014).
- No regions/POIs (TICKET-0013).
- Do not treat Twine-only names (e.g. Grul’thaz / Shadowpaw) as established nodes until owner review.

## Related

- [`../features/world-forge-scope.md`](../features/world-forge-scope.md)
- [`world-forge-factions.md`](world-forge-factions.md)
- Header: `include/engine/assets/world_forge_relationships_asset.h`
