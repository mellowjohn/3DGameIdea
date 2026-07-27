# World Forge Archetypes (`archetypes.worldforge.json`)

Status: active (schemaVersion 1) — TICKET-0186 · Epic EPIC-0002 · names/orgs [DEC-0044](../decisions/index.md#dec-0044-starting-archetype-lane-orgs-and-rename)

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
      "id": "ashfell_blade",
      "kind": "starting",
      "displayName": "Ashfell Blade",
      "role": "Melee — House Ashfell steel arm (Fighter / Brawler)",
      "summary": "...",
      "draftAdvancement": "Fighter or Brawler lean inside House Ashfell; later advanced by morality and allegiance",
      "starterKitPrefabId": "assets/prefabs/Player/player.prefab.json",
      "storyRef": "context/story/ashfell-blade.md",
      "tags": ["starting", "melee", "house_ashfell", "fighter", "brawler"],
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

Home org and sub-themes are authored in `role` / `summary` / `draftAdvancement` / `tags` until a dedicated schema field exists.

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

| id | kind | Home org | Notes |
| --- | --- | --- | --- |
| `ashfell_blade` | starting | House Ashfell | Melee; Fighter/Brawler; starter kit points at player prefab |
| `outrider` | starting | Outrider Lodge | Ranged; Ranger/Forager/Nomad; kit TBD |
| `runecaster` | starting | Runecaster Guild | Rune/Sigil caster; kit TBD |

No advanced archetypes seeded — deferred until after the demo per story context.

Legacy display names **Squire / Archer / Acolyte** map to the rows above ([DEC-0044](../decisions/index.md#dec-0044-starting-archetype-lane-orgs-and-rename)).

## Non-goals

- Character-creation UI / appearance customization fields
- Runtime class progression / combat kits beyond prefab id references
- Inventing detailed advanced archetype lists ahead of owner approval
- Dedicated `homeOrg` / `subThemes` schema fields (encode in prose + tags for now)

## Related

- Story: [`character-creation.md`](../story/character-creation.md), [`ashfell-blade.md`](../story/ashfell-blade.md), [`outrider.md`](../story/outrider.md), [`runecaster.md`](../story/runecaster.md)
- MCP: [`world-forge-mcp.md`](world-forge-mcp.md) `kind=archetypes`
- Editor: World Forge → **Archetypes** pane (TICKET-0186)
