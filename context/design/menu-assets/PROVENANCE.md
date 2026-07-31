# Main Menu / Character Creation Concept Assets — Provenance

Status: prototype chrome + stained-glass art for Act 0 menu / creation Pencil mocks (`act0-menu-creation-ui.pen`) — **v3 glass redesign**

## Source

AI-generated concept art (Cursor image generation), prompted for:

- **Layout roles:** menu panel frame, title plate, primary/secondary buttons, archetype card, lane icons (v1–v2)
- **v3 stained glass:** tall gothic lane cards, difficulty trial cards, circular glass icons
- **Art style (chrome):** same in-game UI language as combat HUD / dialogue — low-poly / Unturned-adjacent blocky iron + gold ([ui-chrome-direction.md](../../art/ui-chrome-direction.md))
- **Art style (glass):** leaded jewel-tone windows (Dragon Age–adjacent class-wall read); not photoreal
- **Title logo:** owner-provided Wrathful Conquest mark; black backdrop punched to true alpha via `_punch_title_logo.py`

## Files (true alpha / export)

| File | Role |
| --- | --- |
| `menu-panel-frame.png` | Tall iron modal panel (v2 — legacy; mocks prefer vector iron+gold) |
| `menu-title-plate.png` | Horizontal title banner chrome (v2) |
| `menu-btn-primary.png` | Gold primary CTA (v2) |
| `menu-btn-secondary.png` | Iron secondary button (v2) |
| `menu-archetype-card.png` | Archetype row card (v2) |
| `icon-ashfell-blade.png` | Lane icon — Ashfell Blade (v2, superseded by glass icons) |
| `icon-outrider.png` | Lane icon — Outrider (v2) |
| `icon-runecaster.png` | Lane icon — Runecaster (v2) |
| `glass-icon-ashfell-blade.png` | v3 stained-glass medallion — Ashfell Blade |
| `glass-icon-outrider.png` | v3 stained-glass medallion — Outrider |
| `glass-icon-runecaster.png` | v3 stained-glass medallion — Runecaster |
| `glass-card-ashfell-blade.png` | v3 class wall card — Ashfell Blade (**sword-over-tabard fix 2026-07-29**; also `glass-card-ashfell-blade-v2.png` / pencil-matte twin) |
| `glass-card-outrider.png` | v3 class wall card |
| `glass-card-runecaster.png` | v3 class wall card |
| `glass-card-ashens-levy.png` | v3 difficulty — Ashen's Levy (Normal) |
| `glass-card-calrenoth-breach.png` | v3 difficulty — Calrenoth Breach (Hard) |
| `glass-card-frangiturs-claim.png` | v3 difficulty — Frangitur's Claim (Nightmare / permadeath) |
| `wrathful-conquest-title-logo.png` | Game title — true transparent PNG |
| `wrathful-conquest-title-on-menu.png` | Title composited onto menu backdrop crop |
| `wrathful-conquest-title-logo-screen.png` | Opaque black-backed title for blend experiments |

## Difficulty naming (mock lock 2026-07-29)

| Tier | Display name | Theme |
| --- | --- | --- |
| Normal | **Ashen's Levy** | War draft · fair steel · free saves |
| Hard | **Calrenoth Breach** | Siege fire · scarce kit · veteran foes |
| Nightmare | **Frangitur's Claim** | Permadeath · hollow throne · Nefarium wake |

## Pencil mattes (`pencil-matte/`)

Pencil **image fills do not composite PNG alpha**. In-panel chrome and glass cards use iron/scrim composites (or charcoal-backed card art). The title on the main menu uses `wrathful-conquest-title-on-menu.png` so no dark rectangle appears over the siege shot.

## Post-process

- `_polish_menu_v2.py` — green chroma-key → alpha, trim, pencil-matte, title backdrop composite (v2 chrome)
- `_punch_title_logo.py` — title-logo border flood + letter counters
- Glass icons: corner flood + chroma green key onto iron matte for Pencil

## Backdrops (concepts)

| File | Screen |
| --- | --- |
| `../art/concepts/act0-main-menu.png` | Main menu establishing shot |
| `../art/concepts/act0-a0-02-character-creation.png` | Appearance pedestal courtyard |
| `../art/concepts/act0-class-cathedral-glass-wall.png` | Class select — cathedral stained-glass wall (extended black, thin stone border) |
| `../art/concepts/act0-difficulty-glass-landscape.png` | Difficulty select — stained-glass landscape mural |
| `../art/concepts/act0-prologue-01-throne-pullback.png` | Prologue carousel still 1 — throne pullback (cinematic entrance) |
| `../art/concepts/act0-prologue-02-whisper-luceran.png` | Prologue carousel still 2 — Luceran alone whisper (regen) |
| `../art/concepts/act0-prologue-03-address-flame.png` | Prologue carousel still 3 — address adventurers / flame |
| `../art/concepts/act0-a0-03-approach-arkand.png` | Into Landfall |

**Do not** reuse Act beat shots (`act0-a0-07`, `act0-a0-08`, siege menu) as prologue carousel stills — those are later Landfall beats, not A0-01 prologue concepts.

## Review pass notes (2026-07-29 UI/UX)

- Title on main menu / settings uses `wrathful-conquest-title-logo-screen.png` with Pencil **`blendMode: screen`** so the black matte drops out (true alpha still does not composite in Pencil image fills).
- Menu primary/secondary buttons shrunk (~52px) so they sit inside the panel instead of overflowing.
- Class / difficulty glass cards: near-black panel + thin gray border; selected state uses gold + ember glow (not a yellow hotspot box).
- Class / difficulty **screen dims lightened** (~40% black) so cathedral / landscape stained-glass backdrops read behind the cards.
- Appearance: circular stained-glass class medallion on panel summary + pedestal badge.
- Canvas zoned: **APPROVED** (green) · **NEEDS REVIEW** (amber) · **REFERENCE** (blue) · **LEGACY** (red).
- Sticker sheet organized: UI tokens → lane colors → difficulty tiers → chrome → glass → backdrops.

## License / use

Generated / owner title art for internal design and prototype mocks. Do not treat as final production art until an owner redraw pass. Confirm redistribution terms before shipping in a commercial build.
