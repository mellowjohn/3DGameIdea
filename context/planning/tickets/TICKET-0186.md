# TICKET-0186: Archetype catalog World Forge schema + pane

- Epic: EPIC-0002
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39fd3efc5695811ba578f9a3c3b9037e

## Goal

Ship a diffable World Forge archetype catalog (`archetypes.worldforge.json`) with C++ load/validate/save, MCP `kind=archetypes`, and an editor Archetypes tab for authoring starting/advanced archetypes per DEC-0009.

## Context links

- `context/formats/world-forge-archetypes.md`
- `context/story/character-creation.md`, `context/story/the-squire.md`
- `context/features/world-forge-scope.md`, `context/features/editor-mvp.md`
- DEC-0009, DEC-0019, DEC-0003
- Pattern: TICKET-0183 (pantheon schema) / Quests pane list+detail

## Acceptance criteria

- [x] Schema documented (shape, enums, path, unlock object, non-goals)
- [x] Sample seeds Squire, Archer, Acolyte as `starting`; no invented advanced list
- [x] `WorldForgeArchetypesAsset` load/parse/to_json/save_atomic/validate; `WORLD-FORGE-ARCHETYPE-*` errors
- [x] Soft-check `unlock.factionId` against factions when known
- [x] MCP `kind=archetypes` get/validate/apply; project validate when file present
- [x] World Forge **Archetypes** pane: list+detail, create from display name, prefab/faction dropdowns
- [x] `world_forge` suite coverage

## Out of scope

Character-creation appearance UI; runtime class progression; inventing detailed advanced archetypes.

## Dependencies

Soft: factions asset for unlock.factionId cross-check. Story canon: DEC-0009.

## Verification

- Rebuild `engine` + `engine_suite_tests` (MSBuild Debug) — passed (C4996 warnings only in render_app).
- `--suite world_forge` — **151/151** passed (includes archetypes load/round-trip/bad kind/faction ref + MCP validate).

## What changed

### Summary

Added `archetypes.worldforge.json` as a first-class World Forge asset and a top-level **Archetypes** editor pane. Authors can catalog starting/advanced archetypes (role, draft advancement, starter kit prefab, optional morality/faction unlock stubs) with the same Reload/Save command path as other World Forge kinds.

### Files / surfaces

**Created:** `world_forge_archetypes_asset.h/.cpp`, sample archetypes JSON, `context/formats/world-forge-archetypes.md`, this ticket stub

**Modified:** CMakeLists, `world_forge_commands.cpp`, `command.cpp` validate, `mcp_server.cpp` tool blurb, `world_forge_editor.h/.cpp`, `suite_tests.cpp`, coverage/indexes/scope/editor-mvp

### Schema / API

Path `assets/world-forge/archetypes.worldforge.json`. Kind: `starting`|`advanced`. Optional `unlock.{moralityThreshold,factionId,tags}`. MCP `kind=archetypes`. Errors: `WORLD-FORGE-ARCHETYPE-*`.

### Seed / sample

`squire`, `archer`, `acolyte` (starting). Squire starter kit points at player prefab. Advanced list deferred.

### Tests / rebuild

world_forge 151/151; engine.exe rebuilt; MCP process restarted.

### Decisions & leftovers

No separate character-creation flow tab. Advanced archetype thresholds remain story-open; unlock fields are stubs for later runtime.

## Agent notes

Owner requested World Forge Archetypes catalog tab (option 1).
