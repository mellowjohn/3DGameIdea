---
name: pencil-ui-chrome
description: >-
  Author Act 0 / in-game UI mocks in Pencil (.pen), with menu chrome PNGs,
  stained-glass cards, and alpha-matte workarounds. Use when editing
  act0-menu-creation-ui.pen, dialogue/HUD .pen files, menu-assets, glass cards,
  title logo compositing, or Pencil image fills that show black rectangles.
---

# Pencil UI Chrome

Design Act 0 menu / creation and related UI mocks in Pencil, with project chrome assets.

**Read first:** [`context/design/README.md`](../../context/design/README.md), [`context/design/menu-assets/PROVENANCE.md`](../../context/design/menu-assets/PROVENANCE.md), [`context/art/ui-chrome-direction.md`](../../context/art/ui-chrome-direction.md).

**Critical:** `.pen` files are encrypted — use **only** Pencil MCP tools (`user-highagency.pencildev-extension-pencil`). Never `Read` / `Grep` / `Write` the `.pen` bytes.

## Checklist

```
Pencil UI:
- [ ] get_editor_state(include_schema: true) on the open .pen
- [ ] get_guidelines for the task type
- [ ] Prefer existing menu-assets / concepts — do not invent a new chrome language
- [ ] Handle Pencil alpha: matte PNGs or blendMode screen for black-backed logos
- [ ] get_screenshot / snapshot_layout to verify; zone APPROVED vs REVIEW
```

## 1. Pencil MCP order

1. `get_editor_state` with `include_schema: true` — required before other tools.
2. `get_guidelines` for the design task.
3. `batch_get` / `batch_design` for reads/writes.
4. `get_screenshot` + `snapshot_layout` to verify.

Primary file: `context/design/act0-menu-creation-ui.pen`  
Related: `dialogue-ui.pen`, `quest-ui.pen`, `inventory-ui.pen`, `player-hud.pen`, `rpg-engine-ui.pen`, `world-forge-map-canvas.pen`.

## 2. Canvas zones (Act 0 menu)

| Zone | Color cue | Meaning |
| --- | --- | --- |
| APPROVED | green | Locked layout — change only with owner ask |
| NEEDS REVIEW / REVIEW | amber | Active polish |
| REFERENCE | blue | Inspiration / sticker sheet |
| LEGACY | red | Superseded — do not copy into APPROVED |

Sticker sheet order: UI tokens → lane colors → difficulty tiers → chrome → glass → backdrops.

## 3. Asset locations

| Role | Path |
| --- | --- |
| Chrome / glass PNGs | `context/design/menu-assets/` |
| Pencil iron/scrim mattes | `context/design/menu-assets/pencil-matte/` |
| Concept backdrops | `context/art/concepts/` |
| Polish scripts | `context/design/menu-assets/_polish_menu_*.py`, `_punch_title_logo.py` |

### Difficulty display names (locked)

| Tier | Name |
| --- | --- |
| Normal | Ashen's Levy |
| Hard | Calrenoth Breach |
| Nightmare | Frangitur's Claim |

### Class lanes

Ashfell Blade · Outrider · Runecaster — glass cards + glass icons under `menu-assets/`.

## 4. Pencil alpha rules (do not rediscover)

Pencil **image fills do not composite PNG alpha**.

| Problem | Fix |
| --- | --- |
| Transparent PNG shows as black box | Use `pencil-matte/` composite (iron/scrim/charcoal behind art) |
| Title logo black rectangle over siege | Prefer `wrathful-conquest-title-on-menu.png`, or `…-title-logo-screen.png` with **`blendMode: screen`** |
| Green-screen concept export | Run polish scripts (chroma → alpha → matte) before reimport |

## 5. Visual language

- **Chrome:** low-poly / Unturned-adjacent blocky iron + gold (same as HUD/dialogue).
- **Glass:** leaded jewel-tone windows (Dragon Age–adjacent class wall); not photoreal.
- Selected class/difficulty: gold + ember glow — not a yellow hotspot box.
- Screen dims ~40% black so stained-glass backdrops still read.
- Buttons must sit **inside** panels (avoid ~overflow at large heights).

## 6. Backdrop hygiene

Do **not** reuse later Landfall beat shots (`act0-a0-07`, `act0-a0-08`, siege menu) as prologue carousel stills — prologue uses `act0-prologue-0*.png` only.

Update `PROVENANCE.md` when adding/replacing menu PNGs.

## Done bar

Schema loaded; correct zone edited; alpha/matte handled; screenshot evidence; provenance updated if assets changed.
