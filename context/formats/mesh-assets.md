# Mesh Asset Import

The runtime mesh path accepts glTF 2.0 `.gltf` and `.glb` assets through fastgltf 0.9.0. Project validation imports every discovered mesh so malformed geometry fails before an editor session starts.

## Current contract

- Triangle primitives with finite `POSITION` data are required.
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
- Retargeting metadata and IK hooks.
- GPU skinning upload.

## Pending importer work

Normals, tangents, UVs, textures, material bindings, full node-transform baking for static meshes, mesh optimization, engine-owned compiled binaries, thumbnails, hot reload, and generated collision are not part of this pass.

The sample `dead-tree.gltf` was authored for this project, has no external source content, and may be modified and used commercially with the project.

`campfire.gltf` is a registry path for the project-owned procedural campfire mesh (stone ring, crossed logs, and flame) generated at import time.
