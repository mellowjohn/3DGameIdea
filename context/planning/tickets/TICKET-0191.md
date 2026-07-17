# TICKET-0191: glTF mesh UV + albedo texture import/render

- Epic: EPIC-0005
- Status: needs-approval
- Agent: cursor-agent
- Priority: P0
- Notion: https://app.notion.com/p/3a0d3efc5695815ca8b5fb5312dad8fc

## Goal

Imported glTF meshes with `TEXCOORD_0` and a PBR `baseColorTexture` render with sampled albedo (point filtering for pixel-art) instead of vertex-color-only fallbacks, so the starting player mesh shows face/clothing atlas detail.

## Context links

- `context/formats/mesh-assets.md` (pending importer: UVs, textures, material bindings)
- `context/formats/prefab-assets.md` (imported mesh parts)
- TICKET-0040 / TICKET-0143 (opaque PBR path to extend, not replace)
- Soft: TICKET-0164 (UI PNG upload via WIC — reuse patterns, different heap)

## Acceptance criteria

- [x] glTF importer reads optional `TEXCOORD_0` into runtime vertices.
- [x] Importer loads `materials[].pbrMetallicRoughness.baseColorTexture` (embedded `data:` PNG or external file) into engine-owned RGBA pixels on `ImportedMesh`.
- [x] Editor/runtime mesh draw samples albedo when a mesh has a texture; otherwise keeps vertex-color behavior.
- [x] Point (nearest) sampling used for albedo (Blockbench / pixel-art friendly).
- [x] Sample `assets/models/player.gltf` ships with UVs + albedo and shows atlas detail in Scene/playtest.
- [x] Assets suite covers UV/texture import happy path + malformed COLOR/UV mismatch style errors as applicable.
- [x] `context/formats/mesh-assets.md` and resource registry updated.

## Out of scope

- Full PBR texture set (normal, metallic-roughness, emissive maps).
- Mipmaps / anisotropic filtering / texture atlasing system.
- Node-transform baking in the importer (player bake script remains the static flatten path).
- Animation / skinning playback changes.
- UI canvas image textures (TICKET-0164).

## Dependencies

Blocked by: none (owner P0 override). Soft: existing WIC PNG decode patterns in `imgui_png_texture.cpp`.

## Verification

Rebuild `engine` + `engine_suite_tests`; run `--suite assets`; `engine validate --project samples/open-world-rpg`; visual check player in editor.

## What changed

**Summary.** Added glTF UV + base-color texture import and GPU albedo sampling so the player mesh shows its Blockbench atlas (eyes/clothing). Untextured meshes and primitives keep the existing vertex-color path.

**Files / surfaces.**
- `include/engine/assets/mesh_asset.h`: `MeshVertex` gains `u,v` (default 0). `ImportedMesh` gains `albedo_rgba`, `albedo_width/height`, and `has_albedo()`.
- `src/assets/mesh_asset.cpp`: reads optional `TEXCOORD_0` (`FLOAT VEC2`, count-checked) into expanded vertices; resolves the first primitive material's `pbrMetallicRoughness.baseColorTexture` → texture → image and decodes it to RGBA8 (embedded `data:` PNG, buffer-view image, or external file). Parser now requests `LoadExternalImages`. Kept `COLOR_0`/brown fallback.
- `include/engine/assets/png_decode.h` + `src/assets/png_decode.cpp` (new): WIC-based `decode_png_file(path)` / `decode_png_bytes(span)` returning `PngImage{width,height,rgba}`. Added to `engine_core` in `CMakeLists.txt`.
- `src/rendering/render_app.cpp`: local `Vertex` gains `u,v`; imported-mesh upload carries UVs; main opaque pipeline VS/PS add UV interpolation and `Texture2D albedo` + point/clamp static sampler at `s0`, sampling when `materialParams.z` (useAlbedo) > 0.5. Root signature adds a `t0` SRV descriptor table + static sampler; input layouts (main + foliage) add `TEXCOORD` R32G32 @ offset 24. New shader-visible SRV heap holds a 1×1 white fallback (slot 0) plus one aligned slot per imported mesh; textured meshes bind their SRV, others bind white with useAlbedo=0. `pack_object_constants` packs useAlbedo.
- `tools/bake_player_gltf.py`: emits `TEXCOORD_0`, writes the atlas to `player.png` next to the glTF, references it via a `baseColorTexture` material (+ point/clamp sampler), keeps `COLOR_0` fallback. Rebaked `samples/open-world-rpg/assets/models/player.gltf` (648 verts / 324 tris) and wrote `player.png` (256²).
- Tests: `tests/suite_tests.cpp` assets suite adds a `TEXCOORD_0` + embedded 2×2 PNG `baseColorTexture` import happy path (asserts UVs, `has_albedo()`, 2×2 size, red top-left texel) and a `MESH-UV-COUNT` mismatch rejection.
- Docs: `context/formats/mesh-assets.md` (UV/albedo now active), `context/resources/index.md` (player atlas entry).

**Schema/API deltas.** New importer errors `MESH-UV-COUNT` / `MESH-UV-TYPE` / `MESH-UV-NONFINITE` and `MESH-TEXTURE-*`; PNG decode errors `PNG-*`. `MeshVertex`/`ImportedMesh` gained fields (additive).

**Verification.** Reconfigured CMake (new source); MSBuild `engine` + `engine_suite_tests` (Debug) succeeded with 0 warnings / 0 errors. `engine_suite_tests --suite assets` → 61/61 pass. `engine validate --project samples/open-world-rpg` → assets valid, exit 0 (confirms `player.gltf` + external `player.png` import). GPU smoke `engine run --frames 5 --hidden --debug-layer` and `engine editor --frames 3 --hidden --debug-layer` both exit 0 with no D3D12 debug-layer errors, exercising the textured player draw + new root signature/descriptor table. Any running `engine.exe` was killed before building and the smoke processes exited cleanly (no stale process left).

**Decisions / leftover risk.** UV V is sampled as-is (glTF top-left convention; WIC decodes top-down), matching the bake script; if the atlas ever appears vertically flipped, flip to `1-v` in the importer expansion. Only the base-color map is imported (no normal/MR/emissive maps, mipmaps, or atlasing). The albedo SRV heap is rebuilt on each mesh sync sized to `imported_meshes + 1`.

## Agent notes

Owner asked 2026-07-17 to track and implement as P0 after player v1 mesh landed without GPU textures.
