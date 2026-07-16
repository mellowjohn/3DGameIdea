# TICKET-0148: Component reference: catalog and how they work

- Epic: EPIC-0009
- Status: ready
- Agent: unassigned
- Priority: P2
- Notion: https://app.notion.com/p/39cd3efc569581e1873beaae01b3170f

## Goal

Produce a durable `context/` reference that catalogs every entity component the engine uses today (core ECS and authored Add Component types), and explains how they are authored, serialized, inherited/overridden from prefabs, applied via editor/MCP, and consumed at runtime — so agents and humans do not have to reverse-engineer headers and format notes.

## Context links

- `context/planning/epics.md` (EPIC-0009)
- Predecessor: [`TICKET-0147.md`](TICKET-0147.md) (authoring + MCP implementation)
- [DEC-0016](../../decisions/index.md#dec-0016-entity-attached-components-and-dual-mcp-apply-paths)
- [DEC-0017](../../decisions/index.md#dec-0017-prefab-and-scene-component-authoring-with-unity-like-inheritance)
- `context/architecture/overview.md` (hybrid ECS)
- `context/architecture/content-vs-engine-workflows.md`
- `context/formats/prefab-assets.md`, `context/formats/world-placement.md`
- `context/features/editor-mvp.md`, `context/features/mcp-live-editor.md`, `context/features/lua-scripting.md`
- `include/engine/world/components.h`, `include/engine/world/authored_components.h`
- DEC-0003 (command-backed tools)

## Acceptance criteria

- [ ] New (or clearly indexed) context doc — prefer `context/architecture/components.md` — lists **core ECS components** from `components.h` (`Id`, `Name`, `Transform`, `Hierarchy`, `WorldPlacement`) with: purpose, who owns writes (commands vs systems), and persistence notes.
- [ ] Same doc catalogs **authored components** (`Collider`, `ScriptBinding` / kinds) with: JSON shape pointers, prefab seed vs entity ownership, `source` / `overridden` semantics, propagation rules (DEC-0017), and effective collision fallback when entity components are absent.
- [ ] Documents the **authoring surface matrix**: Prefab Editor, Scene Inspector Add Component, CLI/commands, MCP (`engine_entity_component_apply`, `engine_prefab_component_apply`, `engine_scene_apply` / `engine_prefab_apply` actions).
- [ ] Documents **runtime consumption** at a pointer level (collision tracker / placement, Lua handler kinds) without duplicating full Lua or collision specs — link out instead.
- [ ] Explicit **extension checklist**: what a future authored component type must touch (enum, JSON, validate, commands, MCP, editor menus, tests, this doc).
- [ ] `context/architecture/overview.md` (and features index if needed) link to the new doc; `epics.md` Notes point at it.
- [ ] Gaps / future types called out as out of scope for this ticket (do not invent new component kinds). Inspector property edit / Open Script is TICKET-0149 (needs-approval).

## Out of scope

- Implementing new component types or expanding beyond collider + scriptBinding.
- Changing inherit/override behavior or MCP contracts (file bugs / follow-on tickets if docs uncover defects).
- Full Unity-style component marketplace or arbitrary user-defined C++ component plugins.
- Completing TICKET-0116 Lua API expansion.
- Specialized M10 tools (TICKET-0131–0138).

## Dependencies

- Soft: TICKET-0147 implementation landed (code + formats); owner approval of 0147 not required to start docs.
- Blocks: clearer agent onboarding for scene/prefab component edits; reduces inventing duplicate format notes.

## Verification

- Doc-only review against acceptance checklist.
- Spot-check doc claims against `components.h`, `authored_components.h`, `prefab-assets.md`, `world-placement.md`, and MCP feature doc.
- No engine rebuild required unless code/comments are touched (they should not be for this ticket).

## Agent notes

_(empty)_
