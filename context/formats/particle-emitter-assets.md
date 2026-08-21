# Particle emitter assets

Versioned JSON emitters (`*.particle.json`) for the CPU particle MVP (TICKET-0122). Property model follows [Roblox ParticleEmitter](https://create.roblox.com/docs/effects/particle-emitters) for low-poly VFX authoring.

## Example

```json
{
  "schemaVersion": 1,
  "id": "campfire_flame",
  "texture": "assets/vfx/fire_flipbook_4x4.png",
  "flipbookLayout": "Grid4x4",
  "flipbookMode": "Loop",
  "flipbookFramerate": 14,
  "flipbookStartRandom": true,
  "color": {
    "keypoints": [
      { "t": 0.0, "color": [1.0, 0.98, 0.85] },
      { "t": 1.0, "color": [0.55, 0.12, 0.04] }
    ]
  },
  "size": { "keypoints": [{ "t": 0.0, "value": 0.28 }, { "t": 1.0, "value": 0.22 }] },
  "transparency": { "keypoints": [{ "t": 0.0, "value": 0.2 }, { "t": 1.0, "value": 1.0 }] },
  "lifetime": { "min": 0.55, "max": 1.05 },
  "speed": { "min": 0.25, "max": 0.7 },
  "rate": 18,
  "spreadAngle": [18, 18],
  "shape": "disc",
  "shapeStyle": "volume",
  "shapeSize": [0.18, 0.04, 0.18],
  "orientation": "FacingCamera",
  "lightEmission": 0.95,
  "blend": "softLight",
  "acceleration": [0.0, 1.4, 0.0],
  "drag": 0.45,
  "maxParticles": 64,
  "enabled": true,
  "emissionDirection": [0.0, 1.0, 0.0]
}
```

## Fields

| Field | Type | Notes |
| --- | --- | --- |
| `schemaVersion` | int | `1` |
| `id` | string | Stable id (often slug of display name) |
| `texture` | string | Optional PNG path; empty = built-in soft disc. Non-empty samples texture RGB×alpha (wind streak, fire flipbook). Must be project-relative `.png` (no `..`); missing files fail closed at validate/MCP (`PARTICLE-TEXTURE-MISSING`). |
| `flipbookLayout` | `None` \| `Grid2x2` \| `Grid4x4` \| `Grid8x8` | Atlas grid (Roblox). Non-`None` requires `texture`. |
| `flipbookMode` | `OneShot` \| `Loop` \| `PingPong` \| `Random` | Frame playback; `Random` holds one random cell. |
| `flipbookFramerate` | number | Frames/sec (capped at 120). |
| `flipbookStartRandom` | bool | Randomize start cell on spawn (default true). |
| `color` | `[r,g,b]` or `{keypoints:[{t,color}]}` | Lifetime ColorSequence |
| `size` | number or NumberSequence | World units |
| `transparency` | number or NumberSequence | **0 = opaque, 1 = clear** (Roblox) |
| `lifetime` / `speed` | number or `{min,max}` | Lifetime capped at 20s |
| `rate` | number | Particles/sec; capped at 400 |
| `spreadAngle` | `[x,y]` degrees | Cone around emission direction |
| `shape` | `box` \| `sphere` \| `cylinder` \| `disc` | |
| `shapeStyle` | `volume` \| `surface` | |
| `shapeSize` | `[x,y,z]` | Half-extents / radius / half-height |
| `orientation` | `FacingCamera` \| `FacingCameraWorldUp` \| `VelocityParallel` \| `VelocityPerpendicular` | `FacingCameraWorldUp` keeps world Y (cylindrical / upright fire) |
| `aspectRatio` | number | Billboard width/height. `size` is height; width = size × aspectRatio (default `1`, clamped 0.05..8) |
| `rotation` | number or NumberSequence | Degrees over lifetime (yaw around world Y for upright; roll around view for full facing) |
| `rotationStartRandom` | bool | Random 0..360° spawn offset (default true) |
| `crossedBillboards` | bool | Draw two world-locked upright quads 90° apart around Y (classic + volume; not camera-edge-on) |
| `minScreenSize` | number | Minimum on-screen height in pixels (`0` = off). Raises world size with distance so fire stays readable far away without growing up close |
| `softOcclusion` | bool | Default `true`. Soft-particle depth fade / hide-behind. Set `false` for weapon impact bursts so hurt-mesh contacts stay visible |
| `lightEmission` | 0..1 | Additive weight |
| `blend` | `alpha` \| `additive` \| `softLight` | SoftLight is default for fire |
| `acceleration` | `[x,y,z]` | World-space |
| `drag` | ≥0 | Exponential velocity damp |
| `maxParticles` | 1..4096 | Pool size |
| `emissionDirection` | `[x,y,z]` | Local axis (default +Y) |

## Prefab attachment

```json
"particle": {
  "asset": "assets/vfx/campfire_flame.particle.json",
  "offset": [0.0, 0.42, 0.0],
  "enabled": true
}
```

## MCP authoring

`engine_asset_apply` with `kind: "particle"` (or path ending `.particle.json`) parses, validates texture, writes atomically, and hot-registers into the live particle system. Recipe index: `assets/vfx/recipes/vfx_recipes.json`.

## Runtime

See [`../features/particles.md`](../features/particles.md). Headers: `include/engine/assets/particle_emitter_asset.h`, `include/engine/vfx/particle_system.h`.
