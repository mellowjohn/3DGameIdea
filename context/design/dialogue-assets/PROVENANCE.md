# Dialogue UI Concept Assets — Provenance

Status: prototype chrome for `dialogue.uicanvas.json` (TICKET-0164 image widgets)

## Source

AI-generated concept art (Cursor image generation), 2026-07-24, prompted for:

- **Layout role:** dialogue modal chrome (panel frame, speaker portrait ring, choice row, continue CTA, key badge, prompt strip)
- **Art style:** same in-game UI language as combat HUD — low-poly / Unturned-adjacent blocky iron + gold + parchment ([ui-chrome-direction.md](../../art/ui-chrome-direction.md))

## Files

| File | Role |
| --- | --- |
| `dialogue-panel-frame.png` | Hollow iron/gold outer frame for parchment plate |
| `dialogue-portrait-ring.png` | Circular speaker portrait ring (hollow center) |
| `dialogue-choice-row.png` | Choice row plate + left key well (engine draws numeral over well; do not stack `dialogue-key-badge.png` on top) |
| `dialogue-continue-button.png` | Gold Continue button face (blank for engine text) |
| `dialogue-key-badge.png` | Standalone numbered keycap chrome (optional; unused when row PNG includes a well) |
| `dialogue-prompt-strip.png` | Retired “last line” / prompt-band experiment; kept only as a provenance source |

Synced to `samples/open-world-rpg/assets/ui/dialogue/`.

## Post-process

`context/design/hud-assets/_polish_chrome.py` punches checkerboard / near-black backdrop to true PNG alpha, hollows frames/rings, and trims content bounds (also covers dialogue assets).

## Runtime note

`dialogue.uicanvas.json` references its active PNGs via widget `image` + `UiTextureCache`. The runtime choices page deliberately omits the prompt strip: it was too ornate at small viewport scales and competed with choice text. Prefer `contain` for rings; stretch only when the widget aspect matches the art. Pencil image fills ignore PNG alpha — prefer vector mocks there.

## License / use

Generated for internal design and prototype dialogue mockups. Do not treat as final production art until an owner pass replaces or redraws them. Confirm redistribution terms before shipping in a commercial build.
