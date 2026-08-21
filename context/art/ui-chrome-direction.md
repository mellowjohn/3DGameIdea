# In-Game UI Chrome Direction

Status: active — shared language for **all player-facing game UI** (combat HUD, dialogue, pause, inventory, menus, world billboards). Editor chrome stays Roboto / tooling-neutral ([visual-direction.md](visual-direction.md)).

Layout inspiration may borrow combat RPG patterns (e.g. Dragon Age Origins face + vitals). **Art treatment is this project's** low-poly / Unturned-adjacent blocky UI — not painted Bioware realism.

## Intent

Dark-fantasy iron furniture over readable parchment or chrome text. Faceted rings and panels, hard edges, muted earth metals. Gold is accent and affordance, never wallpaper. Matches world materials in [theme-palette.md](theme-palette.md) without copying terrain greens into every panel.

## Surfaces

| Surface | Treatment | Primary text |
| --- | --- | --- |
| Combat HUD (always-on) | Dark iron panels, gold studs/borders, circular face + minimap | Chrome `#F1EEE8` on dark |
| Dialogue modal | Parchment plate in iron/gold frame; circular speaker portrait | Ink `#483E30` on parchment |
| Pause / inventory / main menu / settings | Dark iron modal plate + gold primary buttons | Chrome `#F1EEE8` |
| Quest / toast chips | Semi-opaque dark iron, gold eyebrow; kind tint (main gold / side steel-blue / faction bronze / archetype violet); up to 3 tracked | Chrome body, gold label |
| World quest marks | Floating **`?`** (available) / **`!`** (active turn-in), light bob — PNGs in [`../design/quest-assets/`](../design/quest-assets/) | Gold glyph on iron well |
| Minimap / map pins | Kind-tinted pins + dots + offscreen chevron + player arrow ([DEC-0051](../decisions/index.md#dec-0051-no-xp-power-progression-and-quest-ux) / TICKET-0062) | Main / side / faction / archetype tints |
| World billboards (Press E) | Small parchment chip, soft ink | Ink on parchment |
| Combat floating damage | Bare Cinzel number above NPC name (no plate); chrome fill, dark outline | Chrome `#F1EEE8`; crit gold `#FFE08A` |
| Cartography | Separate map language — [cartography-design.md](cartography-design.md) | Do not merge map parchment into combat HUD |

## Token table (RGBA 0–255)

| Token | RGB | Hex | Use |
| --- | --- | --- | --- |
| Iron deep | `30, 28, 24` | `#1E1C18` | Hotbar cells, bar tracks, modal dim companions |
| Iron panel | `45, 41, 35` | `#2D2923` | Modal plates, quest chip |
| Iron charcoal | `42, 40, 38` | `#2A2826` | Face/minimap outer rings |
| Bronze mid | `58, 52, 44` | `#3A342C` | Inner rings, warm mid fills |
| Bronze face | `92, 78, 58` | `#5C4E3A` | Portrait well / dialogue portrait fill |
| Parchment | `201, 184, 150` | `#C9B896` | Dialogue plate, billboard chips, face label |
| Parchment choice | `232, 220, 198` | `#E8DCC6` | Dialogue choice row fill |
| Gold accent | `213, 185, 120` | `#D5B978` | Primary buttons, hotbar keys, eyebrows, studs |
| Gold stamina | `196, 162, 74` | `#C4A24A` | Stamina bar fill (Ashfell Blade / Outrider) |
| Chrome text | `241, 238, 232` | `#F1EEE8` | Titles, names, chip FG on dark |
| Muted steel | `155, 163, 167` | `#9BA3A7` | Secondary labels (Health, hints) |
| Ink | `72, 62, 48` | `#483E30` | Dialogue body / speaker on parchment |
| Ink muted | `96, 84, 68` | `#605444` | Role / prompt labels on parchment |
| Dim scrim | `21, 23, 25` | `#151719` | Fullscreen modal dim (~0.7 opacity) |
| Health crimson | `139, 46, 46` | `#8B2E2E` | Health bar |
| Magic blue | `70, 90, 160` | `#465AA0` | Runecaster resource bar |
| Standing up | `46, 110, 72` | — | Dialogue standing chip + |
| Standing down | `140, 56, 48` | — | Dialogue standing chip − |
| Standing flat | `90, 84, 76` | — | Dialogue standing chip ~ |

Tone chips (persuade / intimidate / etc.) keep distinct hues for gameplay readability; base chrome around them still uses iron/gold/parchment tokens above. See `dialogue_tone_colors` in `src/dialogue/dialogue_ui.cpp`.

## Shape language

1. **Faceted circles** for face viewport, minimap, dialogue portrait — segmented rings with cardinal stud blocks, not smooth CNC rings.
2. **Hard rectangles** for bars, hotbar slots, choice rows, modal plates — slight bevel ok; no soft iOS pills.
3. **Gold studs / corner blocks** as fasteners on frames (match HUD portrait ring).
4. **Thin gold strokes** on interactive slots and primary CTAs; secondary actions stay iron with chrome text.
5. **Low ornament density** — one gold band or stud cluster per component; no filigree lace.

## Typography (game UI)

- Face: **Cinzel** (SIL OFL) — [visual-direction.md](visual-direction.md).
- Titles / speaker: larger Cinzel; body dialogue ~18–22 design px at 1920×1080.
- Labels: muted steel or ink muted; never gold for long body copy.
- Numerals: prefer compact forms (`100/100`) so letterboxed Game viewports do not clip.

## Do / don't

```text
✅ DO: Dark iron + gold accent + parchment for reading surfaces
✅ DO: Match HUD face-ring language on dialogue portraits
✅ DO: Keep saturated reds/blues for vitals / magic / standing only
✅ DO: Generate concept PNGs with true alpha; runtime may use vector until GPU image widgets ship

❌ DON'T: Purple neon, glassmorphic blur, or white modern cards
❌ DON'T: Photoreal painted portraits as chrome (placeholder FACE / initials until RT)
❌ DON'T: Copy cartography parchment frames into combat HUD
❌ DON'T: Bake long dialogue strings into textures
```

## Authoring sources of truth

| Kind | Path |
| --- | --- |
| Combat HUD layout | `samples/open-world-rpg/assets/ui/player.uicanvas.json` |
| Dialogue layout | `samples/open-world-rpg/assets/ui/dialogue.uicanvas.json` |
| Pencil mocks | `context/design/player-hud.pen`, `dialogue-ui.pen`, `quest-ui.pen`, `inventory-ui.pen`, `rpg-engine-ui.pen` |
| Concept PNGs (HUD) | `context/design/hud-assets/` → `assets/ui/hud/` |
| Concept PNGs (dialogue) | `context/design/dialogue-assets/` → `assets/ui/dialogue/` |
| Runtime theme | `samples/open-world-rpg/assets/ui/ui-theme.json` |
| Runtime draw defaults | `src/ui/hud_runtime.cpp` |

## Production status

Concept PNGs are **prototype chrome** until an owner redraw pass. Runtime loads them via widget `image` fields (`UiTextureCache` / TICKET-0164). Record provenance beside each asset pack (`PROVENANCE.md`). Pencil image fills still ignore PNG alpha — keep vector mocks there.

Shared layout tokens: **40px** safe margin on the 1920×1080 letterbox; regenerate `player.uicanvas.json` / `dialogue.uicanvas.json` with `tools/generate_ui_canvases.py`.
