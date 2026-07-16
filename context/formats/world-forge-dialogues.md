# World Forge Dialogues (`dialogues.worldforge.json`)

Status: active (schemaVersion 1) — TICKET-0052 · Epic EPIC-0006 · [DEC-0026](../decisions/index.md#dec-0026-quest-owned-dialogue-hooks-multi-stage)

Branching dialogue trees for quest stages and free-standing conversations. Quests own which tree runs per stage; trees may set `parentQuestId` when they belong to a quest.

## Default path

`assets/world-forge/dialogues.worldforge.json`

Helper: `default_world_forge_dialogues_path(project_root)`.

Sample: `samples/open-world-rpg/assets/world-forge/dialogues.worldforge.json`.

## Link model (DEC-0026)

- Quest hooks (`dialogue.startId` / objective `dialogueId` / …) point at **tree ids** in this file.
- Optional `parentQuestId` on a tree points back at a quest id (validated when quest ids are known).
- Dialogue does not own objectives or rewards.

## Shape

```json
{
  "schemaVersion": 1,
  "id": "tessera_dialogues",
  "trees": [
    {
      "id": "dlg_act0_wrathful_conquest",
      "parentQuestId": "mq_act0_calrenoth",
      "displayName": "Act 0 — Wrathful Conquest (Twine)",
      "canonStatus": "draft",
      "summary": "…",
      "storyRef": "context/story/sources/wrathful-conquest-act0.twee",
      "entryNodeId": "prologue",
      "nodes": [
        {
          "id": "prologue",
          "speakerId": "narrator",
          "line": "…",
          "choices": [
            { "id": "prologue_c1", "text": "Tutorial", "nextNodeId": "tutorial", "setFlags": [] }
          ]
        }
      ],
      "tags": ["main", "act0"],
      "openQuestions": []
    }
  ]
}
```

Empty `nextNodeId` ends the conversation after that choice. Nodes with no choices are terminal leaves (runtime marks complete on arrival).

## Graph editor (TICKET-0053 / 0165–0168)

World Forge **Dialogues → Graph** canvas (schemaVersion 1; no layout persistence):

- Shared camera/minimap helpers ([DEC-0027](../decisions/index.md#dec-0027-shared-world-forge-graph-camera)) also used by the relationship graph
- Compact / Standard / Expanded node display (speaker, 1–2 line preview, type badge, choice count, flag/warning icons)
- Search (speaker/line/id/flags), bookmarks, back/forward selection history, zoom-to-selected, Ctrl+click edge jump
- Toolbar: New Node/Choice, Delete, Duplicate, Auto Layout, Validate, Frame, Undo/Redo (session mutation stack); Preview stub until TICKET-0177
- Shortcuts when canvas focused: F frame, Ctrl+F search, Ctrl+Z/Y undo/redo, Ctrl+D duplicate, Delete
- **ID lookup fields** (`speakerId`, `nextNodeId`, `entryNodeId`, `parentQuestId`, quest dialogue hooks, map/relationship refs, …) use dropdowns populated from loaded World Forge data (editor rule: `.cursor/rules/lookup-fields-dropdowns.mdc`)

Follow-ons: TICKET-0169–0179 (layout tools, connections, inline edit, schema v2, regions, lint, preview, presets, performance).

## Enums

| Field | Values |
| --- | --- |
| `canonStatus` | `established` \| `draft` \| `proposal` \| `open` |

## Validation

- `schemaVersion` must be `1`
- Tree / node / choice ids unique (choices unique within a tree)
- `entryNodeId` must exist among the tree’s nodes
- Non-empty `nextNodeId` must reference a node in the same tree
- When project validate has loaded quests, non-empty `parentQuestId` must exist

Error codes: `WORLD-FORGE-DLG-*` (see `WorldForgeDialoguesAsset`).

## Authoring

World Forge **Dialogues** pane: **Add dialogue tree** (display name → slug id, optional `parentQuestId`) creates a draft tree with entry node `start`. Graph tools add nodes/choices; **Import Twine** remains for bulk seed.

## Runtime

Headless walker: `DialogueRuntime` (`bind` → `start(treeId)` → `present` / `choose`). Choice `setFlags` accumulate on the session.

## Seed (v1 sample)

| id | parentQuestId | Notes |
| --- | --- | --- |
| `dlg_act0_wrathful_conquest` | `mq_act0_calrenoth` | Imported from Twine Act 0 via `tools/twee_to_world_forge_dialogues.py` |

Side-quest dialogue trees are intentionally absent until authored; SQ quest hooks stay empty soft refs.

## Related

- [`world-forge-quests.md`](world-forge-quests.md)
- Converter: `tools/twee_to_world_forge_dialogues.py`
- Twine source: [`../story/sources/wrathful-conquest-act0.twee`](../story/sources/wrathful-conquest-act0.twee)
