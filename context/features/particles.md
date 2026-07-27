# Particle / VFX system (MVP)

Status: **needs-approval** (TICKET-0122). Roblox-shaped CPU emitters with billboard draw.

## Scope (MVP)

- Authored `*.particle.json` emitters ([format](../formats/particle-emitter-assets.md))
- Prefab `particles[]` (multi-emitter) plus legacy optional `particle` — campfire ships flame + embers + smoke; wall torch ships disc-spread core + flame + embers + smoke (torch-scale)
- Prefab Editor can add/remove emitters, pick `*.particle.json`, edit local offset / enabled, and Save Prefab (offsets are prefab-local and follow placement rotation)
- Scene Inspector lists the same emitters on placed prefabs; **Move with Gizmo** places a translate gizmo on the emitter in the viewport (Save Prefab to persist)
- CPU pool spawn/update with Rate / Lifetime / Speed / Spread / Shape / Color·Size·Transparency sequences / LightEmission
- Soft disc billboards drawn **after SSAO composite** onto the viewport/backbuffer (SoftLight or alpha blend) so emissive fire is not AO-crushed and `lit_color_` stays clean for lighting
- Alpha blend keeps authored `lightEmission` when ≥ 0.25 (bright flame sprites); low-emission smoke still clamps soft
- Optional `texture` PNG path: empty keeps procedural soft disc; non-empty samples RGB×alpha (wind streak, fire/smoke/ember flipbooks)
- Flipbook atlas fields (`flipbookLayout` / `Mode` / `Framerate` / `StartRandom`) remap UVs per particle age (Roblox-shaped grids)
- Flipbook frames **crossfade** on the GPU (adjacent atlas cells lerped) so Loop/PingPong playback doesn’t hard-cut
- Billboard shaping: `aspectRatio`, lifetime `rotation` (+ `rotationStartRandom`), true upright `FacingCameraWorldUp`, `crossedBillboards`, and `minScreenSize` (pixel floor for distant landmarks)
- Soft particles: fade billboards against scene depth so flames sit into logs/terrain instead of hard-clipping (sky/far-plane samples skip the fade so distant fire stays readable)
- Multi-path particle texture slots (up to 8) so campfire layers and wind streak can draw in the same frame; PNG slots reload when file mtime changes
- Camera-local ambient wind emitter (`ParticleSystem::set_ambient_wind`) for meadow wind trails (TICKET-0230); prefab sync keys `placement|asset|index`
- Deterministic seed + `particles` CTest suite

Reference UX model: [Roblox ParticleEmitter](https://create.roblox.com/docs/effects/particle-emitters). Layered campfire aims at stylized flame + dark smoke + sparks (readable billboards, not volumetric fire).

## Out of scope (later EPIC-0010)

- Effect graphs (TICKET-0123), collision/LOD/budgets (0124), editor preview (0125/0137)
- GPU compute simulation, shader graphs
- Timeline `emit` → spawn by name (still host hook / stub)

## Integration

| Surface | Behavior |
| --- | --- |
| Prefab | `"particles": [ { "asset": "…", "offset": [x,y,z] }, … ]` (legacy `"particle"` still loads). Scene Inspector + Prefab Editor edit local offset / enabled / asset and Save Prefab. |
| Sample | Campfire: `campfire_{flame,embers,smoke}.particle.json` + matching `*_flipbook_4x4.png`; wall torch: `wall_torch_{core,flame,embers,smoke}.particle.json` (disc spawn + spread); `wind_trail.particle.json` ambient |
| Runtime | `ParticleSystem` syncs from scene placements each frame; ambient wind when Game/play-test |
| Validate | `engine project validate` loads `*.particle.json` |

## Related

- EPIC-0010 / TICKET-0122
- Act 0 readiness: `coding_particle_system_mvp`, `effects_campfire_flame`
- Foliage `disturbVfxId` remains inert until disturb emitters are authored
