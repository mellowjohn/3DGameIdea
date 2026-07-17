# World Forge Archetypes (`archetypes.worldforge.json`)

Status: active (schemaVersion 1) — TICKET-0186 · Epic EPIC-0002

Diffable player archetype catalog for World Forge authoring ([DEC-0009](../decisions/index.md#dec-0009-starting-archetype-character-creation)). Narrative essays stay in `context/story/`; this file is the engine/integration layer ([DEC-0019](../decisions/index.md#dec-0019-world-forge-editor-home-and-story-canon-split)).

## Default path

`assets/world-forge/archetypes.worldforge.json`

Helper: `default_world_forge_archetypes_path(project_root)`.

Sample: `samples/open-world-rpg/assets/world-forge/archetypes.worldforge.json`.

## Shape

```json
{
  "schemaVersion": 1,
  "id": "tessera_archetypes",
  "entities": [
    {
      "id": "squire",
      "kind": "starting",
      "displayName": "Squire",
      "role": "Melee trainee, noble-house path",
      "summary": "...",
      "draftAdvancement": "Knight or Warrior/Barbarian by faction reputation",
      "starterKitPrefabId": "assets/prefabs/Player/player.prefab.json",
      "storyRef": "context/story/the-squire.md",
      "tags": ["starting", "melee"],
      "unlock": {
        "moralityThreshold": 0.5,
        "factionId": "kingdom_tessera",
        "tags": ["advanced"]
      }
    }
  ]
}
```

`unlock` is optional. Omit it for starting archetypes with no unlock requirements. When present, `moralityThreshold` is optional; `factionId` may be empty; `tags` may be empty.

## Enums

| Field | Values |
| --- | --- |
| `kind` | `starting` \| `advanced` |

## Validation

- `schemaVersion` must be `1`
- Entity `id` required, unique, non-empty
- Known enum for `kind`
- When faction ids are known (factions asset present), non-empty `unlock.factionId` must match a factions entity id

Error codes: `WORLD-FORGE-ARCHETYPE-*` (see `WorldForgeArchetypesAsset`).

Project `validate` loads the default path when present and soft-checks unlock faction refs.

## Seed entities (v1 sample)

| id | kind | Notes |
| --- | --- | --- |
| `squire` | starting | Melee; starter kit points at player prefab |
| `archer` | starting | Ranged; kit TBD |
| `acolyte` | starting | Magic; kit TBD |

No advanced archetypes seeded — deferred until after the demo per story context.

## Non-goals

- Character-creation UI / appearance customization fields
- Runtime class progression / combat kits beyond prefab id references
- Inventing detailed advanced archetype lists ahead of owner approval

## Related

- Story: [`character-creation.md`](../story/character-creation.md), [`the-squire.md`](../story/the-squire.md)
- MCP: [`world-forge-mcp.md`](world-forge-mcp.md) `kind=archetypes`
- Editor: World Forge → **Archetypes** pane (TICKET-0186)
