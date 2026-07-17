# World Forge Resources (`resources.worldforge.json`)

Status: active (schemaVersion 1) — TICKET-0192 · Epic EPIC-0002

Diffable catalog of obtainables that can be found throughout Tessera (minerals, herbs, food, craft materials, quest items). Narrative essays stay in `context/story/`; this file is the engine/integration layer ([DEC-0019](../decisions/index.md#dec-0019-world-forge-editor-home-and-story-canon-split)).

## Default path

`assets/world-forge/resources.worldforge.json`

Helper: `default_world_forge_resources_path(project_root)`.

Sample: `samples/open-world-rpg/assets/world-forge/resources.worldforge.json`.

## Shape

```json
{
  "schemaVersion": 1,
  "id": "tessera_resources",
  "entities": [
    {
      "id": "nefarium",
      "kind": "mineral",
      "displayName": "Nefarium",
      "summary": "...",
      "obtainNotes": "...",
      "storyRef": "context/story/nefarium-and-the-shroud.md",
      "rarity": "legendary",
      "regionIds": [],
      "tags": ["cursed", "void"]
    }
  ]
}
```

`rarity` is optional. `regionIds` soft-links to map region ids when the map asset is present.

## Enums

| Field | Values |
| --- | --- |
| `kind` | `mineral` \| `herb` \| `food` \| `craft` \| `quest` \| `other` |
| `rarity` | `common` \| `uncommon` \| `rare` \| `legendary` \| `unique` |

## Validation

- `schemaVersion` must be `1`
- Entity `id` required, unique, non-empty
- Known enums for `kind` and optional `rarity`
- When map region ids are known, non-empty `regionIds` entries must match

Error codes: `WORLD-FORGE-RESOURCE-*` (see `WorldForgeResourcesAsset`).

## Seed entities (v1 sample)

| id | kind | Notes |
| --- | --- | --- |
| `nefarium` | mineral | Seeded from story Nefarium draft; region links open |

## Non-goals

- Inventory / crafting runtime
- Harvest node placement on Map Canvas (follow-on)
- Inventing a full gathering economy ahead of owner approval

## Related

- Story: [`nefarium-and-the-shroud.md`](../story/nefarium-and-the-shroud.md), [`frangitur-the-great-evil.md`](../story/frangitur-the-great-evil.md)
- MCP: [`world-forge-mcp.md`](world-forge-mcp.md) `kind=resources`
- Editor: World Forge → **Resources** pane (TICKET-0192)
