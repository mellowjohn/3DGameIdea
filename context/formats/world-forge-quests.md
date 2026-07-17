# World Forge Quests (`quests.worldforge.json`)

Status: active (schemaVersion 1) — TICKET-0050 · Epic EPIC-0006 · [DEC-0026](../decisions/index.md#dec-0026-quest-owned-dialogue-hooks-multi-stage)

Diffable quest registry keyed to story catalog IDs. Narrative detail stays in [`../story/side-quest-catalog.md`](../story/side-quest-catalog.md) / beat sheets; this file is the engine integration layer ([DEC-0019](../decisions/index.md#dec-0019-world-forge-editor-home-and-story-canon-split)).

## Default path

`assets/world-forge/quests.worldforge.json`

Helper: `default_world_forge_quests_path(project_root)`.

Sample: `samples/open-world-rpg/assets/world-forge/quests.worldforge.json`.

## Link model (DEC-0026)

- **Quests own dialogue hooks.** A quest may reference **multiple dialogue trees** depending on stage:
  - Quest-level: `dialogue.startId` / `completeId` / optional `abandonId`
  - Per **objective**: `objectives[].dialogueId`
  - Per **fork**: `forks[].dialogueId`
- **Dialogue trees (TICKET-0052)** may declare `parentQuestId` pointing at this quest when they are authored as children of a stage. Dialogue does not own objectives/rewards.

- Soft string refs until dialogue assets validate against known IDs.
- Optional `acts: ["act0"…]` for Act lens filtering ([DEC-0036](../decisions/index.md#dec-0036-world-forge-act-lens); [`world-forge-acts.md`](world-forge-acts.md)). Empty = campaign-wide.

## Shape

```json
{
  "schemaVersion": 1,
  "id": "tessera_quests",
  "quests": [
    {
      "id": "sq_01_cart_again",
      "kind": "side",
      "displayName": "Cart Again",
      "canonStatus": "draft",
      "summary": "...",
      "storyRef": "context/story/side-quest-catalog.md#sq-01--cart-again",
      "consequential": false,
      "regionId": "tessera_overland",
      "starts": "After Act 1 hub unlock…",
      "dialogue": {
        "startId": "dlg_sq01_start",
        "completeId": "dlg_sq01_complete",
        "abandonId": ""
      },
      "objectives": [
        { "id": "find_pellin", "summary": "…", "dialogueId": "dlg_sq01_find_pellin" }
      ],
      "forks": [
        {
          "id": "tone_help_vs_mock",
          "summary": "…",
          "outcomeFlags": ["sq01.arkand_pride", "sq01.arkand_embarrassed"],
          "dialogueId": "dlg_sq01_fork_tone"
        }
      ],
      "standingRequirements": [
        { "factionId": "cristallo", "minScore": 25, "minRankId": "friendly" }
      ],
      "standingRewards": [
        { "factionId": "cristallo", "delta": 10 }
      ],
      "tags": ["side"],
      "openQuestions": []
    }
  ]
}
```

Optional standing fields ([DEC-0029](../decisions/index.md#dec-0029-continuous-faction-standing-with-hostility-transfer)):

- `standingRequirements[]` — gate with `factionId` plus `minScore` and/or `minRankId`
- `standingRewards[]` — `{ factionId, delta }` for callers to apply via `StandingRuntime::adjust` / Lua / MCP

**v1 hook:** QuestRuntime complete does **not** auto-apply `standingRewards`. Scripts or MCP must call `standing_adjust` (or a later QuestRuntime follow-up). Documented intentional gap until wired.

## Enums

| Field | Values |
| --- | --- |
| `kind` | `main` \| `side` \| `faction` |
| `canonStatus` | `established` \| `draft` \| `proposal` \| `open` |

## Validation

- `schemaVersion` must be `1`
- Quest `id` required, unique
- Objective / fork `id` required, unique **within** the quest
- `kind` / `canonStatus` known enums
- When project `validate` has loaded map regions, non-empty `regionId` must exist in the map asset

Error codes: `WORLD-FORGE-QUEST-*` (see `WorldForgeQuestsAsset`).

## Runtime (TICKET-0180 / DEC-0028)

Session-only `QuestRuntime` (`bind` → `start` / `complete_objective` / `abandon` / `status` / `list_active`).

- **Explicit completion only** — dialogue, collect/kill scripts, and MCP all call the same API; `DialogueRuntime` does not auto-advance quests.
- Objectives complete **in catalog order** (current = first incomplete).
- `dialogue_for_stage` returns hooked tree ids (`start` / current objective / `complete` / `abandon`) without mutating progress.
- Live surfaces: Lua `engine.quest_*`; MCP `engine_quest_call`; HUD bind `quest.objectiveText`.
- Persistence is **not** in this format — see TICKET-0114.

Error codes: `QUEST-RUNTIME-*`.

## Standing runtime (TICKET-0181 / DEC-0029)

Session-only `StandingRuntime` (`bind(factions, relationships)` → `get` / `set` / `adjust` / `rank` / `meets_requirement` / `lock_in_faction` / `list_tracked`).

- `adjust` applies hostility transfers once per call from rival/opposes faction edges with `standingTransfer > 0`.
- Live surfaces: Lua `engine.standing_*`; MCP `engine_standing_call`.
- Persistence deferred to TICKET-0114 (alongside QuestRuntime).

Error codes: `STANDING-RUNTIME-*`.

## Seed (v1 sample)

| id | kind | consequential | Notes |
| --- | --- | --- | --- |
| `mq_act0_calrenoth` | main | yes | Act 0 Calrenoth spine; `dialogue.startId` → Twine tree |
| `sq_01_cart_again` | side | no | Catalog seed; dialogue hooks empty until authored |
| `sq_02_signal_fire_debt` | side | yes | Catalog seed; fork flags retained; dialogue empty |

Further catalog IDs (`SQ-03`…) land as seeds when ready; do not invent undisclosed canon.

## Non-goals

- Full quest journal / map markers (TICKET-0062)
- Inventing Act 1+ main quests without beat-sheet coverage
- Inventory/reward item tables
- RPG save of quest progress (TICKET-0114)

## Related

- [`../features/world-forge-scope.md`](../features/world-forge-scope.md)
- [`world-forge-map.md`](world-forge-map.md) — `regionId` soft refs
- [`../story/side-quest-catalog.md`](../story/side-quest-catalog.md)
