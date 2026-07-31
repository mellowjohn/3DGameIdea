# Material Asset Format

Material assets use the `.material.json` suffix and schema version 1. They are authoritative, diffable project data. Project validation parses every material and rejects invalid or incomplete values before runtime.

## Fields

- `shader`: optional master profile — `stylized_opaque` (default) or `emissive_magic` ([DEC-0049](../decisions/index.md#dec-0049-agent-writable-material-shader-profiles), [material-shader-profiles.md](../features/material-shader-profiles.md)). Unknown values fail with `MATERIAL-SHADER-UNKNOWN`.
- `baseColor`: linear RGBA values in `[0,1]`. Opaque materials require alpha `1`.
- `roughness`: perceptual roughness in `[0,1]`.
- `metallic`: metallic response in `[0,1]`.
- `opacityMode`: `opaque`, `masked`, or `blended`.
- `opacityCutoff`: masked-material cutoff in `[0,1]`.
- `emissive`: nonnegative RGB values up to `64`.
- `emissivePulseHz`: optional pulse rate in `[0,60]` (0 = steady). Applied for `emissive_magic` at runtime.
- `emissivePulseMin`: optional floor scale in `[0,1]` (default `0.35`) for the pulse envelope.
- `albedoMap`: optional project-relative `.png` sampled as albedo (+ alpha for masked). Empty = mesh-embedded / vertex color path.
- `emissiveMap`: optional project-relative `.png`; **v1** multiplies authored `emissive` by the texture’s **average RGB** (UV-sampled emissive maps can follow). Missing files fail closed (`MATERIAL-MAP-MISSING`).
- `doubleSided`: disables back-face assumptions when the renderer supports the material.
- `physics.friction`: Jolt friction in `[0,2]`.
- `physics.restitution`: bounciness in `[0,1]`.
- `physics.density`: positive density up to `100000` kg/m³.
- `physics.surface`: stable surface identifier used later by footsteps, impacts, particles, and decals.

## Current runtime support

Terrain base color multiplies the low-poly facet palette. Terrain friction and restitution configure its Jolt body. The editor can create, inspect, and save `.material.json` assets. Prefab primitive parts can reference a material asset; the editor uses the material `baseColor` for viewport rendering. The **Sculpt** tab selects the active terrain material and opens it in the Inspector. Saving the active terrain material refreshes the viewport; friction/restitution changes reload loaded heightfields. MCP `engine_asset_apply` (`kind: material`) creates/updates the same schema for agents.

### Opaque PBR lighting (TICKET-0040 / 0143)

Opaque materials drive a Cook-Torrance (GGX) lighting path:

- `roughness` and `metallic` affect directional and point-light response for compositional prefab parts and the active terrain material.
- `emissive` is added after lighting (linear RGB, same units as authored).
- Ambient uses `baseColor * ambient` (metallic surfaces still receive ambient; there is no IBL yet).
- Parts that reference a **masked** material draw with **alpha clip** (`opacityCutoff`): textured albedo uses texture alpha; untextured uses `baseColor` alpha (TICKET-0239). **Blended** non-water materials are still not drawn (fail closed).
- Terrain always draws through the opaque path: if the terrain material’s `opacityMode` is masked/blended, roughness/metallic/emissive fall back to dielectric defaults (`1` / `0` / `0`) and no transparency is simulated.
- Foliage and meshes without a material reference use dielectric defaults (`roughness=1`, `metallic=0`, no emissive).
- `doubleSided` remains informational; the current mesh pipeline already disables back-face culling.
- `opacityCutoff` drives masked clip when `opacityMode` is `masked`.

CPU reference evaluation lives in `include/engine/rendering/pbr_lighting.h` and is covered by the `assets` suite.

### Shader profiles (TICKET-0238)

- Missing `shader` loads as `stylized_opaque`.
- `emissive_magic` copies `emissivePulseHz` / `emissivePulseMin` into runtime surface params; when Hz > 0, packed emissive scales with a sine envelope each frame (prefab instance draws and per-object constant path).
- Sample: `samples/open-world-rpg/assets/materials/rune_glow.material.json`.

### Masked cutout (TICKET-0239)

- Prefab parts with `opacityMode: masked` are no longer skipped.
- GPU `clip(alpha - opacityCutoff)` on mesh and instanced prop passes (`materialParams.w` / instance material.w ≥ 0 enables clip).
- Alpha source: albedo texture `.a` when sampling; else packed `baseColor.a`. Meaningful leaf/banner cutouts need an alpha channel in the mesh albedo (TICKET-0191) or material `albedoMap` (TICKET-0240).
- Sample: `samples/open-world-rpg/assets/materials/leaf_cutout.material.json`.
- Blended (non-water) remains fail-closed.

### Material texture maps (TICKET-0240)

- `albedoMap` / `emissiveMap` are optional project-relative PNGs (no `..`, not absolute).
- Project validate and MCP material apply call `validate_texture_maps` — missing files → `MATERIAL-MAP-MISSING`.
- Runtime prefers material `albedoMap` over mesh-embedded albedo when both exist; prop batches split by mesh+map.
- `emissiveMap` v1: scales authored emissive by average RGB of the PNG (cheap modulate for agents).
- Samples: `leaf_cutout` (`albedoMap` → `wind_streak.png`), `rune_glow` (`emissiveMap` → `fire_flipbook_4x4.png`).

### Ambient occlusion (TICKET-0042)

Ambient occlusion is a **screen-space post-process** (SSAO v1, depth-derived), not a material map. The material schema has no `occlusionMap`/`ao` field, and none is planned until a baked-AO ticket is scoped. See `context/planning/tickets/TICKET-0042.md` and `context/features/index.md`.

Directional sun contact shading uses **cascaded shadow maps** (CSM v1, TICKET-0219) sampled in the opaque/foliage lighting pass — complementary to SSAO, not a material property. See `context/features/csm-shadows.md`.
