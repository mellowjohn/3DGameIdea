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
| Player body v2 (skinned + Idle; finger/thumb bones) | `tools/art/player/Player_V2_rigged.bbmodel` | `assets/models/player.gltf` (+ `player.png`); NPC test prefab `assets/prefabs/NPC/npc_test.prefab.json` |
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

Still primitive-composed (candidate to replace): `bush_wide` only. Rebake Tier 1 props with `python tools/bake_tier1_props_gltf.py` (optional name filter, e.g. `barrel lantern wall_torch`).

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

Story-critical set dressing and interactables for the siege tutorial. Scene mood / layout targets: [concepts/README.md](concepts/README.md) (`act0-a0-03` … `act0-a0-09`).

| Asset | Why | Beat / quest |
| --- | --- | --- |
| Wheelbarrow (upright + overturned) | Arkand intro + cart gag | A0-03, SQ-01 |
| Wooden cart / supply wagon | Siege roads, rescue clutter | A0-04, SQ-01 |
| Drawbridge kit (planks, chain spool, gate posts) | Rear defense set piece | A0-06 |
| Castle wall / battlement kit (wall, corner, crenel, tower stub) | Fortress silhouette | A0-04–A0-06 |
| Keep door / gate | Entrance beats | A0-04 |
| Signal pyre / watchtower fire basket | Hill signal quest | SQ-02 |
| Command table / desk | Grenge / ledger prop | A0-05, SQ-03 |
| Barricade / sandbags / spike fence | Road blockers | A0-04 |
| Catapult / siege engine (simple) | Approach backdrop | A0-04 |
| Arrow bundle / quiver prop | Clutter + combat vibe | |
| Scaffold / ladder | Vertical fortress routes | A0-06 |

## Tier 3 — Characters and weapons

Prefer one shared body + kit swaps over per-archetype body remakes. Rig in T-pose for later retargeting.

### Humanoids / kits

| Asset | Why |
| --- | --- |
| Player base body (clean T-pose, modular slots) | Character creation foundation — match approved front `reference/player-base-body-front.png` |
| Ashfell Blade starter kit | Tunic, wraps, belt/pouch, boots — match [character-direction.md](character-direction.md) (legacy Squire) |
| Outrider starter kit | Cloak/hood, bracer, quiver attachment — concept still open |
| Runecaster starter kit | Rune/sigil caster kit (focus + inscribed accents) — concept still open; not crystal-warden default |
| Arkand | Full-plate knight; goofy personality vs imposing armor |
| Vanessa | Mage robes, academy look |
| Generic Tessera soldier / guard | Grenge’s forces, Pellin, Larrell |
| Commander Grenge variant | Plate + green shroud accent |
| Imperium footsoldier | Act 0 blockers |
| Underflow orc | Local warband enemy (`underflow`) |
| Thalassar coastal fighter | Act 1 dual-path neighbor (`thalassar`) |

### Attachable weapons

| Asset | Kit / role |
| --- | --- |
| Short sword / arming sword | Ashfell Blade |
| Bow + arrow | Outrider |
| Rune focus (staff/rod or inscribed token) | Runecaster |
| Knight sword / shield | Arkand |
| Orc axe / cleaver | Underflow |
| Spear | Guards |

## Tier 4 — Player camp

Anywhere-camp kit ([DEC-0033](../decisions/index.md#dec-0033-anywhere-player-camp-as-editable-instance-dao-style)).

| Asset | Why |
| --- | --- |
| Tent (small / medium) | Camp pitch |
| Bedroll | Rest / sleep staging |
| Camp stool / crate seat | Companion talk staging |
| Cooking pot over fire | Camp life |
| Storage chest | Shared stash visual |
| Banner pole (neutral / Tessera) | Camp identity |

## Tier 5 — Ledgeport / Act 1 hub

Modular coastal market kit — variants beat unique hero buildings.

| Asset | Why |
| --- | --- |
| Dock planks + piling | Port silhouette |
| Market stall | Hub services |
| Tavern sign + barrel stack | Social hub |
| Small house A/B (door + window variants) | Street fill |
| Fishing boat / skiff | Harbor flavor |
| Ferry dock bollard / cleat | If ferry path locks (Dom D-P0-09) |
| Notice board | Side-quest starts |
| Rope coil / net / crate pile | Port clutter |

## Tier 6 — Story / supernatural

Do after Tier 1–3; reserved saturated accents per [theme-palette.md](theme-palette.md).

| Asset | Why |
| --- | --- |
| Nefarium crystal shard | Legendary resource, corrupted sites |
| Nefarium Shroud (worn + floating prop) | Prologue / climax icon |
| Throne | Prologue Luceran beat |
| Simple shrine / standing stone | Sea of Whispers / Muirthalia / Grakk-Maren sites |
| Corrupted ground prop (jagged crystal outcrop) | Imperium blight landmarks |

---

## Production rules

- Keep pieces **modular** (wall segments, clothing kits, attachable weapons) so they fit compositional prefabs ([DEC-0008](../decisions/index.md#dec-0008-compositional-prefab-meshes-from-primitives)).
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

- Outrider and Runecaster starter-kit turnaround references ([character-direction.md](character-direction.md)).
- Flat shading versus softened normals on characters/props ([visual-direction.md](visual-direction.md)).
- Whether starting kits share one base body with swaps or use per-archetype bodies.
- Ferry dock asset only after Dom locks ferry yes/no (D-P0-09).
