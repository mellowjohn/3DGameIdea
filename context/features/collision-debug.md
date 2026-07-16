# Collision Debug and Contact Events

## Contact events

`CollisionWorld::drain_contact_events()` returns trigger enter/exit records produced during the most recent physics steps. Call it once per frame or test step after `step()`.

Each `ContactEvent` includes:

- `type`: `Enter` or `Exit`
- `body_a`, `body_b`: colliding body ids
- `layer_a`, `layer_b`: layers recorded at event time
- `contact_point`: optional world position on enter; omitted on exit and unload

Ordering: events are appended in Jolt callback order within a step. Unrelated pairs have no cross-step ordering guarantee. Enter always precedes exit for the same body pair.

Trigger bodies use `CollisionLayer::Trigger` (sensor, no physical response). Removing a body or unloading its owning cell emits exit events for active trigger overlaps.

## Shape queries

`overlap_sphere(center, radius, filter)` and `overlap_box(center, half_extent, filter, rotation)` return zero or more `OverlapHit` records with body id, layer, contact point, and penetration normal. Optional `CollisionQueryFilter::layer` restricts matches to one layer. Box queries accept an optional quaternion rotation (defaults to identity).

`sweep_sphere(origin, direction, radius, filter)` returns the closest `SweepHit` along the segment (body, layer, contact point, normal, fraction). Direction length defines sweep distance; fraction is in `[0, 1]` along that segment.

## Editor debug overlay

Diagnostics panel toggle **Show collision debug** draws collision bounds in the World Viewport:

- Green wireframes: static colliders and terrain heightfields
- Cyan wireframes/fill: trigger sensors
- Gold wireframes/fill: interaction volumes
- Red wireframes/fill: combat hit volumes
- Magenta wireframes/fill: combat hurt volumes
- Orange wireframes: dynamic bodies
- Red crosses: recent contact points (when physics events are available)

Toggle state is session-local and not persisted. When placement collision is active, the editor steps physics each frame and records recent trigger enter contact points for the overlay.

## Prefab collision

Versioned prefab JSON may include a `collision` array (see `context/formats/prefab-assets.md`). Each volume specifies shape, layer, optional trigger flag, local transform, and size.

- Editor **Show collision debug** draws prefab placement volumes alongside streamed terrain physics.
- Bodies are owned by the placement partition cell and unload with `CollisionWorld::unload_cell()`.
- Rotated box debug wireframes remain axis-aligned approximations; physics uses the authored orientation.
