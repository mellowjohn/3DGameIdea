# Scene / Faction Concept Art

Draft visual targets for production. Not runtime assets.

**Interactive board:** open the Cursor canvas `level-design-concepts.canvas.tsx` beside chat (catalog + filters + previews).

**Private web gallery:** password-gated collaborator browser for concepts, UI, fonts, cartography, kits, and runtime glTF models lives under [`gallery/`](../../../gallery/) (Vercel deploy; see that README).

**Production pipeline:**

1. **LD perspectives** — elevated 3⁄4 layout views (path, chokes, modular massing).
2. **Scene sets** — player-readable compositions / mood.
3. **Modular kits** — Blockbench piece sheets → bake/import.

Style: [visual-direction.md](../visual-direction.md), [theme-palette.md](../theme-palette.md). Beats: [campaign-beat-sheet.md](../../story/campaign-beat-sheet.md). Prop backlog: [blockbench-asset-list.md](../blockbench-asset-list.md).

---

## Act 0 — Landfall

### LD perspectives

| File | Beat | Notes |
| --- | --- | --- |
| `act0-ld-overview-perspective.png` | A0-03→A0-06 | Full corridor overview |
| `act0-ld-a0-03-approach-perspective.png` | A0-03 | Road + wheelbarrow staging |
| `act0-ld-a0-04-gate-perspective.png` | A0-04 | Combat corridor + gate |
| `act0-ld-a0-05-courtyard-perspective.png` | A0-05 | Command courtyard |
| `act0-ld-a0-06-drawbridge-perspective.png` | A0-06 | Moat drawbridge + land spur |
| `act0-ld-a0-07-luceran-perspective.png` | A0-07 | Soft-gated climax massing |
| `act0-ld-a0-08-creotar-perspective.png` | A0-08 | Vision instance staging |
| `act0-ld-a0-09-camp-perspective.png` | A0-09 | Travel-only camp layout |

### Scene sets

| File | Beat |
| --- | --- |
| `act0-sceneset-a0-03-approach.png` | A0-03 |
| `act0-sceneset-a0-04-gate.png` | A0-04 |
| `act0-sceneset-a0-05-courtyard.png` | A0-05 |
| `act0-sceneset-a0-06-drawbridge.png` | A0-06 |
| `act0-sceneset-a0-07-luceran.png` | A0-07 |
| `act0-sceneset-a0-08-creotar.png` | A0-08 |
| `act0-sceneset-a0-09-camp.png` | A0-09 |

### Legacy beat / menu stills

| File | Role |
| --- | --- |
| `act0-main-menu.png` | Menu siege establishing |
| `wrathful-conquest-title-logo.png` | Title logo |
| `act0-a0-02-character-creation.png` | Creation courtyard |
| `act0-class-cathedral-glass-wall.png` | Class select backdrop |
| `act0-difficulty-glass-landscape.png` | Difficulty backdrop |
| `act0-prologue-01-throne-pullback.png` | Prologue 1 |
| `act0-prologue-02-whisper-luceran.png` | Prologue 2 |
| `act0-prologue-03-address-flame.png` | Prologue 3 |
| `act0-a0-03-approach-arkand.png` … `act0-a0-09-survivor-camp.png` | Prior beat mood shots |

---

## Act 1 — Ledgeport hub

| File | Layer | Notes |
| --- | --- | --- |
| `act1-ld-overview-ledgeport.png` | LD | Free-town overview: entry, market, tavern, docks, ferry |
| `act1-ld-market-perspective.png` | LD | Market street spine |
| `act1-ld-dock-ferry-perspective.png` | LD | Dock + ferry toward Cristallo U-bay |
| `act1-sceneset-market.png` | Scene | Market composition |
| `act1-sceneset-dock-ferry.png` | Scene | Ferry dock composition |

---

## Act 2 — Faction theaters (draft)

| File | Layer | Notes |
| --- | --- | --- |
| `act2-ld-cristallo-theater.png` | LD | Central island + U-bay / Claritas seat massing |
| `act2-ld-thalassar-coast.png` | LD | Thalassar coastal dual-path |
| `act2-sceneset-cristallo-harbor.png` | Scene | Porto Lucente harbor mood |

---

## Act 3 — Usurper approach (draft)

| File | Layer | Notes |
| --- | --- | --- |
| `act3-ld-luceran-approach.png` | LD | Soft-gated throne approach + corruption massing |
| `act3-sceneset-luceran-approach.png` | Scene | Approach corridor composition |

Act 4 endings remain open in the beat sheet — no level kits until locks firm up.

---

## Modular kit sheets (`context/art/`)

| File | Tier | Use |
| --- | --- | --- |
| `tier1-bush-variants-concept.png` | 1 | Foliage (shipped) |
| `tier1-campfire-concept.png` | 1 | Campfire (shipped) |
| `tier1-crate-barrel-concept.png` | 1 | Crate / barrel (shipped) |
| `tier1-torch-lantern-concept.png` | 1 | Torch / lantern (shipped) |
| `tier1-dead-log-stump-concept.png` | 1 | Log / stump (shipped) |
| `tier2-castle-wall-kit-concept.png` | 2 | Fortress modular shell |
| `tier2-drawbridge-kit-concept.png` | 2 | Drawbridge + spool |
| `tier2-siege-props-kit-concept.png` | 2 | Approach / combat props |
| `tier2-command-clutter-kit-concept.png` | 2 | Courtyard clutter |
| `tier4-camp-kit-concept.png` | 4 | Player camp |
| `tier5-ledgeport-hub-kit-concept.png` | 5 | Dock / market / house modules |
| `tier6-supernatural-kit-concept.png` | 6 | Shroud / crystal / throne / blight |

---

## Faction / characters / items

| File | Notes |
| --- | --- |
| `chaotic-imperium-soldiers-concept.png` | Imperium identity poster (not bake density) |
| `../reference/starting-player-*-turnaround.png` | Starter kit turnarounds |
| `starter-ashfell-arming-sword.png` / shortbow / rune-focus | Starter weapons |
| `act0-field-bandage.png` / scrap pouch / vein pendant | Act 0 item concepts |

### Act 0 humanoids (production)

**Body lock:** kits over **GoodPlayerModel**. Latest regen keeps that mannequin but restores **fuller plate kits + knight helm family** (closed/open/crest/star variants) for Tessera / Imperium / Arkand / Grenge / Larrell / Luceran. Starters stay cloth kits; modular sheet catalogs helm attaches.

**Heraldry lock:** plate / soldier / warband kits use shipped cartography emblems (`assets/ui/cartography/heraldry-*.png`, World Forge `emblemPath`) — simplified stamps on chest, shield, banner, helm — not freehand logos. Full rules: [character-direction.md](../character-direction.md#heraldry-on-characters-and-armor-production-lock).

Canvas filter: **Characters**.

| File | Role | Beat |
| --- | --- | --- |
| `act0-char-player-archetypes.png` | Ashfell / Outrider / Runecaster lineup — lane-org heraldry + Ashfell mail (2026-08-03) | A0-02 |
| `act0-char-ashfell-blade.png` | Ashfell starter — charcoal mail, pauldrons, greaves, ash-tree house stamp | A0-02 |
| `act0-char-outrider.png` | Outrider Lodge scout — forest-green cloak, bronze antler/branch badge | A0-02 |
| `act0-char-runecaster.png` | Runecaster Guild — navy wrap-coat, gold rune seal (not Cristallo) | A0-02 |
| `act0-char-modular-body-slots.png` | Shared base + attach slots | Production |
| `act0-char-arkand.png` | Companion knight | A0-03+ |
| `act0-char-grenge.png` | Commander + green shroud | A0-05 |
| `act0-char-damius.png` | Tessera scout — leather + cloak + restrained Tessera star (2026-08-03) | A0-05 |
| `act0-char-larrell.png` | Sergeant (optional hostage) | A0-06/07 |
| `act0-char-tessera-soldier.png` | Generic Tessera kit | A0-04–06 |
| `act0-char-imperium-footsoldier.png` | Imperium footsoldier (A0 mid) — dark iron + oxblood jagged star | A0-04 |
| `act0-char-imperium-skirmisher.png` | Imperium skirmisher (A0 weak) | A0-04 |
| `act0-char-imperium-vanguard.png` | Imperium siege vanguard (A0 heavy) | A0-04–06 |
| `act1-char-imperium-units.png` | Man-at-arms + shade scout | A1 |
| `act2-char-imperium-units.png` | Blackguard + voidbound | A2 |
| `act3-char-imperium-units.png` | Hollow elite (+ Luceran apex compare) | A3 |
| `chaotic-imperium-power-ladder.png` | Full weak→strong Imperium ladder overview | A0–A3 |
| `act0-char-underflow-orc.png` | Underflow warband — add clan banner / water emblem / corruption variants (TICKET-0255) | Vicinity |
| `act0-char-luceran.png` | Luceran the Hollow — dark iron / Nefarium crimson / void aura (not cream pale kit) | A0-07 |
| `act0-char-creotar.png` | Creotar vision being — production = tesseract mirage pulse (D-P2-18 / TICKET-0256) | A0-08 |

Imperium palette family locks in [character-direction.md § Chaotic Imperium unit ladder](../character-direction.md#chaotic-imperium-unit-ladder-a0a3).

## Usage

1. Block terrain from **LD** sheets.
2. Dress props to **scene sets**.
3. Author **kits** (armor/cloth/hair/weapons) in Blockbench against **GoodPlayerModel**, not alternate bodies.
4. Bake kits / reimport GoodPlayerModel via **Import Model…** into `samples/open-world-rpg`.

Geography locks: western peninsular Calrenoth, landlocked entrance, moat-scale drawbridge to land spur, Act 1 hub **Ledgeport**, ferry to **Porto Lucente**.
