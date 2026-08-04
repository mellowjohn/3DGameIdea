# Mesh Asset Import

The runtime mesh path accepts glTF 2.0 `.gltf` and `.glb` assets through fastgltf 0.9.0. Project validation imports every discovered mesh so malformed geometry fails before an editor session starts.

## Current contract

- Triangle primitives with finite `POSITION` data are required.
- Optional `COLOR_0` (`FLOAT` `VEC3` or `VEC4`) is imported into runtime vertex RGB when present; otherwise imported meshes use a default brown fallback.
- Optional `TEXCOORD_0` (`FLOAT` `VEC2`) is imported into per-vertex `u,v` (glTF top-left convention, sampled with V as-is); absent UVs default to `(0,0)`. See TICKET-0191.
- Optional base-color texture: the first used material's `pbrMetallicRoughness.baseColorTexture` image is decoded (WIC) into engine-owned RGBA8 pixels on `ImportedMesh` (`albedo_rgba`, `albedo_width/height`, `has_albedo()`). Embedded `data:image/png;base64,` payloads and external PNG files (relative to the glTF) are both supported; the importer requests `LoadExternalImages`. Only the base-color map is imported (no normal/metallic-roughness/emissive maps yet).
- The editor/runtime opaque mesh pipeline samples `albedo` with a point/clamp static sampler (pixel-art friendly) when a mesh has a texture, and falls back to vertex color (`COLOR_0` or the brown default) otherwise. Primitive-generated meshes carry no UVs or textures.
- Malformed UVs return structured errors: `MESH-UV-COUNT` (count ≠ POSITION), `MESH-UV-TYPE` (not `FLOAT` `VEC2`), `MESH-UV-NONFINITE`. Texture decode failures surface as `MESH-TEXTURE-*` / `PNG-*` asset-import errors.
- Indexed and non-indexed primitives are expanded into deterministic runtime triangle lists.
- Missing positions, unsupported primitive modes, out-of-range indices, non-triangle index counts, empty geometry, and excessive vertex counts return structured asset-import errors.
- Prefabs select a mesh using a project-relative top-level `mesh` field, such as `"mesh": "assets/models/dead-tree.gltf"`.
- [DEC-0008](../decisions/index.md#dec-0008-compositional-prefab-meshes-from-primitives) adds planned v2 support for multi-part prefabs composed from built-in primitives and/or imported meshes. See `context/formats/prefab-assets.md`.
- Optional prefab `light` blocks describe warm local point lights for atmosphere tests. See `context/formats/prefab-assets.md`.
- Prefab dependency metadata must list that mesh path so registry validation and incremental rebuilds track the relationship.
- The editor discovers all glTF/GLB assets, uploads each mesh into the shared D3D12 geometry buffer, and selects the correct range for each placed prefab.
- A missing or unresolved prefab mesh continues to use the diagnostic box proxy rather than corrupting world state.

## Skeletal / skin subset (TICKET-0101)

The importer also reads a documented glTF skinning subset into engine-owned structures on `ImportedMesh`. This proves the M5 skeletal import path. Animation clips are a separate import: see [`animation-clip-assets.md`](animation-clip-assets.md) (TICKET-0102). Play-test **GPU LBS skinning** (TICKET-0227 / [DEC-0047](../decisions/index.md#dec-0047-frame-upload-ring-and-gpu-lbs-skinning)) uploads bind-pose JOINTS/WEIGHTS into the shared VB and skins in the lit/shadow/prop VS from a CPU-built bone palette (`MAX_BONES = 64`, **16 aligned slots** per frame ring). Each play-test entity with an `animator` component gets its own animator instance and slot upload (`skin_entity_id` on expand); draws batch per mesh+entity so poses stay independent. Catalog-wide non-play paths remain follow-on.

### Supported

- `skins[]`: non-empty `joints` (node indices), optional `name`, optional `skeleton` root node, optional `inverseBindMatrices` (`FLOAT` `MAT4`, one matrix per joint; omitted → identity matrices).
- Joint display names are copied from the referenced node `name` fields (may be empty).
- Inverse-bind matrices are stored column-major as 16 floats per joint.
- Per-vertex `JOINTS_0` + `WEIGHTS_0` when both are present: `JOINTS_0` is `UNSIGNED_BYTE` or `UNSIGNED_SHORT` `VEC4`; `WEIGHTS_0` is `FLOAT` `VEC4`. Counts must match `POSITION`.
- Influences expand with the triangle list so `influences.size() == vertices.size()` when skinning attributes are present.
- Static meshes without `skins` / `JOINTS_0` / `WEIGHTS_0` continue to import unchanged (`has_skinning()` is false).

### Rejected (structured errors)

| Code | Condition |
| --- | --- |
| `MESH-SKIN-EMPTY` | Skin with no joints |
| `MESH-SKIN-JOINT-RANGE` | Joint node index out of range |
| `MESH-SKIN-SKELETON-RANGE` | `skeleton` node out of range |
| `MESH-SKIN-IBM-MISSING` / `MESH-SKIN-IBM-TYPE` / `MESH-SKIN-IBM-COUNT` / `MESH-SKIN-IBM-NONFINITE` | Invalid inverse-bind accessor |
| `MESH-SKIN-ATTR-PAIR` | Only one of `JOINTS_0` / `WEIGHTS_0` |
| `MESH-SKIN-ATTR-COUNT` | Skinning attribute count ≠ position count |
| `MESH-SKIN-JOINTS-TYPE` / `MESH-SKIN-WEIGHTS-TYPE` / `MESH-SKIN-WEIGHTS-NONFINITE` | Unsupported or non-finite attributes |
| `MESH-SKIN-MISSING` | Skinning attributes without any `skins` entry |
| `MESH-SKIN-JOINT-INDEX` | Non-zero-weight joint index outside every skin’s joints range |
| `MESH-SKIN-MIXED` | Mixing skinned and unskinned primitives in one asset |

### Explicitly out of this subset

- `JOINTS_1+` / `WEIGHTS_1+` (more than four influences).
- Sparse accessors for skinning attributes.
- Animation clip **formats** are documented in [`animation-clip-assets.md`](animation-clip-assets.md); this mesh subset still does not import clips into `ImportedMesh`.
- Node hierarchy bake into a runtime skeleton pose beyond storing joint node indices / names / IBMs.
- Catalog-wide NPC/prop GPU skinning (player play-test path is live; general enablement is follow-on).

Retargeting metadata and IK hooks ship as authorable `*.rig.json` ([`rig-assets.md`](rig-assets.md), TICKET-0106 / DEC-0041) — not embedded in the glTF importer.

## Pending importer work

Normals, tangents, full node-transform baking for static meshes, mesh optimization, engine-owned compiled binaries, thumbnails, generic filesystem hot reload, and generated collision are not part of this pass. **Live catalog load is active for MCP/editor authoring:** when the prefab catalog changes (or MCP writes a `.gltf`/`.glb`), missing mesh assets are imported and queued reloads re-upload GPU geometry without restarting the editor. UVs and base-color texture import/sampling are now active (TICKET-0191); the remaining PBR texture set (normal, metallic-roughness, emissive maps), mipmaps, anisotropic filtering, and texture atlasing remain out of scope.

The sample `dead-tree.gltf` was authored for this project, has no external source content, and may be modified and used commercially with the project.

`campfire.gltf` is the Blockbench campfire bake (**stone ring + cylindrical logs** — no flame mesh). Source: `tools/art/campfire/Campfire_New.gltf`. Rebake with `python tools/bake_tier1_props_gltf.py` (feet at y=0, ~1.3 m ring diameter, atlas `campfire.png`). Bake flips inside-out primitives and snaps transparent UVs to opaque atlas texels so prop albedo sampling matches Blockbench’s shaded view. Prefab keeps warm point light, solid staticWorld sphere, and `use_campfire` trigger. Flame deferred to particles (`effects_campfire_flame`).

`player.gltf` is the open-world RPG starting player visual (**GoodPlayerModel** Blockbench bake: skinned mesh + locomotion clips including `Idle`/`Walk`/`Run`/`Fall` (+ Jump/Attack/Dodge/… when exported) + `player.png` atlas + **37 joints** including per-digit hand bones). Prefer repo-canonical `tools/art/player/GoodPlayerModel.gltf` (+ `.png`); optional drop `Documents/Models/GoodPlayerModel.gltf`. **Rebake:** `engine asset-bake --project samples/open-world-rpg --target player` (or Diagnostics → **Assets** tab, or `python tools/asset_bake.py --target player`). Low-level: `python tools/bake_player_v2_gltf.py`. Bake clears atlas backdrop, pads UV islands, normalizes feet at y=0 / height ≈ 2.75 m; keeps Blockbench UV V as-is for D3D; flips majority-inward hand/digit faces for D3D cull; refuses GoodPlayerModel+legacy V2 atlas pairing; fail-closes via `ASSET-BAKE-*` gates (clips/joints/atlas/height/hand winding). Latest bake generator: `GoodPlayerModel-2026-07-31-winding`. Report: `assets/models/player.bake.json`. Hand bones: `Left`/`Right` + `Thumb`/`Index`/`Middle`/`Ring`/`Pinky` + `1`/`2` under each hand. Joint names match `assets/characters/player.rig.json`. Locomotion controller `assets/animators/player.animator.json` blends `Idle`↔`Walk`↔`Run` from `speed` and plays `Fall` when `grounded` is false.

`tree.gltf` is the open-world RPG scene tree visual (Blockbench free-mesh bake). Source: `tools/art/tree/Tree.bbmodel`. Rebake with `tools/bake_tree_bbmodel.py`, which triangulates mesh elements, emits `TEXCOORD_0`, writes the atlas to `tree.png`, bakes a `COLOR_0` fallback, and normalizes feet at y=0 / height ≈ 3.0 m. The `tree.prefab.json` Scene Asset references this mesh with a trunk capsule collider (prefab-local; mesh entity scale is typically 3×).

Oak silhouette variants derived from the same Blockbench oak (shared `tree.png` atlas): `oak_wide.gltf`, `oak_tall.gltf`, `oak_lean.gltf`, `oak_asymmetric.gltf`, `oak_young.gltf`. Sources under `tools/art/tree/variants/`. Regenerate with `python tools/generate_oak_variants.py`. Matching Scene Asset prefabs: `oak_wide`, `oak_tall`, `oak_lean`, `oak_asymmetric`, `oak_young` (each with a staticWorld trunk capsule sized for its mesh scale).

`stones.gltf` is a small Blockbench rock cluster bake. Source: `tools/art/stones/Stones.gltf`. Rebake with `tools/bake_stones_gltf.py` (feet at y=0, height ≈ 0.45 m, atlas `stones.png`). Prefab: `assets/prefabs/Scene Assets/stones.prefab.json` (staticWorld box).

`dead_log.gltf` is a fallen Blockbench log bake. Source: `tools/art/dead-log/DeadLog.gltf` (`.bbmodel` alongside). Rebake with `tools/bake_dead_log_gltf.py` (feet at y=0, thickness ≈ 0.45 m, atlas `dead_log.png`). Prefab: `assets/prefabs/Scene Assets/dead_log.prefab.json` (staticWorld box).

`stump.gltf` is a cut Blockbench stump bake. Source: `tools/art/stump/Stump.gltf` (`.bbmodel` alongside). Rebake with `tools/bake_stump_gltf.py` (feet at y=0, height ≈ 0.55 m, atlas `stump.png`). Prefab: `assets/prefabs/Scene Assets/stump.prefab.json` (staticWorld capsule).

`crate.gltf` is a Blockbench supply-crate bake. Source: `tools/art/crate/Crate.gltf` (also mirrored from `Documents/Models/Crate.gltf`). Rebake with `python tools/bake_tier1_props_gltf.py` (generator `v2-uv-snap`; feet at y=0, height ≈ 1.0 m, atlas `crate.png`). Prefab: `assets/prefabs/Scene Assets/crate.prefab.json` (staticWorld box collider). Atlas paint is very dark brown — if it reads as black in-engine, check lighting before assuming a missing texture.

`bush.gltf` / `bush_tall.gltf` are Blockbench shrub bakes. Sources: `tools/art/bush/Bush.gltf`, `tools/art/tall-bush/Tall_Bush.gltf` (tall also mirrored from `Documents/Models/Tall_Bush.gltf` + `Tall_Bush.png`). Rebake with `python tools/bake_tier1_props_gltf.py` / `... bush_tall` (tall generator `v6-foliage-soft-inpaint`: soft chrome clean + small island dilate — **not** full-atlas inpaint; feet at y=0; heights ≈ 1.25 m / 1.9 m; atlases `bush.png` / `bush_tall.png`; materials force `alphaMode: OPAQUE`; bake flips whole inside-out prims and individual mixed-winding tris). If tall bush looks muddy/camo again, see [`recurring-asset-failures.md`](../testing/recurring-asset-failures.md). Prefabs: `bush.prefab.json`, `bush_tall.prefab.json` (staticWorld sphere colliders). `bush_wide` remains primitive-composed until a wide mesh ships (also has a static sphere collider; do not place new instances — see no-bush-wide rule).

### Importing a model from inside the editor

TICKET-0247 adds a pick-a-file path on top of the named bake catalog, so bringing art in no longer requires editing `tools/asset_bake_catalog.json` by hand.

- **Asset Browser → `Import Model...`** (or **Diagnostics → Assets → Choose model file...**) opens a native picker for `.gltf` / `.glb` / `.bbmodel`. Rows under `assets/models` offer **Re-import** (bake from the registered source) and **Replace...** (pick a new source for that target). There is no separate Blockbench tab — `.bbmodel` files are authoring sources for the same import flow.
- **Player animations:** Import / Re-import / `engine asset-bake --target player` keeps the Blockbench **mesh + UVs**, then replaces **every** clip from `GoodPlayerModel_rigged.bbmodel`. Position keys are applied as `restTranslation + offset/16` (Blockbench stores offsets from rest — treating them as absolute sinks Hips through the floor). Full bbmodel→mesh export remains last-resort only (destroys UV seams).
- The editor first builds a **plan** and shows it before anything is written: which catalog target the file maps to, how it matched (registered source / baked output / id / file name), static vs skinned, mesh and atlas outputs, the animation clip list, and the prefab that references the mesh. Picking `GoodPlayerModel.gltf` or `GoodPlayerModel_rigged.bbmodel` therefore reports "Updates existing asset `player`" instead of creating a second character.
- Unmatched files become a **new target**: the id is slugified from the file name (`My Cool Prop.gltf` → `my_cool_prop`, uniquified against the catalog), the world height is editable before import, the source is normalized into `tools/art/<slug>/`, a catalog entry is appended, and a placeable prefab is written to `assets/prefabs/Imported/<slug>.prefab.json`.
- The bake runs on a worker thread. Diagnostics → Assets shows staged text plus a progress bar while it works, then a green check with the elapsed time (a skinned character bake is ~90 s). Failures list the `ASSET-BAKE-*` / `ASSET-IMPORT-*` gate that stopped it.
- On success the mesh, atlas, and animation clip library hot-reload in the running editor; newly registered targets also refresh the asset catalog so the prefab appears without a restart.
- CLI parity: `engine asset-import --project <path> --file <model.gltf> [--id <slug>] [--plan] [--json]`. `--plan` prints the resolution without writing anything.
- Generic bakers: `tools/bake_generic_gltf.py` (skinned) and `tools/bake_tier1_props_gltf.py` (static) run against a normalized copy produced by `tools/gltf_normalize.py`, which flattens `.glb`, external `.bin`, and external images into single-buffer glTF plus a sidecar PNG. Models whose nodes carry baked `matrix` transforms skip the scale/feet normalize pass, and their catalog entry disables the height/feet gates accordingly.

### Recovering a corrupted Tier-1 prop mesh

When an in-game prop looks hollow, checkerboarded, truncated, wrong-colored, or otherwise “corrupted,” restore from a known-good Blockbench export — do not hand-edit the baked glTF.

1. **Save a clean export** from Blockbench (`File → Export → glTF`) plus its atlas PNG into `Documents/Models/<Name>.gltf` (+ `.png`). Keep the `.bbmodel` if you have one.
2. **Copy into the repo** under `tools/art/<slug>/` (overwrite the prior source). The bake script must not be the only copy of the good art.
3. **Rebake** the named prop so the generator string bumps and the GPU reload is obvious:
   ```bash
   python tools/bake_tier1_props_gltf.py bush_tall
   ```
   Confirm logs show `transparentUVs` snapped / `flipped_prims` as expected, feet at y=0, and the target height/span.
4. **Confirm the prefab** still points at `assets/models/<name>.gltf` with a full JSON body (never an MCP `source` string without `json` — that can replace the prefab with garbage).
5. **Reload the mesh in the editor** — kill `engine.exe`, restart `editor` / `mcp --project samples/open-world-rpg` so the GPU drops the stale buffer. Re-place or re-select an instance and screenshot.
6. **Quick integrity checks** (optional): `asset.generator` matches the bake label; atlas PNG opens; glTF JSON parses; UV sample count with alpha&lt;8 is 0 after bake.

Skinned kits (player) use `engine asset-bake --target player` / `tools/bake_player_v2_gltf.py` with `tools/art/player/GoodPlayerModel.gltf`.

`barrel.gltf` is a Blockbench barrel bake. Source: `tools/art/barrel/Barrel.gltf`. Rebake with `python tools/bake_tier1_props_gltf.py barrel` (feet at y=0, height ≈ 1.0 m, atlas `barrel.png`). Prefab: `assets/prefabs/Scene Assets/barrel.prefab.json` (staticWorld capsule).

`lantern.gltf` / `wall_torch.gltf` are Blockbench light props. Sources: `tools/art/lantern/Lantern.gltf`, `tools/art/wall-torch/Wall_Torch.gltf` (torch also mirrored from `Documents/Models/Wall Torch.gltf`). Rebake with `python tools/bake_tier1_props_gltf.py lantern wall_torch` (torch generator `v2-uv-snap`; feet at y=0; heights ≈ 0.55 m / 0.7 m; atlases `lantern.png` / `wall_torch.png`). Bake clears Blockbench near-white / pale UV-editor backdrop texels before UV snap. Prefabs include warm point lights; wall torch ships layered particles (`wall_torch_{core,flame,embers,smoke}`).

`ashfell_arming_sword.gltf` is the Ashfell Blade starter arming sword (Blockbench bake). Source: `tools/art/ashfell-arming-sword/Ashfell_Arming_Sword.gltf` (mirrored from `Documents/Models/Ashfell Arming Sword.gltf`). Rebake with `python tools/bake_tier1_props_gltf.py ashfell_arming_sword` (feet at y=0, tip-to-pommel ≈ 1.05 m, atlas `ashfell_arming_sword.png`). Prefab: `assets/prefabs/Scene Assets/ashfell_arming_sword.prefab.json` (visual mesh; no staticWorld collider — attachable / clutter). Concept: `context/art/concepts/starter-ashfell-arming-sword.png`. **Hand attach:** play-test parents `worldMesh` to a skinned joint; author `handAttach` on the item (`joint`, `gripOffset`, `gripEulerDeg`) via Inspector → **Held Weapon Attach** → Save; see `gearing-system.md`.

`outrider_shortbow.gltf` / `outrider_arrow.gltf` are the Outrider starter bow + arrow (Blockbench bake). Sources: `tools/art/outrider-shortbow/Outrider_Shortbow.gltf`, `tools/art/outrider-arrow/Outrider_Arrow.gltf`. Rebake bow with `python tools/bake_tier1_props_gltf.py outrider_shortbow` (opt-in `preserve_skin` — keeps `skins`, JOINTS/WEIGHTS, and `bow_draw`; also `fix_winding` + `double_sided_thin` + atlas clean so BACK-culled props do not hole-out) or arrow with `… outrider_arrow`. Prefer `engine asset-bake --target outrider_shortbow` for named verify. Bow tip-to-tip ≈ 1.05 m height; arrow uses `scale_mode: max_extent` ≈ 0.75 m tip-to-nock. Prefabs: `outrider_shortbow.prefab.json`, `outrider_arrow.prefab.json` (visual clutter; no solid collider). **Hand attach:** item catalog `handAttach` on `outrider_shortbow` (LeftHand + `drawClip: bow_draw`). Runtime draws a skinned held mesh and samples `bow_draw` against player `BowShoot` / state `bowShoot` scrub. Nock arrow mesh / string IK remain follow-on.

`guild_rune_focus.gltf` is the Runecaster starter inscribed focus (Blockbench bake). Source: `tools/art/guild-rune-focus/Guild_Rune_Focus.gltf`. Rebake with `python tools/bake_tier1_props_gltf.py guild_rune_focus` (feet at y=0, height ≈ 0.55 m). Prefab: `guild_rune_focus.prefab.json` (visual clutter). **Hand attach:** item catalog `handAttach` on `guild_rune_focus` (LeftHand, same hand as sword for this body).

`loot_bag.gltf` is a generic low-poly drawstring loot bag (Blockbench). Preferred bake source: `tools/art/loot-bag/Loot_Bag.gltf` via `python tools/bake_tier1_props_gltf.py loot_bag` (feet at y=0, height ≈ 0.45 m, atlas `loot_bag.png`). Legacy bbmodel path: `python tools/bake_loot_bag_gltf.py`. Prefab: `assets/prefabs/Scene Assets/loot_bag.prefab.json` (staticWorld box + `open_loot_bag` search trigger). Crate-as-chest stand-in: `supply_chest.prefab.json` (`open_supply_chest`) for icon-only Landfall finds until a dedicated chest mesh ships.

Act 0 Landfall item defs + container loot tables (icon-only for trinkets/consumables): `samples/open-world-rpg/assets/items/act0_landfall_items.json`. Inventory icons under `assets/ui/icons/items/` (concept art copies). Sandbox preview cluster near `campfire_mesh_test`.

Planned Blockbench props, character kits, and set pieces (not yet authored) are tracked in [`context/art/blockbench-asset-list.md`](../art/blockbench-asset-list.md).
