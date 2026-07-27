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
            { "id": "prologue_c1", "text": "Tutorial", "nextNodeId": "tutorial", "setFlags": [], "tone": "Curious",
              "standingAdjust": [{ "factionId": "arrotrebae", "delta": 5 }] }
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

Optional choice fields (omit when unused; still schemaVersion 1):

| Field | Type | Notes |
| --- | --- | --- |
| `tone` | string | Short option-type hint on the choices page (e.g. `Honest`, `Blunt`) |
| `standingAdjust` | `{ factionId, delta }[]` | Applied via `StandingRuntime::adjust` when the player selects the choice |

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

Headless walker: `DialogueRuntime` (`bind` → `start(treeId)` → `present` / `choose`). Choice `setFlags` accumulate on the session. Choice `standingAdjust` entries apply through `StandingRuntime::adjust` when choosing (missing / non-tracked factions log a warning).

Play pipeline (Act 0 `coding_dialogue_runtime_hooks`):

- Lua: `engine.dialogue_start` / `dialogue_present` / `dialogue_choose` / `dialogue_active` / `dialogue_reset`
- MCP: `engine_dialogue_call` kinds `start` | `present` | `status` | `continue` | `choose` | `reset` (allowed during play test)
- Quest hooks remain lookups via `quest_dialogue_hook` / `dialogue_for_stage` — scripts start trees explicitly (DEC-0028)
- Event timelines inject the same session `DialogueRuntime` for `start_dialogue` steps
- Sample volume: interaction `talk_sandbox` → `dlg_sandbox_sample`; campaign `talk_act0` → Act 0 trees
- UI: line page (portrait initials + display speaker + role + typed body + Continue) then choices page (truncated prompt strip + up to four rows with keycaps 1–4, act label, AA tone/standing chips). Keys `1`–`4` activate slots; Esc closes and resets dialogue. Button hover uses engine easing; focus ring on keyboard/gamepad nav. Design target: [`../design/dialogue-ui.pen`](../design/dialogue-ui.pen).
- Diagnostics shows active tree/node

Does not auto-complete quests when a tree finishes.

## Seed (v1 sample)

| id | parentQuestId | Notes |
| --- | --- | --- |
| `dlg_act0_wrathful_conquest` | `mq_act0_calrenoth` | Imported from Twine Act 0 via `tools/twee_to_world_forge_dialogues.py` |
| `dlg_sandbox_sample` | _(none)_ | Sandbox talk tree with tone + standing + `setFlags` (`sandbox.peace_kept` on `greeting_c2`); MCP Scenario D |

Side-quest dialogue trees are intentionally absent until authored; SQ quest hooks stay empty soft refs.

## Related

- [`world-forge-quests.md`](world-forge-quests.md)
- Converter: `tools/twee_to_world_forge_dialogues.py`
- Twine source: [`../story/sources/wrathful-conquest-act0.twee`](../story/sources/wrathful-conquest-act0.twee)
