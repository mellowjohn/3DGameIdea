# Player HUD Concept Assets — Provenance

Status: prototype chrome for `player.uicanvas.json` (TICKET-0164 image widgets)

## Source

AI-generated concept art (Cursor image generation), prompted for:

- **Layout inspiration:** Dragon Age Origins combat HUD (circular face portrait, vitals bars, ability hotbar, minimap, quest tracker)
- **Art style:** this project's low-poly / Unturned-adjacent blocky UI — flat colors, hard edges, muted earth palette — not Bioware painted realism

Reference prompts keyed to `context/art/ui-chrome-direction.md`, `visual-direction.md`, and `theme-palette.md` (charcoal iron, muted gold `#D5B978`, parchment `#C9B896`).

## Files

| File | Role |
| --- | --- |
| `hud-portrait-ring.png` | Face viewport mock (frame + placeholder face) |
| `hud-portrait-ring-hollow.png` | Hollow face ring (preferred over solid mid fill) |
| `hud-resource-bar.png` | Health / stamina bar rail chrome |
| `hud-ability-slot.png` | Hotbar cell |
| `hud-minimap-frame.png` | Minimap ring |
| `hud-quest-panel.png` | Quest tracker panel (baked label is mock-only) |
| `hud-icon-sword.png` | Ability icon — attack |
| `hud-icon-shield.png` | Ability icon — block |
| `hud-icon-sprint.png` | Ability icon — sprint |
| `hud-rune-pip-ready.png` | Runecaster cast charge — ready / glow |
| `hud-rune-pip-dim.png` | Runecaster cast charge — spent / recharging |
| `hud-status-bleed.png` | Player status chip — bleed DoT (faceted crimson droplet, true RGBA) |
| `hud-status-poison.png` | Player status chip — poison DoT (faceted green droplet + purple glint, true RGBA) |

Synced to `samples/open-world-rpg/assets/ui/hud/`.

Rune pips are procedural diamond glyphs (Pillow), magic-blue palette aligned to `ui-chrome-direction.md` (`#465AA0`). Runtime swaps ready/dim via `HudRuntime::set_rune_charges`.

Status droplets are procedural faceted icons (`tools/art/_make_status_hud_icons.py`) — corner alpha 0; drawn above afflicted **target** HP chips with remaining-seconds text + duration bar while `StatusEffectRuntime` has stacks on that target.

## Post-process

`_polish_chrome.py` (and legacy `_make_transparent.py`) punches checkerboard / near-black backdrop to true PNG alpha, hollows circular rings and bar fill wells, and trims content bounds. Prefer re-running polish from Cursor `assets/` originals when regenerating.

## Pencil limitation

Pencil **image fills do not composite PNG alpha** (transparent texels render as white boxes). The Play HUD mock in `player-hud.pen` uses **vector chrome** for that reason. Keep these PNGs for engine / uicanvas runtime; use board **09** only as an asset reference on dark panels.

## License / use

Generated for internal design and prototype HUD mockups. Do not treat as final production art until an owner pass replaces or redraws them to match Blockbench / atlas pipeline quality. Confirm redistribution terms before shipping in a commercial build.
