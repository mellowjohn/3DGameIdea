# Material Asset Format

Material assets use the `.material.json` suffix and schema version 1. They are authoritative, diffable project data. Project validation parses every material and rejects invalid or incomplete values before runtime.

## Fields

- `baseColor`: linear RGBA values in `[0,1]`. Opaque materials require alpha `1`.
- `roughness`: perceptual roughness in `[0,1]`.
- `metallic`: metallic response in `[0,1]`.
- `opacityMode`: `opaque`, `masked`, or `blended`.
- `opacityCutoff`: masked-material cutoff in `[0,1]`.
- `emissive`: nonnegative RGB values up to `64`.
- `doubleSided`: disables back-face assumptions when the renderer supports the material.
- `physics.friction`: Jolt friction in `[0,2]`.
- `physics.restitution`: bounciness in `[0,1]`.
- `physics.density`: positive density up to `100000` kg/m³.
- `physics.surface`: stable surface identifier used later by footsteps, impacts, particles, and decals.

## Current runtime support

Terrain base color multiplies the low-poly facet palette. Terrain friction and restitution configure its Jolt body. The editor can create, inspect, and save `.material.json` assets. Prefab primitive parts can reference a material asset; the editor uses the material `baseColor` for viewport rendering. The **Sculpt** tab selects the active terrain material and opens it in the Inspector. Saving the active terrain material refreshes the viewport; friction/restitution changes reload loaded heightfields.

### Opaque PBR lighting (TICKET-0040 / 0143)

Opaque materials drive a Cook-Torrance (GGX) lighting path:

- `roughness` and `metallic` affect directional and point-light response for compositional prefab parts and the active terrain material.
- `emissive` is added after lighting (linear RGB, same units as authored).
- Ambient uses `baseColor * ambient` (metallic surfaces still receive ambient; there is no IBL yet).
- Parts that reference a **masked** or **blended** material are **not drawn** (fail closed — no fake alpha).
- Terrain always draws through the opaque path: if the terrain material’s `opacityMode` is masked/blended, roughness/metallic/emissive fall back to dielectric defaults (`1` / `0` / `0`) and no transparency is simulated.
- Foliage and meshes without a material reference use dielectric defaults (`roughness=1`, `metallic=0`, no emissive).
- `doubleSided` remains informational; the current mesh pipeline already disables back-face culling.
- `opacityCutoff` is validated and preserved but unused until a masked pipeline exists.

CPU reference evaluation lives in `include/engine/rendering/pbr_lighting.h` and is covered by the `assets` suite.

### Ambient occlusion (TICKET-0042)

Ambient occlusion is a **screen-space post-process** (SSAO v1, depth-derived), not a material map. The material schema has no `occlusionMap`/`ao` field, and none is planned until a baked-AO ticket is scoped. See `context/planning/tickets/TICKET-0042.md` and `context/features/index.md`.
