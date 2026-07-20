# World-map tile provenance

- Clean continent/ocean hires generated 2026-07-20 via Cursor GenerateImage (project-owned AI art).
- Pyramid built by `tools/build_world_map_tiles.py`: 4096-wide continuous master from overview upscale only.
- Detail-plate inject (`--with-detail-plates`) is off by default — independent NW/NE/SW/SE AI plates do not share geography and previously baked visible 2×2 seams into every LOD.
- No settlement, road, or highway markings — those are World Forge–authored overlays.
- Runtime tiles: `samples/open-world-rpg/assets/ui/cartography/world-map-tiles/`.
