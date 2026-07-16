# Navigation Grid

Partition-aligned walkability samples derived from the same height function as stylized terrain.

## Cell alignment

- Navigation cells use the world partition grid: **128 m** per cell (`WorldPartition::config().cell_size`).
- Each cell stores a regular height/walkability lattice with resolution **33** (32 spans, **4 m** sample spacing).
- Walkability marks samples whose terrain slope is at or below **0.45** as traversable.

## Runtime API

`build_navigation_grid(partition_cell)` builds one cell synchronously from `sample_terrain_height()`.

`StreamedNavigationField` mirrors terrain streaming:

- `update(camera_position, radius)` loads a square neighborhood of partition cells (default radius **2** → 25 cells) and unloads cells outside it.
- `nearest_walkable_point(query, max_search)` returns the closest walkable world position within one loaded cell.
- `line_of_walk(from, to)` validates a straight walk across loaded partition cells by sampling the grid along the segment. Returns an error when a required cell is not resident.

## Limitations

- Height samples come from the analytic terrain height function, not triangle mesh collision.
- No character radius, step height, or dynamic obstacle carving yet.
- Queries fail when the target partition cell is not resident in the streamed field.
