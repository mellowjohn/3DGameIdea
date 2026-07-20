# Cartography stroke tiles provenance

- Generated 2026-07-20 by `tools/generate_cartography_strokes.py` (project-owned procedural ink).
- Transparent RGBA tiles (256×48) for Map Canvas image-stamp rendering along authored XZ polylines.
- Styles: political border, track, road, highway, ferry, river.
- Mountains are **not** stroke overlays — they remain in discrete world-map plates.
- Runtime copies: `samples/open-world-rpg/assets/ui/cartography/strokes/`.
- Coast / ridge natural edges stay in plate art; political strokes sit on the land side only.
