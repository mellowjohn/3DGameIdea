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

The importer also reads a documented glTF skinning subset into engine-owned structures on `ImportedMesh`. This proves the M5 skeletal import path. Animation clips are a separate import: see [`animation-clip-assets.md`](animation-clip-assets.md) (TICKET-0102). GPU skinning playback remains a follow-on.

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
- GPU skinning upload.

Retargeting metadata and IK hooks ship as authorable `*.rig.json` ([`rig-assets.md`](rig-assets.md), TICKET-0106 / DEC-0041) — not embedded in the glTF importer.

## Pending importer work

Normals, tangents, full node-transform baking for static meshes, mesh optimization, engine-owned compiled binaries, thumbnails, generic filesystem hot reload, and generated collision are not part of this pass. **Live catalog load is active for MCP/editor authoring:** when the prefab catalog changes (or MCP writes a `.gltf`/`.glb`), missing mesh assets are imported and queued reloads re-upload GPU geometry without restarting the editor. UVs and base-color texture import/sampling are now active (TICKET-0191); the remaining PBR texture set (normal, metallic-roughness, emissive maps), mipmaps, anisotropic filtering, and texture atlasing remain out of scope.

The sample `dead-tree.gltf` was authored for this project, has no external source content, and may be modified and used commercially with the project.

`campfire.gltf` is a registry path for the project-owned procedural campfire mesh (stone ring, crossed logs, and flame) generated at import time.

`player.gltf` is the open-world RPG starting player visual (Blockbench v1 bake). Source export: `tools/art/player/player.blockbench.gltf`. Rebake with `tools/bake_player_gltf.py`, which flattens node transforms, emits `TEXCOORD_0`, writes the atlas to `player.png` next to the glTF (referenced via a `baseColorTexture` material), also bakes a `COLOR_0` fallback, and normalizes feet at y=0 / height ≈ 1.8 m. The GPU now samples `player.png` (point/clamp) so eyes/clothing atlas detail shows in Scene/playtest. No locomotion clips yet — static mesh only until animation authoring.

`tree.gltf` is the open-world RPG scene tree visual (Blockbench free-mesh bake). Source: `tools/art/tree/Tree.bbmodel`. Rebake with `tools/bake_tree_bbmodel.py`, which triangulates mesh elements, emits `TEXCOORD_0`, writes the atlas to `tree.png`, bakes a `COLOR_0` fallback, and normalizes feet at y=0 / height ≈ 3.0 m. The `tree.prefab.json` Scene Asset references this mesh.

Oak silhouette variants derived from the same Blockbench oak (shared `tree.png` atlas): `oak_wide.gltf`, `oak_tall.gltf`, `oak_lean.gltf`, `oak_asymmetric.gltf`, `oak_young.gltf`. Sources under `tools/art/tree/variants/`. Regenerate with `python tools/generate_oak_variants.py`. Matching Scene Asset prefabs: `oak_wide`, `oak_tall`, `oak_lean`, `oak_asymmetric`, `oak_young`.

`stones.gltf` is a small Blockbench rock cluster bake. Source: `tools/art/stones/Stones.gltf`. Rebake with `tools/bake_stones_gltf.py` (feet at y=0, height ≈ 0.45 m, atlas `stones.png`). Prefab: `assets/prefabs/Scene Assets/stones.prefab.json`.

`dead_log.gltf` is a fallen Blockbench log bake. Source: `tools/art/dead-log/DeadLog.gltf` (`.bbmodel` alongside). Rebake with `tools/bake_dead_log_gltf.py` (feet at y=0, thickness ≈ 0.45 m, atlas `dead_log.png`). Prefab: `assets/prefabs/Scene Assets/dead_log.prefab.json`.

`stump.gltf` is a cut Blockbench stump bake. Source: `tools/art/stump/Stump.gltf` (`.bbmodel` alongside). Rebake with `tools/bake_stump_gltf.py` (feet at y=0, height ≈ 0.55 m, atlas `stump.png`). Prefab: `assets/prefabs/Scene Assets/stump.prefab.json`.

Planned Blockbench props, character kits, and set pieces (not yet authored) are tracked in [`context/art/blockbench-asset-list.md`](../art/blockbench-asset-list.md).
