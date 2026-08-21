# Navigation Grid

Partition-aligned walkability samples derived from the same height function as stylized terrain.

## Cell alignment

- Navigation cells use the world partition grid: **128 m** per cell (`WorldPartition::config().cell_size`).
- Each cell stores a regular height/walkability lattice with resolution **33** (32 spans, **4 m** sample spacing).
- Walkability marks samples whose terrain slope is at or below **0.45** as traversable.
- Deep / submerged water samples are marked unwalkable (same height + water surface rules as terrain).

## Runtime API

`build_navigation_grid(partition_cell)` builds one cell synchronously from `sample_terrain_height()` (honors active terrain edits when the editor has set them).

`StreamedNavigationField` mirrors terrain streaming:

- `update(camera_position, radius)` loads a square neighborhood of partition cells (default radius **4** → 81 cells) and unloads cells outside it.
- `ensure_loaded_for_query(from, to, margin)` loads cells covering a path segment (used by MCP / AI path follow).
- `nearest_walkable_point(query, max_search)` returns the closest walkable world position within one loaded cell.
- `line_of_walk(from, to)` validates a straight walk across loaded partition cells by sampling the grid along the segment. Returns an error when a required cell is not resident.
- `find_path(from, to, snap_radius, simplify)` runs **A\*** on loaded 4 m samples (8-connected, no corner-cutting), snaps endpoints to nearest walkable, optionally string-pulls with `line_of_walk`. Returns `NavigationPath` (`found`, `points`, `length_xz`).

## Agent / MCP

`engine_pathfinding_call` (`kind`: `status` | `find_path` | `nearest_walkable` | `line_of_walk`) queries the same field. Prefer a live editor so sculpted combat / world terrain edits are active; offline MCP builds a temporary field from analytic height.

## Limitations

- Height samples come from the analytic terrain height function (+ edits), not triangle mesh collision or prop colliders.
- No character radius, step height, or dynamic obstacle carving yet.
- Queries fail / return `found: false` when the needed partition cells are not resident or no walkable snap exists.
- Recast/detour remains deferred ([TICKET-0109](../planning/epics.md)).
