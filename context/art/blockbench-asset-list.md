# Blockbench Asset List (Open World Slice)

- Status: draft production backlog
- Scope: Act 0 **Landfall** → early Act 1 (**Ledgeport**)
- Style: Unturned-inspired blocky low-poly ([DEC-0006](../decisions/index.md#dec-0006-smooth-low-poly-art-direction)); muted earth tones ([theme-palette.md](theme-palette.md), [visual-direction.md](visual-direction.md))
- Character kits: [character-direction.md](character-direction.md)
- Story drivers: [campaign-beat-sheet.md](../story/campaign-beat-sheet.md), [companions.md](../story/companions.md), [factions.md](../story/factions.md), [side-quest-catalog.md](../story/side-quest-catalog.md)
- Pipeline: source under `tools/art/`; bake/export glTF into `samples/open-world-rpg/assets/models/` (see [mesh-assets.md](../formats/mesh-assets.md))

## Already shipped

Do not remake these first. Refine only when a higher-tier kit needs a shared base.

| Asset | Source | Runtime |
| --- | --- | --- |
| Oak tree + variants (tall / wide / lean / young / asymmetric) | `tools/art/tree/` | `assets/models/tree.gltf`, `oak_*.gltf` |
| Stone cluster | `tools/art/stones/Stones.bbmodel` | `assets/models/stones.gltf` |
| Player body v2 (legacy skinned + Idle/Run/Fall; superseded by GoodPlayerModel) | `tools/art/player/Player_V2_rigged.bbmodel` / `.gltf` | kept for reference; runtime uses GoodPlayerModel bake |
| Player body v3 (WIP — bald Ashfell kit T-pose palms-down; no textures/rig; 2026-07-29) | `tools/art/player/Player_V3.bbmodel` (+ `build_player_v3_ashfell.py`) | — (not baked) |
| GoodPlayerModel (runtime player — skinned 37-bone + Idle/Run/Fall; bake 2026-07-30b) | `tools/art/player/GoodPlayerModel.gltf` / `_rigged.bbmodel` (+ `Documents/Models/GoodPlayerModel.gltf`) | `assets/models/player.gltf` (+ `player.png`); animator `assets/animators/player.animator.json`; NPC test prefab `assets/prefabs/NPC/npc_test.prefab.json` |
| Dead tree | — | `assets/models/dead-tree.gltf` |
| Dead log | `tools/art/dead-log/DeadLog.bbmodel` | `assets/models/dead_log.gltf` |
| Stump | `tools/art/stump/Stump.bbmodel` | `assets/models/stump.gltf` |
| Crate | `tools/art/crate/Crate.gltf` | `assets/models/crate.gltf` |
| Bush (normal) | `tools/art/bush/Bush.gltf` | `assets/models/bush.gltf` |
| Tall bush | `tools/art/tall-bush/Tall_Bush.gltf` | `assets/models/bush_tall.gltf` |
| Campfire | `tools/art/campfire/Campfire_New.gltf` | `assets/models/campfire.gltf` |
| Barrel | `tools/art/barrel/Barrel.gltf` | `assets/models/barrel.gltf` |
| Lantern | `tools/art/lantern/Lantern.gltf` | `assets/models/lantern.gltf` |
| Wall torch | `tools/art/wall-torch/Wall_Torch.gltf` | `assets/models/wall_torch.gltf` |
| Ashfell arming sword | `tools/art/ashfell-arming-sword/Ashfell_Arming_Sword.gltf` | `assets/models/ashfell_arming_sword.gltf` |
| Outrider shortbow | `tools/art/outrider-shortbow/Outrider_Shortbow.gltf` | `assets/models/outrider_shortbow.gltf` + Scene Asset `outrider_shortbow.prefab.json` |
| Outrider arrow | `tools/art/outrider-arrow/Outrider_Arrow.gltf` | `assets/models/outrider_arrow.gltf` + Scene Asset `outrider_arrow.prefab.json` |
| Guild rune focus | `tools/art/guild-rune-focus/Guild_Rune_Focus.gltf` | `assets/models/guild_rune_focus.gltf` + Scene Asset `guild_rune_focus.prefab.json` |
| Loot bag (generic) | `tools/art/loot-bag/Loot_Bag.gltf` (+ `.bbmodel`) | `assets/models/loot_bag.gltf` + Scene Asset `loot_bag.prefab.json` |

Still primitive-composed (candidate to replace): `bush_wide` only. Rebake Tier 1 props with `python tools/bake_tier1_props_gltf.py` (optional name filter, e.g. `barrel lantern wall_torch ashfell_arming_sword outrider_shortbow outrider_arrow guild_rune_focus loot_bag`). Legacy bbmodel loot bake: `python tools/bake_loot_bag_gltf.py` (prefer glTF Tier-1 bake going forward).

## Suggested work order

1. ~~Tier 1 — replace placeholders~~ **done** (except `bush_wide`)
2. Tier 2 — Act 0 Calrenoth props  
3. Tier 3 — characters + weapons  
4. Tier 4 — player camp kit  
5. Tier 5 — Ledgeport / hub  
6. Tier 6 — supernatural / story icons  

---

## Tier 1 — Replace placeholders

Unlock readable world dressing without new story content.

| Asset | Why | Notes |
| --- | --- | --- |
| ~~Bush (normal / tall)~~ | Foliage scatter + scene dressing | **Shipped** (`bush`, `bush_tall`); wide still primitive |
| ~~Campfire (ring + logs)~~ | Camp interaction + warm landmarks | **Shipped** mesh + light; **particle flame later** (`effects_campfire_flame`) |
| ~~Crate / supply crate~~ | Physics prop + loot clutter | **Shipped** (`crate.prefab.json`) |
| ~~Loot bag (generic)~~ | World pickup / approach finds clutter | **Shipped** (`loot_bag.prefab.json`); ~0.45 m |
| ~~Barrel~~ | Town / dock / siege clutter | **Shipped** (`barrel.prefab.json`) |
| ~~Torch / lantern~~ | Night readability, fortress lights | **Shipped** (`wall_torch`, `lantern`) + warm point lights; **particle flame later** |
| ~~Dead log / stump~~ | Road / forest fill | **Both shipped** (`dead_log`, `stump`) |

### Tier 1 concept sheets

Draft Blockbench references under `context/art/` (palette-aligned; not final meshes):

- `tier1-bush-variants-concept.png` — normal / wide / tall silhouettes (keep canopy at shrub height when modeling; sheet reads a bit tree-like)
- `tier1-campfire-concept.png` — stone ring, crossed logs, placeholder flame (+ top view)
- `tier1-crate-barrel-concept.png` — supply crate + barrel orthos / 3⁄4 views
- `tier1-torch-lantern-concept.png` — wall torch variants + hanging lantern
- `tier1-dead-log-stump-concept.png` — fallen log + stump (both authored)

## Tier 2 — Act 0 Calrenoth (Landfall)

Story-critical set dressing and interactables for the siege tutorial.

**Layout targets (in order):** LD perspectives → scene sets → kits — see [concepts/README.md](concepts/README.md).

- LD: `act0-ld-a0-01-prologue-perspective.png`, `act0-ld-a0-02-creation-perspective.png`, `act0-ld-overview-perspective.png`, `act0-ld-a0-03` … `a0-06`, `act0-ld-a0-09-camp-perspective.png`
- Scene sets: `act0-sceneset-a0-01-prologue.png`, `act0-sceneset-a0-02-character-creation.png`, `act0-sceneset-a0-03` … `a0-06`, `act0-sceneset-a0-09-camp.png`
- Legacy mood: `act0-prologue-*`, `act0-a0-02-character-creation.png`, `act0-a0-03` … `act0-a0-09`
- Opening kits: `act0-prologue-cathedral-kit-concept.png` (A0-01; + Tier 6 throne/Shroud), `act0-appearance-courtyard-kit-concept.png` (A0-02)

### Tier 2 concept sheets

| Sheet | Covers |
| --- | --- |
| `tier2-castle-wall-kit-concept.png` | Wall / corner / crenel / tower stub / gatehouse / keep door / walk / stair |
| `tier2-drawbridge-kit-concept.png` | Bridge panel, posts, chain spool, hinges, moat edge |
| `tier2-siege-props-kit-concept.png` | Wheelbarrow (both), cart, barricade, sandbags, catapult, pyre, banner, arrows, ladder/scaffold |
| `tier2-command-clutter-kit-concept.png` | Command table, benches, crate stacks, torches, green shroud pole |
| `act0-appearance-courtyard-kit-concept.png` | Pedestal dais, class mannequin stand, courtyard wall/tower, weapons rack, workbench, torch, Tessera banner (A0-02 appearanceWorld) |

| Asset | Why | Beat / quest | Kit sheet |
| --- | --- | --- | --- |
| Wheelbarrow (upright + overturned) | Arkand intro + cart gag | A0-03, SQ-01 | siege props |
| Wooden cart / supply wagon | Siege roads, rescue clutter | A0-04, SQ-01 | siege props |
| Drawbridge kit (planks, chain spool, gate posts) | Rear defense set piece | A0-06 | drawbridge |
| Castle wall / battlement kit (wall, corner, crenel, tower stub) | Fortress silhouette | A0-04–A0-06 | castle wall |
| Keep door / gate | Entrance beats | A0-04 | castle wall / command |
| Signal pyre / watchtower fire basket | Hill signal quest | SQ-02 | siege props |
| Command table / desk | Grenge / ledger prop | A0-05, SQ-03 | command clutter |
| Pedestal dais + class mannequin stands | Appearance preview stage | A0-02 | appearance courtyard |
| Barricade / sandbags / spike fence | Road blockers | A0-04 | siege props |
| Catapult / siege engine (simple) | Approach backdrop | A0-04 | siege props |
| Arrow bundle / quiver prop | Clutter + combat vibe | | siege props |
| Scaffold / ladder | Vertical fortress routes | A0-06 | siege props / command |

## Tier 3 — Characters and weapons

**Body lock:** every Act 0 **humanoid** reuses **GoodPlayerModel** (`tools/art/player/GoodPlayerModel.gltf` → `assets/models/player.gltf`). No per-character body remakes. Concept `act0-char-*.png` sheets are **costume / kit identity** over that body. Exceptions: Creotar (vision non-human); Underflow orc (non-human kit or later dedicated mesh).

**Heraldry lock:** armored / ranked kits stamp from shipped `assets/ui/cartography/heraldry-*.png` via World Forge `emblemPath` — do not freehand faction logos. Placement, simplify rules, and cast bindings: [character-direction.md § Heraldry on characters and armor](character-direction.md#heraldry-on-characters-and-armor-production-lock). Masters also under `context/art/cartography/`.

### Humanoids / kits

| Asset | Why | Concept sheet | Heraldry |
| --- | --- | --- | --- |
| **GoodPlayerModel** (shared body + rig) | Runtime foundation for player + NPC humanoids | Body is the mesh; kits layer on top | — |
| Starter kit trio | Ashfell mail / Outrider Lodge / Runecaster Guild on GoodPlayerModel — **concepts MVP-locked 2026-08-05**; Blockbench outfits open | `act0-char-{ashfell-blade,outrider,runecaster}.png` + lineup `act0-char-player-archetypes.png` | Draft lane-org stamps (ash-tree / Lodge antler / Guild rune); not sphere `emblemPath` yet |
| Modular slots / gear language | Hair, helm, hand, belt attach reference | `act0-char-modular-body-slots.png` (read against GoodPlayerModel scale) | Reserve chest / shield / banner UV for stamps |
| Arkand | Full-plate **kit** on shared body; personality vs armor | `concepts/act0-char-arkand.png` | `kingdom_tessera` |
| Vanessa | Mage robes kit (Act 1) | (Act 1) | `cristallo` when faction tag applies |
| Tessera soldier / guard | Armor/tabard kit; Grenge forces, Larrell as kit variant | `concepts/act0-char-tessera-soldier.png`, `act0-char-larrell.png` | `kingdom_tessera` |
| Commander Grenge | Rank + **green shroud** kit on shared body | `concepts/act0-char-grenge.png` | `kingdom_tessera` (+ shroud prop) |
| Scout Damius | Leather + cloak scout kit | `concepts/act0-char-damius.png` | Restrained `kingdom_tessera` |
| **Imperium ladder (A0→A3)** | Weak→strong dark crusade kits | `concepts/chaotic-imperium-power-ladder.png` + unit sheets | `chaotic_imperium` |
| Imperium skirmisher (A0 weak) | Levy / road trash | `concepts/act0-char-imperium-skirmisher.png` | `chaotic_imperium` (minimal) |
| Imperium footsoldier (A0 mid) | Siege blockers — **revamped** dark iron + oxblood jagged star (not parade gold) | `concepts/act0-char-imperium-footsoldier.png` | `chaotic_imperium` |
| Imperium siege vanguard (A0 heavy) | Landfall heavy | `concepts/act0-char-imperium-vanguard.png` | `chaotic_imperium` |
| Imperium Act 1 units | Man-at-arms + shade scout | `concepts/act1-char-imperium-units.png` | `chaotic_imperium` |
| Imperium Act 2 units | Blackguard + voidbound | `concepts/act2-char-imperium-units.png` | `chaotic_imperium` |
| Imperium Act 3 units | Hollow elite (+ Luceran compare) | `concepts/act3-char-imperium-units.png` | `chaotic_imperium` |
| Luceran the Hollow | Hollow dark rider apex (A0-07 / A3) — black iron + crimson + void aura | `concepts/act0-char-luceran.png` | `chaotic_imperium` (restrained chest stamp) |
| Creotar (Creo) | Vision instance — **not** GoodPlayerModel body | `concepts/act0-char-creotar.png` | No army heraldry |
| Underflow orc | Non-human enemy — separate if needed | `concepts/act0-char-underflow-orc.png` | `underflow` |
| Thalassar coastal fighter | Act 1 dual-path | (Act 1; still prefer shared humanoid body when human) | `thalassar` |

### Attachable weapons

| Asset | Kit / role |
| --- | --- |
| ~~Ashfell arming sword~~ | Ashfell Blade — **Shipped** mesh + Scene Asset + `handAttach` |
| ~~Outrider shortbow + arrow~~ | Outrider — **Shipped** skinned shortbow (`bow_draw` + handAttach.drawClip) + static arrow Scene Assets; inventory icons `outrider_arrow.png` / `crude_arrow.png` (2026-08-05) |
| ~~Guild rune focus~~ | Runecaster — **Shipped** mesh + Scene Asset + `handAttach` |
| Knight sword / shield | Arkand |
| Orc axe / cleaver | Underflow |
| Spear | Guards |

## Tier 4 — Player camp

Anywhere-camp kit ([DEC-0033](../decisions/index.md#dec-0033-anywhere-player-camp-as-editable-instance-dao-style)).

**Concept sheet:** `tier4-camp-kit-concept.png` (LD: `concepts/act0-ld-a0-09-camp-perspective.png`; scene: `concepts/act0-sceneset-a0-09-camp.png`).

| Asset | Why |
| --- | --- |
| Tent (small / medium) | Camp pitch |
| Bedroll | Rest / sleep staging |
| Camp stool / crate seat | Companion talk staging |
| Cooking pot over fire | Camp life |
| Storage chest | Shared stash visual |
| Banner pole (neutral / Tessera) | Camp identity |
| Campfire ring + flame module | Landmark / rest focal |

## Tier 5 — Ledgeport / Act 1 hub

Modular coastal market kit — variants beat unique hero buildings.

**Concept sheet:** `tier5-ledgeport-hub-kit-concept.png`  
**LD / scenes:** `concepts/act1-ld-overview-ledgeport.png`, `act1-ld-market-perspective.png`, `act1-ld-dock-ferry-perspective.png`, matching `act1-sceneset-*`.

| Asset | Why |
| --- | --- |
| Dock planks + piling | Port silhouette |
| Market stall | Hub services |
| Tavern sign + barrel stack | Social hub |
| Small house A/B (door + window variants) | Street fill |
| Fishing boat / skiff | Harbor flavor |
| Ferry dock bollard / cleat | Ferry to Porto Lucente (D-P0-09) |
| Notice board | Side-quest starts |
| Rope coil / net / crate pile | Port clutter |

## Tier 6 — Story / supernatural

Do after Tier 1–3; reserved saturated accents per [theme-palette.md](theme-palette.md).

**Concept sheets:** `tier6-supernatural-kit-concept.png`, `act0-prologue-cathedral-kit-concept.png`  
**LD / scenes:** A0-01 prologue hall + A0-07/08 + Act 3 `act3-ld-luceran-approach.png`.

| Asset | Why |
| --- | --- |
| Nefarium crystal shard | Legendary resource, corrupted sites |
| Nefarium Shroud (worn + floating prop) | Prologue / climax icon |
| Throne | Prologue Luceran beat |
| Nave column / vault rib / aisle half-arch / gallery rail | A0-01 cathedral shell (prologue kit) |
| Blood-glass panel / pew / aisle carpet runner | A0-01 aisle dressing (prologue kit) |
| Simple shrine / standing stone | Sea of Whispers / Muirthalia / Grakk-Maren sites |
| Corrupted ground prop (jagged crystal outcrop) | Imperium blight landmarks |
| Claritas staff head | Cristallo relic accent (sparse) |

---

## Production rules

- Keep pieces **modular** (wall segments, clothing kits, attachable weapons) so they fit compositional prefabs ([DEC-0008](../decisions/index.md#dec-0008-compositional-prefab-meshes-from-primitives)).
- **Blockbench authoring:** for curved / organic / tapered weapons and props, prefer **mesh** elements over stacked cubes; grow connected shapes with **extrude** then **move vertices/faces/edges** (see `.cursor/rules/blockbench-prefer-meshes.mdc`). Cubes remain fine for boxy props and explicit graybox passes.
- Prefer **palette / vertex color / small atlases** over photoreal textures.
- Favor **strong silhouettes** over dense micro-detail; combat readability first.
- Export **glTF**; place baked runtime under `samples/open-world-rpg/assets/models/`; keep editable `.bbmodel` (or Blockbench glTF source) under `tools/art/<asset>/`.
- Humanoids: shared T-pose, feet at y=0, target height ≈ 1.8 m (match existing player bake).
- Props: feet at y=0; document approximate height in bake notes when adding a new bake script.
- Do not invent new faction visual languages ahead of Dom locks; stick to Act 0–1 named groups (Tessera, Imperium, Thalassar, Underflow).

## Suggested source layout

```text
tools/art/
  bush/
  campfire/
  crate/
  barrel/
  loot-bag/
  ashfell-arming-sword/
  outrider-shortbow/
  outrider-arrow/
  guild-rune-focus/
  torch/
  wheelbarrow/
  cart/
  drawbridge/
  castle-kit/
  camp/
  ledgeport/
  characters/   # kits, companions, enemies
  weapons/
  story/        # shroud, throne, nefarium, shrines
```

Mirror existing conventions: `tools/art/tree/`, `tools/art/stones/`, `tools/art/player/`.

## Open art questions

- Flat shading versus softened normals on characters/props ([visual-direction.md](visual-direction.md)).
- ~~Whether starting kits share one base body with swaps or use per-archetype bodies~~ — **locked:** **GoodPlayerModel** body + kit swaps for Act 0 humanoids (`character-direction.md`).
- Ferry dock asset only after Dom locks ferry yes/no (D-P0-09).
