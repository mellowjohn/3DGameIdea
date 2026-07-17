# TICKET-0183: Pantheon / religion World Forge schema

- Epic: EPIC-0002
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39fd3efc569581c68e09f8cee2cda7e8

## Goal

Ship a diffable World Forge pantheon schema (`pantheon.worldforge.json`) for Religion hierarchy authorship, with C++ load/validate/save and MCP get/set.

## Context links

- `context/formats/world-forge-pantheon.md`
- `context/features/world-forge-scope.md`
- TICKET-0011 (factions pattern), TICKET-0184 (Hierarchy UI)
- DEC-0035; story deity ids `frangitur`, `creotar`

## Acceptance criteria

- [x] Schema documented (shape, enums, path, non-goals)
- [x] Sample seeds frangitur (established) + creotar (draft); no invented Creo/Wild God
- [x] `WorldForgePantheonAsset` load/parse/to_json/save_atomic/validate; `WORLD-FORGE-PANTHEON-*` errors
- [x] `default_world_forge_pantheon_path`; project validate when present
- [x] MCP `kind=pantheon` get/validate/apply
- [x] `world_forge` suite coverage

## Out of scope

Hierarchy UI (0184); inventing theology; runtime worship.

## Dependencies

Soft: TICKET-0011 pattern. Blocks TICKET-0184 Religion page.

## Verification

- Rebuild `engine_core` + `engine` + `engine_suite_tests` (MSBuild Debug) — passed (C4996 warnings only in render_app).
- `--suite world_forge` — 139/139 passed (includes pantheon load/round-trip/parent/cycle + MCP validate).

## What changed

### Summary

Added `pantheon.worldforge.json` as a first-class World Forge asset for Religion hierarchy. Engine load/validate/save + MCP `kind=pantheon` mirror the factions pattern. Sample seeds only `frangitur` and `creotar` (ids aligned with relationship deity nodes).

### Files / surfaces

**Created:** `world_forge_pantheon_asset.h/.cpp`, sample pantheon JSON, `context/formats/world-forge-pantheon.md`

**Modified:** CMakeLists, `world_forge_commands.cpp`, `command.cpp` validate, `mcp_server.cpp` tool blurb, `suite_tests.cpp`, coverage/indexes

### Schema / API

Path `assets/world-forge/pantheon.worldforge.json`. Kinds: deity|aspect|force. parentId tree with cycle detection. Errors: WORLD-FORGE-PANTHEON-*.

### Seed / sample

frangitur (established), creotar (draft). Creo/Wild God not seeded.

### Tests / rebuild

world_forge 139/139; engine.exe rebuilt.

### Decisions & leftovers

DEC-0035. Deity relationship nodes remain for edges until a follow-on migrates targets.

## Agent notes

Shipped with TICKET-0184/0185 in one change set.
