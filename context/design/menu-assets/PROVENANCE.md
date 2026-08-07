# Main Menu / Character Creation Concept Assets — Provenance

Status: prototype chrome + stained-glass art for Act 0 menu / creation Pencil mocks (`act0-menu-creation-ui.pen`) — **v3 glass redesign**

## Source

AI-generated concept art (Cursor image generation), prompted for:

- **Layout roles:** menu panel frame, title plate, primary/secondary buttons, archetype card, lane icons (v1–v2)
- **v3 stained glass:** tall gothic lane cards, difficulty trial cards, circular glass icons
- **Art style (chrome):** same in-game UI language as combat HUD / dialogue — low-poly / Unturned-adjacent blocky iron + gold ([ui-chrome-direction.md](../../art/ui-chrome-direction.md))
- **Art style (glass):** leaded jewel-tone windows (Dragon Age–adjacent class-wall read); not photoreal
- **Title logo:** owner-provided Wrathful Conquest mark (v1, chewed edges in source RGB); **v2 regenerated 2026-08-05** as a solid iron+ember mark keyed from a navy backdrop; **v3 regenerated 2026-08-06** on pure chroma `#00FF00` then soft green-key → true RGBA (no luminance punch)

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
| `glass-card-outrider.png` | v3 class wall card — **silhouette/scale regen 2026-08-07** (Ashfell-matched alpha can; composition pass v4) |
| `glass-card-outrider-v4.png` | Runtime canvas path for matched Outrider card (cache-bust) |
| `glass-card-runecaster.png` | v3 class wall card — **silhouette/scale regen 2026-08-07** (Ashfell-matched alpha can; composition pass v4) |
| `glass-card-runecaster-v4.png` | Runtime canvas path for matched Runecaster card (cache-bust) |
| `glass-card-ashens-levy.png` | v3 difficulty — Ashen's Levy (Normal); **regen + Ashfell can 2026-08-07** |
| `glass-card-ashens-levy-v3.png` | Runtime canvas path for regenerated Ashen's Levy (cache-bust) |
| `glass-card-calrenoth-breach.png` | v3 difficulty — Calrenoth Breach (Hard); **regen + Ashfell can 2026-08-07** |
| `glass-card-calrenoth-breach-v3.png` | Runtime canvas path for regenerated Calrenoth Breach (cache-bust) |
| `glass-card-frangiturs-claim.png` | v3 difficulty — Frangitur's Claim (Nightmare); **regen + Ashfell can 2026-08-07** |
| `glass-card-frangiturs-claim-v3.png` | Runtime canvas path for regenerated Frangitur's Claim (cache-bust) |
| `wrathful-conquest-title-logo.png` | Game title — true transparent PNG (v3 chroma-key regen) |
| `glass/raw/wrathful-conquest-title-logo-v3-green.png` | Unkeyed green-screen source for v3 title logo |
| `wrathful-conquest-title-on-menu.png` | Title composited onto menu backdrop crop |
| `wrathful-conquest-title-logo-screen.png` | Opaque black-backed title for blend experiments |
| `glass/glass-menu-panel-frame.png` | Runtime menu panel — opaque stained-glass plate (no chroma key) |
| `glass/glass-menu-btn-primary.png` | Runtime primary — ember glass **capsule** (regen+chroma 2026-08-07; resampled ~385×140 for nine-slice) |
| `glass/glass-menu-btn-secondary.png` | Runtime secondary — slate glass **capsule** (same pass) |
| `glass/glass-menu-btn-primary-pill.png` | Opening-flow primary path (cache-bust; preferred) |
| `glass/glass-menu-btn-secondary-pill.png` | Opening-flow secondary path (cache-bust; preferred) |
| `glass/glass-menu-btn-primary-round.png` | Intermediate pill twin (same pixels as primary) |
| `glass/glass-menu-btn-secondary-round.png` | Intermediate pill twin (same pixels as secondary) |
| `glass/raw/regen-glass-btn-primary-round.png` | Unkeyed green-screen source for primary pill |
| `glass/raw/regen-glass-btn-secondary-round.png` | Unkeyed green-screen source for secondary pill |
| `glass/glass-menu-title-plate.png` | Runtime title banner — glass plate |
| `glass/raw/*.png` | Untrimmed Cursor generations for the plates above |
| `creation-glass-card-select-glow.png` | Class-select rim — gold/ember glow derived from glass-card alpha silhouette (fits can / tombstone arch; true RGBA) |

## Difficulty naming (mock lock 2026-07-29)

| Tier | Display name | Theme |
| --- | --- | --- |
| Normal | **Ashen's Levy** | War draft · fair steel · free saves |
| Hard | **Calrenoth Breach** | Siege fire · scarce kit · veteran foes |
| Nightmare | **Frangitur's Claim** | Permadeath · hollow throne · Nefarium wake |

## Pencil mattes (`pencil-matte/`)

Pencil **image fills do not composite PNG alpha**. In-panel chrome and glass cards use iron/scrim composites (or charcoal-backed card art). The title on the main menu uses `wrathful-conquest-title-on-menu.png` so no dark rectangle appears over the siege shot.

## Post-process

- `_polish_menu_v2.py` — green chroma-key → alpha, trim, pencil-matte, title backdrop composite (v2 chrome; superseded for runtime)
- `_polish_menu_v2.py --reprocess-existing` — despill leftover green-dominant RGB on already-keyed chrome; use only when green-screen v2 sources are the only option
- `_punch_title_logo.py` — title-logo border flood + letter counters (initial pass; can chew dark iron) — **superseded by regen+chroma for v3**
- `_repair_title_logo.py` — restore letter interiors after punch, keep counters ≥400px, drop keying crumbs <3k px — legacy for punched copies
- `_chroma_key_regen_glass.py` — preferred path for overlays: regen on `#00FF00` → soft green key → RGBA (title logo v3 used the same key/despill)
- `_prepare_glass_chrome.py` — trim letterbox, force opaque alpha, resample stained-glass plates into `glass/` + `samples/.../assets/ui/menu/`
- Glass icons: corner flood + chroma green key onto iron matte for Pencil
- Runtime sample copies live under `samples/open-world-rpg/assets/ui/menu/` (glass plates + repaired logo). Prefer glass plates over v2 green-keyed chrome — that RGB cannot be despilled cleanly.

## Backdrops (concepts)

| File | Screen |
| --- | --- |
| `../art/concepts/act0-main-menu.png` | Main menu establishing shot |
| `../art/concepts/act0-a0-02-character-creation.png` | Appearance pedestal courtyard (legacy mood) |
| `../art/concepts/act0-ld-a0-02-creation-perspective.png` | A0-02 LD + modular callouts |
| `../art/concepts/act0-sceneset-a0-02-character-creation.png` | A0-02 scene set |
| `../art/act0-appearance-courtyard-kit-concept.png` | A0-02 modular kit sheet |
| `../art/concepts/act0-class-cathedral-glass-wall.png` | Class select — cathedral stained-glass wall (extended black, thin stone border) |
| `../art/concepts/act0-difficulty-glass-landscape.png` | Difficulty select — stained-glass landscape mural |
| `../art/concepts/act0-prologue-01-throne-pullback.png` | Prologue carousel still 1 — throne pullback (cinematic entrance) |
| `../art/concepts/act0-prologue-02-whisper-luceran.png` | Prologue carousel still 2 — Luceran alone whisper (regen) |
| `../art/concepts/act0-prologue-03-address-flame.png` | Prologue carousel still 3 — address adventurers / flame |
| `../art/concepts/act0-ld-a0-01-prologue-perspective.png` | A0-01 LD + modular callouts |
| `../art/concepts/act0-sceneset-a0-01-prologue.png` | A0-01 scene set |
| `../art/act0-prologue-cathedral-kit-concept.png` | A0-01 cathedral modular kit (pairs with Tier 6 throne/Shroud) |
| `../art/concepts/act0-a0-03-approach-arkand.png` | Into Landfall |

**Do not** reuse Act beat shots (`act0-a0-07`, `act0-a0-08`, siege menu) as prologue carousel stills — those are later Landfall beats, not A0-01 prologue concepts.

### Prologue 2D FX overlays (2026-08-06)

Project-owned procedural RGBA plates under `samples/open-world-rpg/assets/ui/menu/prologue/` (true alpha; soft Gaussian dots / radial vignette). Driven at runtime by Ken Burns + opacity pulse in the editor opening boot path:

| File | Role |
| --- | --- |
| `prologue-fx-vignette.png` | Soft edge vignette over stills |
| `prologue-fx-dust.png` | Dust motes (beats 1–3; drifts) |
| `prologue-fx-embers.png` | Ember/ash scatter (flame beat) |

## Appearance courtyard chrome (2026-08-07)

Regen on `#00FF00` → chroma key → runtime:

| File | Role |
| --- | --- |
| `glass/raw/regen-appearance-panel-well-v2.png` | Unkeyed source — gothic arch + opaque iron well + parchment foot band |
| `creation-glass-appearance-panel-v2.png` (design) / `samples/.../creation/creation-glass-appearance-panel-v2.png` | Runtime panel (replaces hollow frame-only plate) |
| `glass/raw/regen-appearance-option-row.png` | Unkeyed option-row plate |
| `samples/.../creation/creation-glass-appearance-row.png` | Keyed row chrome (optional; cycle buttons currently use `glass-menu-btn-secondary`) |
| `samples/.../creation/glass-icon-*.png` | Class medallions copied for appearance summary |

Appearance canvas keeps the 3D courtyard (underlay/backdrop hidden); soft dim + solid panel well for readable copy.

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
