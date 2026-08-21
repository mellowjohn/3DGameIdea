# Act 0 item inventory icons — Provenance

Status: play-test / inventory UI icons under `samples/open-world-rpg/assets/ui/icons/items/`

## Files (ammo)

| File | Role |
| --- | --- |
| `crude_arrow.png` | Starter Outrider ammo (`crude_arrow`) — dedicated icon (was temporary shortbow art) |
| `outrider_arrow.png` | World-mesh ammo clutter (`outrider_arrow`) — dedicated icon |

Generated 2026-08-05 to match existing low-poly item-icon style (reference: `outrider_shortbow.png`). Catalog paths in `assets/items/act0_landfall_items.json`.

## License / use

`siege_tonic.png` and `unknown_item.png` were generated 2026-08-12 via the built-in image generator, rendered against a removable green chroma-key background, and locally converted to alpha PNGs. `siege_tonic.png` is the dedicated icon for `siege_tonic`. `unknown_item.png` is the universal fallback for a valid inventory item with no catalog icon; `assets/scripts/ui_handlers.lua` applies it after empty-slot handling, covering bag, hotbar, equipped, ammo, and selected-item views without replacing an item-specific icon.

Generated for internal prototype inventory chrome. Confirm redistribution terms before a commercial ship; owner may replace with Blockbench orthographic stills later.
