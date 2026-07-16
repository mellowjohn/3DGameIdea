# TICKET-0149: Inspector: edit component props + open script

- Epic: EPIC-0009
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ed3efc5695817b8899dfad1a9bc186

## Goal

After adding a component on a scene entity or prefab, the author can edit that component’s properties in the Inspector / Prefab Editor (Unity-like). For `scriptBinding`, they can also open the resolved Lua script file. Agents finish at `needs-approval`; only the human moves to `done`.

## Context links

- `context/planning/epics.md` (EPIC-0009)
- Predecessor: [`TICKET-0147.md`](TICKET-0147.md) (add/remove + MCP; list-only UI today)
- Soft related: [`TICKET-0148.md`](TICKET-0148.md) (docs should mention inspector edit/open when this lands)
- [DEC-0016](../../decisions/index.md#dec-0016-entity-attached-components-and-dual-mcp-apply-paths), [DEC-0017](../../decisions/index.md#dec-0017-prefab-and-scene-component-authoring-with-unity-like-inheritance)
- DEC-0003 (command-backed tools — GUI must not mutate ECS directly)
- `context/formats/prefab-assets.md`, `context/formats/world-placement.md`, `context/formats/script-assets.md`
- `context/features/editor-mvp.md`, `context/features/lua-scripting.md`
- `src/rendering/render_app.cpp` (Inspector + Prefab Editor component lists)
- `include/engine/world/authored_components.h`, `include/engine/assets/prefab_asset.h`

## Acceptance criteria

- [x] **Scene Inspector — Collider:** selecting/listing a collider shows editable fields for at least shape (box/sphere), half-extent or radius as applicable, and local offset; Apply / commit goes through `SetEntityComponentCommand` (or equivalent command history path) with undo/redo and dirty/save behavior consistent with 0147.
- [x] **Scene Inspector — ScriptBinding:** editable `kind` and `bindingId` via the same command path; instance edits mark override when prefab-linked (DEC-0017).
- [x] **Open Script:** for a scriptBinding, a control resolves `bindingId` (+ kind) through `bindings.script.json` to the project-relative Lua `script` path and opens that file with the OS default association (or reports a clear error if unbound / missing).
- [x] **Prefab Editor:** same property editing for prefab `collision[]` / `script_bindings` (and Open Script for bindings); Save Prefab still propagates to non-overridden instances.
- [x] MCP already supports `set_component`; no contract break. If UI needs new fields, keep JSON shapes aligned with existing formats.
- [x] Context updated: `editor-mvp.md` (and TICKET-0148 doc if it exists) describe inspector property edit + open script.
- [x] Tests or automation coverage for set-component field round-trip where practical; otherwise manual verification steps documented on the ticket.

## Out of scope

- In-engine Lua source editor / syntax highlighting (open external file only).
- Viewport collider gizmos / visual volume editing (may be a follow-on).
- New authored component types beyond collider + scriptBinding.
- Changing prefab inherit/override semantics.
- Completing TICKET-0116 Lua API expansion.

## Dependencies

- Soft: TICKET-0147 implementation (add/remove + `SetEntityComponentCommand` / MCP set).
- Independent of TICKET-0148 docs, but 0148 should note this gap until 0149 lands.

## Verification

- Rebuild `engine` after C++/UI changes — succeeded (stopped locked `engine.exe` once). Remaining warnings: existing `getenv`/`sscanf` C4996 in `render_app.cpp`.
- Suites: `world` 49/49, `scripting` 11/11, `automation` 22/22.
- Binding resolve covered in `scripting` suite; collider/script field set + undo/redo in `world` suite.

## Agent notes

- Inspector + Prefab Editor: TreeNode per component with shape/half-extent/radius/offset/trigger and script kind/bindingId; Apply or deactivate-after-edit commits via `SetEntityComponentCommand`.
- `ScriptBindingsAsset::resolve_script_path` + `resolve_script_binding_path` for Open Script (`ShellExecuteW`); linked `shell32`.
- Docs: `editor-mvp.md`, `script-assets.md`.
