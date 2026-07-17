# World Forge Pantheon (`pantheon.worldforge.json`)

Status: active (schemaVersion 1) — TICKET-0183 · Epic EPIC-0002

Diffable pantheon / religion registry for Hierarchy → Religion authorship ([DEC-0035](../decisions/index.md#dec-0035-world-forge-hierarchy-authorship)). Narrative essays stay in `context/story/`; this file is the engine/integration layer ([DEC-0019](../decisions/index.md#dec-0019-world-forge-editor-home-and-story-canon-split)).

## Default path

`assets/world-forge/pantheon.worldforge.json`

Helper: `default_world_forge_pantheon_path(project_root)`.

Sample: `samples/open-world-rpg/assets/world-forge/pantheon.worldforge.json`.

## Shape

```json
{
  "schemaVersion": 1,
  "id": "tessera_pantheon",
  "entities": [
    {
      "id": "frangitur",
      "kind": "deity",
      "displayName": "Frangitur",
      "canonStatus": "established",
      "summary": "...",
      "storyRef": "context/story/frangitur-the-great-evil.md",
      "tags": ["great-evil"],
      "parentId": "",
      "openQuestions": []
    }
  ]
}
```

## Enums

| Field | Values |
| --- | --- |
| `kind` | `deity` \| `aspect` \| `force` |
| `canonStatus` | `established` \| `draft` \| `proposal` \| `open` |

`parentId` is empty for roots. When set, it must match another entity `id` in the same file (no self-parent; no cycles).

## Validation

- `schemaVersion` must be `1`
- Entity `id` required, unique, non-empty
- Known enums for `kind` / `canonStatus`
- Non-empty `parentId` must exist in the same asset; reject self-parent and parent cycles

Error codes: `WORLD-FORGE-PANTHEON-*` (see `WorldForgePantheonAsset`).

Project `validate` loads the default path when present.

## Seed entities (v1 sample)

| id | kind | canonStatus | Notes |
| --- | --- | --- | --- |
| `frangitur` | deity | established | Same id as relationships deity node |
| `creotar` | deity | draft | Same id as relationships deity node; Creo/Wild God not seeded |

## Non-goals

- Do **not** invent Creo, Wild God, or Cristallo theology until owner-approved.
- Relationship graph deity nodes remain for edge endpoints until a follow-on migrates targets to pantheon ids.
- No runtime worship / faith mechanics in this ticket.

## Related

- Hierarchy UI: TICKET-0184
- MCP: [`world-forge-mcp.md`](world-forge-mcp.md) `kind=pantheon`
- Relationships (deity nodes): [`world-forge-relationships.md`](world-forge-relationships.md)
