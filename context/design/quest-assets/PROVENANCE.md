# Quest UI Assets — Provenance

Status: prototype icons for quest journal / HUD / minimap / world map (TICKET-0062)

## Source

Hybrid pack keyed to [`../art/ui-chrome-direction.md`](../../art/ui-chrome-direction.md) and [DEC-0051](../../decisions/index.md#dec-0051-no-xp-power-progression-and-quest-ux):

| Set | Method |
| --- | --- |
| World marks (`?` / `!`) + kind pins | Cursor AI concept art in `ai-source/` → `_ingest_ai_icons.py` (black + checkerboard punch, hollow iron wells on marks, true alpha, square fit) |
| Minimap dots, player arrow, offscreen chevron, focus ring, path dash | `_generate_quest_icons.py` (Pillow, 4× supersample + LANCZOS) |

Style: low-poly / Unturned-adjacent blocky chrome — iron wells, gold accents, kind tints (main `#C4A24A` · side `#6A8A9A` · faction `#8B5A3C` · archetype `#7A6BB0`).

## Files

| File | Role |
| --- | --- |
| `quest-mark-available.png` | World / NPC available quest (`?`) |
| `quest-mark-turnin.png` | Active / turn-in (`!`) |
| `quest-pin-main.png` | Full-map pin — main |
| `quest-pin-side.png` | Full-map pin — side |
| `quest-pin-faction.png` | Full-map pin — faction |
| `quest-pin-archetype.png` | Full-map pin — archetype |
| `quest-minimap-dot-main.png` | Compact minimap objective (main) |
| `quest-minimap-dot-side.png` | Compact minimap objective (side) |
| `quest-minimap-dot-turnin.png` | Compact minimap turn-in |
| `quest-player-arrow.png` | Player facing on minimap / map |
| `quest-offscreen-chevron.png` | Edge pointer when objective is off minimap |
| `quest-waypoint-ring.png` | Selected / focused objective ring |
| `quest-path-dash.png` | Path-to-objective dash segment |
| `pencil-matte/*.png` | Opaque iron composites for Pencil sticker sheet |
| `ai-source/*-ai.png` | Raw Cursor AI exports (before punch) |

Synced to `samples/open-world-rpg/assets/ui/quest/` (true-alpha only).

## Regenerate

```text
python context/design/quest-assets/_ingest_ai_icons.py
python context/design/quest-assets/_generate_quest_icons.py
# then rematte geometric widgets via ingest script or:
# python -c "... rematte from ROOT into pencil-matte/"
```

## Pencil limitation

Pencil **image fills do not composite PNG alpha** (and newly added image paths often fail to resolve). The `quest-ui.pen` REFERENCE sticker sheet uses **vector chrome** for that reason. Keep true-alpha PNGs at this folder root for runtime / uicanvas. Optional opaque composites for experiments: `pencil-matte/` (and `../quest-icons/` copies).

## License / use

Internal design + prototype runtime. Not final production art until an owner redraw pass. Confirm redistribution before commercial ship.
