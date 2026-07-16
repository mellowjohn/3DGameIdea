# TICKET-0150: Viewport green collider overlays (box/sphere)

- Epic: EPIC-0009
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ed3efc569581ef8586c5f42ec671f8

## Goal

In the editor viewport, authored collider components visualize as green wireframes — box as an AABB outline, sphere as a projected circle — on the selected scene entity and on the prefab-edit preview, so authors can see volume size/offset while editing. Agents finish at `needs-approval`; only the human moves to `done`.

## Context links

- Predecessor: [`TICKET-0149.md`](TICKET-0149.md) (inspector edit; gizmos were out of scope)
- `src/rendering/render_app.cpp` (`draw_authored_collider_overlays`, `draw_debug_aabb`, `draw_debug_sphere`)
- `include/engine/world/authored_components.h`, `src/world/prefab_collision.cpp` (world transform = placement × volume.transform)
- `context/features/editor-mvp.md`

## Acceptance criteria

- [x] Selected placed entity with authored/effective colliders draws green wireframe in the Scene viewport: **box** → AABB, **sphere** → screen-space circle sized from world radius.
- [x] Volume uses the same placement × local transform (offset/scale) path as physics spawn.
- [x] Prefab Editor preview at `(0, 3, 0)` draws the prefab’s `collision[]` volumes the same way while editing.
- [x] Overlay does not require “Show collision debug” (that remains the broader physics body dump); green authored overlays are selection/prefab-edit focused.
- [x] Context: `editor-mvp.md` mentions green collider overlays.
- [x] Rebuild `engine`; no suite regression from touched helpers (doc/UI-heavy; extend a small test only if a pure math helper is extracted).

## Out of scope

- Interactive drag-handles to resize colliders in the viewport.
- Full 3D tessellated sphere mesh / rotated OBB wireframe beyond axis-aligned AABB for boxes.
- Showing every collider in the world at once (use existing collision debug for that).

## Dependencies

- Soft: TICKET-0147/0149 collider authoring.

## Verification

- Rebuild `engine` — succeeded. Remaining warnings: existing `getenv`/`sscanf` C4996 in `render_app.cpp`.
- Manual smoke: select entity with collider; Prefab Editor collision at preview root.

## Agent notes

- `draw_authored_collider_overlays` draws green AABB/circle for selected entity effective volumes + prefab-edit collision at `(0,3,0)`.
- Reuses `multiply_transforms` / scale helpers matching `prefab_collision.cpp`; independent of Show collision debug.
