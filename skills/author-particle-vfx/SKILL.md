---
name: author-particle-vfx
description: >-
  Author or clone stylized particle VFX (flame, torch, sparks, smoke, wind)
  via recipes, MCP particle apply, materials, and prefab particles[]. Use when
  adding campfire/torch layers, hit sparks, corrupt aura, wind trails, bloom-
  friendly emissive fire, or cloning from vfx_recipes.json.
---

# Author Particle VFX

Clone agent recipes into `*.particle.json` + prefab emitters — code-first masters, not shader graphs ([DEC-0049](../../context/decisions/index.md)).

**Read first:** [`context/features/particles.md`](../../context/features/particles.md), [`context/features/stylized-flame-vfx.md`](../../context/features/stylized-flame-vfx.md), [`context/formats/particle-emitter-assets.md`](../../context/formats/particle-emitter-assets.md), [`context/art/stylized-flame-goal.md`](../../context/art/stylized-flame-goal.md).

**Not for:** new C++ particle features (ticket + rebuild); volumetric raymarch fire.

## Checklist

```
VFX:
- [ ] Pick recipe id from assets/vfx/recipes/vfx_recipes.json
- [ ] Clone / tune *.particle.json via engine_asset_apply (kind: particle)
- [ ] Attach on prefab particles[] with local offsets (Save Prefab)
- [ ] Layer emissive material + point light when flame needs a core
- [ ] Screenshot in Game/play-test (bloom after particles)
```

## 1. Recipe catalog

Path: `samples/open-world-rpg/assets/vfx/recipes/vfx_recipes.json`

| Recipe id | Clone these |
| --- | --- |
| `torch` | `wall_torch_{core,flame,embers,smoke}.particle.json` |
| `campfire_layer` | `campfire_{flame,embers,smoke}.particle.json` |
| `hit_spark` | `hit_spark.particle.json` |
| `corrupt_aura` | `corrupt_aura.particle.json` |
| `dodge_dust` | `dodge_dust.particle.json` (spawn via `ParticleSystem::spawn_burst`) |
| `footstep_dust` | `footstep_dust.particle.json` (spawn via stride counter / land footstep) |
| `arrow_trail` | `arrow_trail.particle.json` (play-test arrow wake via `spawn_burst`) |
| `arrow_impact` | `arrow_impact.particle.json` (play-test arrow impact via `spawn_burst`) |
| `stylized_flame_molten` | blobby crossed billboards (hero default) |
| `stylized_flame_wispy` | narrow ribbon / wind-blown |
| `stylized_flame_column` | tall cylinder column |

Hero prefab: `assets/prefabs/Scene Assets/stylized_flame.prefab.json`  
Production: `campfire.prefab.json`, `wall_torch.prefab.json`.

## 2. MCP apply

1. Ensure live editor MCP (`skills/live-editor-mcp`).
2. `engine_asset_apply` with `kind: particle` (or path ending `.particle.json`) — validates, writes, hot-registers.
3. Prefab: `engine_prefab_apply` / Inspector — `particles: [ { "asset": "…", "offset": [x,y,z] }, … ]`.
4. Offsets are prefab-local and follow placement rotation; use Scene **Move with Gizmo** then Save Prefab.

## 3. Layering rules (readable stylized fire)

Typical stack (bottom → top intent):

1. Mesh keeps baked glTF albedo — **do not** apply `flame_core_emissive.material.json` to the whole campfire/torch (it floods every face with pulsed emissive yellow).
2. Small flipbook core + molten / flame billboards (`lightEmission` ~0.5–0.75 for softLight fire; avoid untextured full-opacity discs).
3. Embers / `hit_spark`.
4. Smoke (low emission, soft fade).
5. Warm orange point light on the prefab (moderate radius/strength).
6. Bloom (TICKET-0242) softens sparks — particles draw **before** bloom.

Flipbooks: project-owned under `assets/vfx/` (`gen_fire_flipbook.py` provenance in `context/resources/index.md`). Invalid texture paths fail closed.

Useful shaping: `crossedBillboards`, `FacingCameraWorldUp`, `aspectRatio`, `minScreenSize`, flipbook crossfade fields — see format doc.

## 4. Ambient wind

Meadow wind uses `wind_trail.particle.json` + `ParticleSystem::set_ambient_wind` (not a placement recipe). Prefer tuning the existing trail over inventing a second ambient system.

## 5. Verify

- `engine_project_validate`
- `engine_editor_screenshot` on Game tab / play-test near the emitter
- Do not claim volumetric match to blob-mesh ref — current path is layered billboards + emissive core

## Done bar

Recipe-sourced JSON; prefab `particles[]` wired; live screenshot; no third-party textures without resources/index entry.
