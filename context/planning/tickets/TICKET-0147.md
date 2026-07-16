# TICKET-0147: Entity component authoring + MCP add component/script

- Epic: EPIC-0009
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39cd3efc569581feaf9ff527fb95dcfc

## Goal

Make Unity-style Add Component the supported authoring model on **prefab assets** and **scene entities** (colliders, scripts, and related components), with Unity-like prefab→instance inheritance and per-instance overrides, and expose the same operations through MCP. Agents finish at `needs-approval`; only the human moves to `done`.

## Context links

- `context/planning/epics.md` (EPIC-0009)
- [DEC-0016](../../decisions/index.md#dec-0016-entity-attached-components-and-dual-mcp-apply-paths) (entity components + dual MCP)
- [DEC-0017](../../decisions/index.md#dec-0017-prefab-and-scene-component-authoring-with-unity-like-inheritance) (prefab + scene authoring; inherit/override)
- `context/architecture/overview.md` (hybrid ECS)
- `context/architecture/content-vs-engine-workflows.md` (DEC-0011)
- `context/formats/prefab-assets.md` (prefab components / collision)
- `context/formats/world-placement.md` (placements; GUI must not mutate ECS directly — command path)
- `context/features/editor-mvp.md`, `context/features/mcp-live-editor.md`, `context/features/lua-scripting.md`
- `include/engine/world/components.h`, `include/engine/world/authored_components.h`
- DEC-0003 (command-backed tools), DEC-0010/0011 (automation / content vs engine)
- Soft related: TICKET-0116 (extend Lua beyond interaction/combat handlers)

## Acceptance criteria

- [x] Owner decisions recorded: [DEC-0016](../../decisions/index.md#dec-0016-entity-attached-components-and-dual-mcp-apply-paths) + [DEC-0017](../../decisions/index.md#dec-0017-prefab-and-scene-component-authoring-with-unity-like-inheritance).
- [x] Command-backed add/remove/edit for at least: collider volume, and script/handler binding — on **prefab assets** and **scene entities** — undo/redo and changed UUIDs per DEC-0003.
- [x] Prefab editor can add/remove those components on the prefab under edit.
- [x] Scene Inspector can add/remove those components on a selected scene entity; edits mark instance overrides where applicable.
- [x] Prefab component edits **propagate** to placed instances that have not overridden that component; overridden instance components stay local (document override metadata in world/scene format).
- [x] MCP: dedicated entity/prefab component apply tools **and** equivalent actions on `engine_prefab_apply` / `engine_scene_apply`; `engine_scene_plan` routes correctly.
- [x] Existing prefab `collision[]` remains a valid seed/template with documented compatibility or migration; sample project still validates.
- [x] Context updated: formats, features, content-vs-engine workflows, and MCP feature doc.

## Out of scope

- Full Unity Component marketplace / arbitrary C# MonoBehaviour equivalent.
- Full nested prefab override UX polish beyond the minimum inherit/override model for collider + script components.
- New gameplay systems beyond wiring existing collider, interaction, combat, and Lua handler surfaces.
- Completing TICKET-0116 Lua API expansion (may share binding points; not required to close this ticket).
- Specialized M10 tools already listed as TICKET-0131–0138.

## Dependencies

- Unblocked: DEC-0016 and DEC-0017 accepted.
- Soft: current MCP bridge + `CommandHistory` scene/prefab mutation path.
- Soft related: TICKET-0116 for richer per-entity script surfaces later.

## Verification

- Rebuild `engine` after C++ changes — succeeded (engine.exe was locked once mid-build; rebuilt after stopping process). Remaining warnings: existing `getenv`/`sscanf` C4996 in `render_app.cpp`.
- Suites: `world` 42/42, `automation` 22/22, `collision` 567/567, `assets` 36/36.
- `engine validate` on sample project: exit 0.
- Inherit vs override covered in `world` suite; MCP add/remove via `scene_apply` and `entity_component_apply` in `automation` suite.

## Agent notes

- Implemented `AuthoredComponentsComponent` on scene entities; prefab `collision[]` + `components[]` (scriptBinding); place seeds; Inspector/Prefab Editor Add Component; propagate on prefab save / prefab_apply.
- MCP: `engine_entity_component_apply`, `engine_prefab_component_apply`, plus `add_component`/`remove_component`/`set_component` on `engine_scene_apply`.
- Prefab collision without entity components still works via tracker fallback.
