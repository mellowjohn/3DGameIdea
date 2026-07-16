# World Forge MCP apply (`engine_world_forge_apply`)

Status: active — TICKET-0014 · Epic EPIC-0002

Command-backed read/validate/write for World Forge narrative assets. Shares one path for MCP and the live editor bridge ([DEC-0003](../decisions/index.md#dec-0003-automation-first-tools)).

## Tool

`engine_world_forge_apply`

| Param | Required | Notes |
| --- | --- | --- |
| `action` | yes | `get` (alias `read`), `validate`, `apply` (alias `write`), `import_twee` (alias `import-twee`) |
| `kind` | usually | `factions` \| `relationships` \| `map` \| `quests` \| `dialogues` |
| `path` | optional | Relative `*.worldforge.json`; used to infer kind if omitted |
| `json` / `source` | for apply | Full asset JSON object or string |
| `tweePath` | for `import_twee` | Path to Harlowe `.twee` (absolute or relative to project) |
| `treeId` | for `import_twee` | Dialogue tree id to create or replace |
| `displayName` / `parentQuestId` / `entryNodeId` / `storyRef` | optional | Twine import metadata |

Default paths when `path` omitted: `assets/world-forge/factions.worldforge.json`, `relationships.worldforge.json`, `map.worldforge.json`, `quests.worldforge.json`, `dialogues.worldforge.json`.

## Behavior

- Offline-capable: writes validated JSON to disk without a running editor.
- When the editor MCP bridge is connected, the same operation name `world_forge_apply` is forwarded (still file-backed; no graph UI yet).
- Relationships/map apply/validate cross-check faction ids against the factions asset when present.
- Quests apply/validate soft-check `regionId` against the map asset when present.
- Dialogues apply/validate soft-check `parentQuestId` against the quests asset when present.
- `import_twee` (dialogues only): parse a Twine `.twee` into one tree and upsert into the dialogues asset (TICKET-0054).
- Not Scene/Sculpt: do not place meshes here.

## Classifier

`engine_scene_plan` → `targetKind=world_forge` for `*.worldforge.json` / `world-forge` paths (and clear World Forge descriptions).

## Related

- Formats: factions / relationships / map / quests / dialogues under `context/formats/`
- Implementation: `include/engine/automation/world_forge_commands.h`
- Workflow: `context/architecture/content-vs-engine-workflows.md`
