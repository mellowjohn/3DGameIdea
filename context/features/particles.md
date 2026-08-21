# Particle / VFX system (MVP)

Status: **active** (TICKET-0122 runtime; TICKET-0241 MCP apply + recipes).

## Scope (MVP)

- Authored `*.particle.json` emitters ([format](../formats/particle-emitter-assets.md))
- Prefab `particles[]` (multi-emitter) plus legacy optional `particle` — campfire ships flame + embers + smoke; wall torch ships disc-spread core + flame + embers + smoke (torch-scale)
- Prefab Editor can add/remove emitters, pick `*.particle.json`, edit local offset / enabled, and Save Prefab (offsets are prefab-local and follow placement rotation)
- Scene Inspector lists the same emitters on placed prefabs; **Move with Gizmo** places a translate gizmo on the emitter in the viewport (Save Prefab to persist)
- CPU pool spawn/update with Rate / Lifetime / Speed / Spread / Shape / Color·Size·Transparency sequences / LightEmission
- Soft disc billboards drawn **after SSAO composite** onto the viewport/backbuffer (SoftLight or alpha blend) so emissive fire is not AO-crushed and `lit_color_` stays clean for lighting; **bloom** (TICKET-0242) runs after particles so sparks glow
- Alpha blend keeps authored `lightEmission` when ≥ 0.25 (bright flame sprites); low-emission smoke still clamps soft
- Optional `texture` PNG path: empty keeps procedural soft disc; non-empty samples RGB×alpha (wind streak, fire/smoke/ember flipbooks). Invalid / missing textures fail closed (`PARTICLE-TEXTURE-PATH-INVALID` / `PARTICLE-TEXTURE-MISSING`)
- Flipbook atlas fields (`flipbookLayout` / `Mode` / `Framerate` / `StartRandom`) remap UVs per particle age (Roblox-shaped grids)
- Flipbook frames **crossfade** on the GPU (adjacent atlas cells lerped) so Loop/PingPong playback doesn’t hard-cut
- Billboard shaping: `aspectRatio`, lifetime `rotation` (+ `rotationStartRandom`), true upright `FacingCameraWorldUp`, `crossedBillboards`, and `minScreenSize` (pixel floor for distant landmarks)
- Soft particles / occlusion: billboards sample scene depth as an SRV (hardware depth unbound during the pass), linearize both scene and particle NDC to eye-space meters, then hard-hide behind solid geometry with a ~0.45 m soft intersection band (logs/terrain). Raw NDC compares fail at play distances with a large far plane. Sky/far-plane samples skip the fade so distant fire stays readable. The depth SRV is wired in `create_particle_pipeline` and refreshed on resize — without that bind, occlusion never runs. Authored `softOcclusion: false` skips depth fade (weapon impact bursts on hurt meshes).
- Multi-path particle texture slots (up to 8) so campfire layers and wind streak can draw in the same frame; PNG slots reload when file mtime changes
- Camera-local ambient wind emitter (`ParticleSystem::set_ambient_wind`) for meadow wind trails (TICKET-0230): upwind spawn offset, gust-pulsed rate, morphing `wind_streak_flipbook_4x4.png` + fade-tuned `wind_trail.particle.json`; prefab sync keys `placement|asset|index`
- MCP `engine_asset_apply` with `kind: particle` (or path ending `.particle.json`) validates, writes, and hot-registers into the live `ParticleSystem`
- Agent recipe catalog: `assets/vfx/recipes/vfx_recipes.json` (torch, campfire_layer, hit_spark, corrupt_aura, dodge_dust, stylized flame variants)
- **Gameplay one-shot bursts:** `ParticleSystem::spawn_burst(asset, world_position, count, emission_direction?)` creates a transient emitter that survives `sync_placements` and is culled when its particles die (play-test dodge uses `assets/vfx/dodge_dust.particle.json`; walk/run/land footstep puffs use `assets/vfx/footstep_dust.particle.json`; bow arrows use `assets/vfx/arrow_trail.particle.json` — faint cool-white wind-rush, ~1 wisp every ~0.22 m of flight — and `arrow_impact_flash` + `arrow_impact` on hit/ground/life end; Ashfell melee `hitFrame` contacts spawn `sword_impact_flash` + `sword_impact`; Runecaster MagicCast uses school charge stacks at the focus tip from `castCharge`→`castRelease`, then a particle-only bolt aimed like the bow. Default `guild_rune_focus` is magenta `arcane_bolt_*`; `magic_fire` / `magic_frost` / `magic_lightning` tags select `fire_bolt_*` / `frost_bolt_*` / `lightning_bolt_*` plus matching charge/swirl emitters)
- Deterministic seed + `particles` CTest suite

Reference UX model: [Roblox ParticleEmitter](https://create.roblox.com/docs/effects/particle-emitters). Layered campfire aims at stylized flame + dark smoke + sparks (readable billboards, not volumetric fire).

## Agent recipes (TICKET-0241)

| Recipe id | Clone these emitters |
| --- | --- |
| `torch` | `wall_torch_{core,flame,embers,smoke}.particle.json` |
| `campfire_layer` | `campfire_{flame,embers,smoke}.particle.json` |
| `hit_spark` | `hit_spark.particle.json` |
| `sword_impact` | `sword_impact_flash` + `sword_impact.particle.json` |
| `corrupt_aura` | `corrupt_aura.particle.json` |
| `dodge_dust` | `dodge_dust.particle.json` (spawn via `spawn_burst`, not prefab attach) |
| `footstep_dust` | `footstep_dust.particle.json` (spawn via `spawn_burst` on stride / land footstep) |
| `arrow_trail` | `arrow_trail.particle.json` (spawn via `spawn_burst` while play-test arrow flies) |
| `arrow_impact` | `arrow_impact_flash` + `arrow_impact.particle.json` (spawn via `spawn_burst` on projectile end) |
| `rune_charge` | `rune_charge_{core,rings,embers}` + `rune_cast_swirl{,_embers}` (tip + foot swirl) |
| `arcane_bolt` | `arcane_bolt_{core,trail,helix,embers,impact_flash,impact}.particle.json` (castRelease magenta spiral bolt) |
| `fire_bolt` | `fire_{charge,cast,bolt}_*` orange fireball (tag `magic_fire`) |
| `frost_bolt` | `frost_{charge,cast,bolt}_*` ice shard (tag `magic_frost`) |
| `lightning_bolt` | `lightning_{charge,cast,bolt}_*` white-gold crack (tag `magic_lightning`) |

Workflow: read recipe → clone JSON via `engine_asset_apply` → attach on prefab `particles[]` → screenshot.

## Out of scope (later EPIC-0010)

- Effect graphs (TICKET-0123), collision/LOD/budgets (0124), editor preview (0125/0137)
- GPU compute simulation, shader graphs
- Timeline `emit` → spawn by name (still host hook / stub)

## Weapon sweep ribbons (foundation)

`WeaponSweepSystem` is the companion path for melee effects that must follow a blade, rather than emit camera-facing sprites. `*.sweep-vfx.json` validates an authored PNG and timing/sample limits, then converts timestamped hilt-to-tip samples into deterministic additive triangle-list geometry. An authored outer-radius aura band and a narrower bright core band make the slash deliberately larger than the physical blade. The first sample is `assets/vfx/ashfell_light_sweep.sweep-vfx.json`; the Ashfell `attack` / `attack2` / `attack3` cut windows now sample its welded hilt and mesh-bound tip. The renderer submits the triangle list through its additive particle pass and reserves a texture slot so regular particle loading cannot overwrite the sweep texture. See [`../formats/weapon-sweep-vfx-assets.md`](../formats/weapon-sweep-vfx-assets.md).

## Integration

| Surface | Behavior |
| --- | --- |
| Prefab | `"particles": [ { "asset": "…", "offset": [x,y,z] }, … ]` (legacy `"particle"` still loads). Scene Inspector + Prefab Editor edit local offset / enabled / asset and Save Prefab. |
| Sample | Campfire / wall torch use stylized molten stacks; `hit_spark` / `corrupt_aura` recipe singles; `wind_trail.particle.json` ambient |
| MCP | `engine_asset_apply` `kind: particle` |
| Runtime | `ParticleSystem` syncs from scene placements each frame; ambient wind when Game/play-test |
| Validate | `engine project validate` loads `*.particle.json` and checks texture files |

## Related

- EPIC-0010 / TICKET-0122; agent apply TICKET-0241
- Act 0 readiness: `coding_particle_system_mvp`, `effects_campfire_flame`
- Foliage `disturbVfxId` remains inert until disturb emitters are authored
- Stylized flame goal: [stylized-flame-goal.md](../art/stylized-flame-goal.md) (TICKET-0242/0243)
