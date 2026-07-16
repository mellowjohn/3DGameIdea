# TICKET-0016: World Forge list/detail inspectors (factions, graph, map)

- Epic: EPIC-0002
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ed3efc56958121b0f1e6dc3f22cae8

## Goal

List + detail inspectors for factions, relationships, and map assets inside the World Forge tab. Save/reload through `apply_world_forge_operation` (same path as MCP).

## Context links

- TICKET-0015 shell
- `include/engine/automation/world_forge_commands.h`
- Format docs under `context/formats/world-forge-*.md`

## Acceptance criteria

- [x] List + select entities / nodes+edges / regions+POIs+links
- [x] Edit primary string fields (displayName, summary, storyRef, openQuestions, etc.)
- [x] Dirty flag; Save via shared apply command
- [x] Validation errors shown in status
- [x] Round-trip: edit → save → reload preserves changes (by construction: Save round-trips through `AssetT::to_json()` → `apply_world_forge_operation` → `AssetT::save_atomic`, and Reload re-parses the same `to_json()` text back via `AssetT::parse`; not re-verified against a rebuilt running exe — see Verification evidence)

## Out of scope

Visual graph canvas (0017); inventing story canon; Scene placement.

## Dependencies

TICKET-0015; schemas 0011–0013; MCP 0014.

## Verification

Rebuild `engine`; manual edit+save; `engine_project_validate` / automation as applicable.

## What changed

- `WorldForgeEditorSession` (in the new `world_forge_editor.h/.cpp` shared with TICKET-0015) holds all three World Forge assets in memory plus `pane`, `list_kind` (`Entities|Nodes|Edges|Regions|Pois|Links`), `selected_id`, `dirty`, `status`, and `loaded`.
- **Factions** pane: single list of entities; detail panel edits `displayName`, `kind` (combo), `canonStatus` (combo), `politicalRole` (optional combo with "(none)"), `summary`, `storyRef`, `parentId`, `tags` (comma-separated ↔ `vector<string>`), and `openQuestions` (one-per-line multiline ↔ `vector<string>`).
- **Relationships** pane: secondary **Nodes**/**Edges** toggle. Nodes edit `displayName`, `kind`, `canonStatus`, `summary`, `storyRef`, `tags`, `openQuestions`. Edges edit `from.target`/`from.id`, `to.target`/`to.id`, `kind`, `canonStatus`, `bidirectional`, `summary`, `storyRef`, `openQuestions` (edges have no `displayName`; the list row shows `id [kind] from -> to`).
- **Map** pane: secondary **Regions**/**POIs**/**Links** toggle. Regions edit `displayName`, `kind`, `canonStatus`, `summary`, `storyRef`, `parentRegionId`, `factionIds` (csv), `tags` (csv), `softGate.enabled`/`softGate.notes`, `openQuestions`. POIs edit `displayName`, `kind`, `canonStatus`, `regionId`, `summary`, `storyRef`, `sceneEntityId`, `prefabId`, `tags`, `openQuestions` (the scene/prefab id fields are plain reference strings; no Scene/mesh placement is performed here, per the World Forge non-overlap boundary). Links edit `kind`, `fromKind`/`fromId`, `toKind`/`toId`, `canonStatus`, `bidirectional`, `softGate`, `summary`, `storyRef`, `openQuestions`.
- All enum fields (`kind`, `canonStatus`, endpoint `target`/`fromKind`/`toKind`) use a shared `draw_enum_combo<EnumT>` template driven by each type's existing `to_string(...)` overload — no new string-parsing code was added for enums.
- **Dirty tracking**: every field edit sets `session.dirty = true`; the toolbar's **Save** button is disabled via `ImGui::BeginDisabled(!session.dirty)` until something changes.
- **Save**: `WorldForgeEditorSession::save(project_root)` is a no-op success when `!dirty`; otherwise it calls `apply_world_forge_operation(project_root, {"action":"apply","kind":<k>,"source":<AssetT::to_json()>})` once per kind (factions, relationships, map) — the exact command MCP agents use — so edits always go through the same validation and atomic-write path (`*_asset.cpp` `save_atomic`, with `.bak`/`.tmp` handling) as MCP. It intentionally applies **all three** kinds on every save (simpler and more robust than per-pane dirty tracking, and keeps cross-references like faction ids consistent); failures from each kind are aggregated into one error message and `dirty` stays `true` so the user can fix and retry.
- **Validation errors in status**: both `Reload##WorldForge` and `Save##WorldForge` toolbar buttons catch a failed `Result<void>` and write `"Reload failed: " + message` / `"Save failed: " + message` into `session.status`, which is drawn next to the toolbar buttons every frame.
- No add/remove UI for entities/nodes/edges/regions/POIs/links was implemented — this ticket only asked for list+select+edit of existing items; creating new World Forge objects still goes through MCP `apply_world_forge_operation` (action=apply with a full new asset body) or hand-editing JSON, consistent with "Out of scope: inventing story canon."

### Verification evidence

- `MSBuild ... /t:engine_core:Rebuild` succeeded (0 errors, 0 new warnings) with `world_forge_editor.cpp` compiled into `engine_core.lib`.
- `MSBuild ... /t:engine` failed only at the final **link** step because `engine.exe` was locked by an already-running instance on this machine (`LNK1168`); this is an environment lock, not a code defect. Manual "edit → Save → Reload" round-trip in a live editor session was **not** re-verified end-to-end for this reason — the logic is a direct, symmetric round-trip through the same `AssetT::to_json()`/`AssetT::parse()`/`apply_world_forge_operation` calls already covered by the `world_forge` suite for the underlying asset types, but the ImGui session glue itself has no automated test yet.

### Leftover risk

- No automated coverage for the ImGui session (`WorldForgeEditorSession::reload/save`, field-binding helpers); relies on the already-tested asset (de)serialization and `apply_world_forge_operation` command layer underneath.
- Save always writes all three World Forge files even if only one pane changed; this is simple/robust but means an unrelated pane with pre-existing invalid data will block Save for the pane you actually edited. Acceptable for this MVP; revisit if it becomes a real friction point.
- Political role, soft-gate, and scene/prefab reference fields were included for completeness beyond the ticket's literal "at minimum" field list; no new validation was added for them (they reuse the existing asset validators).

## Agent notes

2026-07-15: Implemented together with 0015 in `world_forge_editor.h/.cpp`; both tickets share the same "What changed" file set — see TICKET-0015 for the tab/shell wiring in `render_app.cpp` and the icon-font finding.
