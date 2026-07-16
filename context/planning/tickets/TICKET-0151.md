# TICKET-0151: Expose existing prefab colliders as entity components

- Epic: EPIC-0009
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ed3efc5695815090a4dac0f3fd675b

## Goal

Placed objects that already have prefab `collision[]` / script bindings (and thus physics volumes in the scene) show those as Inspector components on the entity — including legacy world placements that never wrote a `components` array — so authors can edit them like newly added colliders.

## Context links

- Follow-on to TICKET-0147/0149/0150
- `Scene::ensure_authored_components_seeded`, `seed_missing_authored_components`
- `context/formats/world-placement.md`, `context/features/editor-mvp.md`

## Acceptance criteria

- [x] Editor load seeds missing authored components from the prefab catalog for all placements that have prefab collision/script data; marks scene dirty when any were seeded.
- [x] Selecting a placement still seeds if missing and marks dirty.
- [x] Place (GUI + MCP + batch) and Duplicate seed from prefab (duplicate also copies instance overrides when present).
- [x] World suite covers seed_missing idempotence.
- [x] Docs note that prefab colliders surface as entity components.

## Out of scope

- Rewriting all sample world JSON in-repo unless needed for tests.
- Changing physics spawn behavior beyond component exposure.

## Verification

- Rebuild `engine` — succeeded (existing `getenv`/`sscanf` C4996 warnings).
- `world` suite 54/54.

## Agent notes

- Added `Scene::seed_missing_authored_components`; `ensure_authored_components_seeded` returns whether newly seeded.
- Editor startup seeds after prefab catalog load; Inspector select seeds + dirty; duplicate/batch place seed prefab.
