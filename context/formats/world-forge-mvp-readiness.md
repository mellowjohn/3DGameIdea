# World Forge MVP readiness (`act0_mvp_readiness.worldforge.json`)

Status: active — Act Zero Overview lens

Curated **Act Zero (Landfall) MVP** checklist that drives the World Forge **Overview** progress bar. This is the authoritative launch-readiness score for the playable Act 0 prototype — not the all-acts field-completeness heuristic.

## Path

`assets/world-forge/act0_mvp_readiness.worldforge.json`

Default helper: `default_world_forge_mvp_readiness_path(project_root)`.

## Schema (v1)

| Field | Type | Notes |
| --- | --- | --- |
| `schemaVersion` | int | Must be `1` |
| `id` | string | e.g. `act0_mvp_readiness` |
| `actId` | string | Canonical act id (`act0`…`act4`) |
| `categories[]` | array | Ordered groups shown in Overview |
| `categories[].id` | string | Unique category id (often matches a workstream) |
| `categories[].title` | string | Display title |
| `categories[].items[]` | array | Checklist rows |
| `items[].id` | string | Unique across the whole file |
| `items[].title` | string | Required display title |
| `items[].status` | string | `todo` \| `wip` \| `done` \| `blocked` |
| `items[].priority` | string | `p0` (now) \| `p1` (next) \| `p2` (later) |
| `items[].workstream` | string | Bounce lens (see below) |
| `items[].notes` | string | Optional short summary |
| `items[].description` | string | Longer context / acceptance for the detail pane |
| `items[].imagePaths` | string[] | Repo-relative PNG paths for concept art |
| `items[].dependsOn` | string[] | Soft prerequisite checklist item ids (do these first) |
| `items[].goal` | bool | Optional. When `true`, this row is a **verifiable acceptance sink**. Overview ranks open work by progress toward open goals. |
| `items[].refs` | object | Optional soft refs (not hard-validated) |

### Workstreams

`art` · `effects` · `coding` · `project` · `storyline` · `gameplay` · `combat` · `archetype` · `cinematics` · `ui_ux`

Aliases: `events` / `theatrical` → `cinematics`; `story` → `storyline`; `pm` → `project`; `ui` / `ux` / `user_interface` → `ui_ux`.

**Cinematics** tracks in-game events and theatrical sequences players watch (prologue, siege backdrop, Luceran shadow, Creotar vision, camp wake) plus minimal timeline tooling — distinct from free-play combat/gameplay.

**UI / UX** tracks player-facing presentation: play HUD, dialogue canvas, character creation screen, interaction prompts, pause/journal, camp screens, cinematic chrome, and accessibility baseline — distinct from the coding workstream that owns runtime binds and canvas stack plumbing.

**Prerequisites:** Overview detail pane lists `dependsOn` with live status. Example: character creation depends on hair/outfit kits, kit slot runtime, and archetype bindings.

### `refs` (optional)

| Field | Purpose |
| --- | --- |
| `storyRef` | Path or story doc pointer |
| `questId` | World Forge quest id |
| `dialogueId` | World Forge dialogue tree id |
| `assetPath` | Mesh/prefab path |
| `ticketId` | Ticket / epic id |

## Progress math

- Authoritative Overview % = `count(status == done) / count(items)`.
- Live Act 0 catalog signals (regions/POIs/quests/dialogues) are **supporting evidence only**.

## Editor

- Overview: full-width checklist table sorted by **next unblocker** by default — open items whose `dependsOn` are all `done`, then highest **Goal+** (how many open goal-path items completing this advances), then Waiting fan-in; optional priority sort. **Hide done** defaults on. Workstream filter chips.
- **Do next** banner highlights the top actionable item toward tagged `goal` sinks (Act 0: playable boot → camp).
- **Goal+** column = open items on a path to a verifiable goal that this row unblocks (including itself).
- **Waiting** column = count of other checklist rows that list this id in `dependsOn`.
- Select a row to open the **detail** pane: who is blocked on this, prerequisites, description, refs, and linked concept images.
- Status combo edits mark World Forge session dirty; **Reload** / **Save** persist this file (not yet an MCP `WorldForgeKind`).
- Project **validate** includes `WorldForgeMvpReadinessAsset::validate_file`.

## Related

- [`world-forge-acts.md`](world-forge-acts.md) — act ids / Act lens
- [`world-forge-events.md`](world-forge-events.md) — event timeline sequences (DEC-0045; TICKET-0221+)
- [`../story/campaign-beat-sheet.md`](../story/campaign-beat-sheet.md) — Act 0 beats
- [`../art/blockbench-asset-list.md`](../art/blockbench-asset-list.md) — Act 0 art backlog
- [`../features/editor-mvp.md`](../features/editor-mvp.md) — Overview UI
