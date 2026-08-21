# Cascaded directional sun shadows (CSM v1)

Status: needs-approval (TICKET-0219)

Cheap outdoor cascaded shadow maps for the main directional sun so contact under characters, trees, and terrain reads as real shadow rather than SSAO-only darkening.

## Behavior

- **3 cascades** (sphere-fit around the camera, texel-snapped) at ~22 / 75 / 220 m split distances (`include/engine/rendering/csm_shadows.h`).
- **1024²** `Texture2DArray` depth atlas; depth-only caster pass before each world pass.
- **Casters (v1):** terrain cells + placed prefab/character meshes. Each cascade conservatively rejects caster AABBs outside its light-space frustum before submission. Instanced foliage blades receive shadows but do not cast (fail-closed for cheap v1).
- **Prop caster submit:** depth-only props are **GPU-instanced** per mesh+skin (`StructuredBuffer` of model rows + `DrawInstanced`), matching the lit prop path. Terrain still uses a single root-constant model.
- **Instance rows live in a per-frame arena.** Caster rows (and lit prop rows) are bump-allocated out of `InstanceArena`, which reserves one slot per in-flight frame and hands each pass a fresh offset folded into the root SRV address. Rewriting offset 0 per pass let the CPU clobber rows the GPU was still reading — for the previous frame (only frame N-2 is waited on) and for the previous pass in the same frame — which showed up as props and cast shadows flickering. Set `ENGINE_LOD_TRACE=1` to log LOD and caster-set churn when a similar artifact is suspected.
- **Scene free-cam budget:** editor Scene / menu world pass sets `shadow_props_all_cascades=false` so props only cast into **cascade 0** (near); mid/far cascades stay terrain-only. Game / play-test still casts props into all cascades.
- **Receivers:** opaque PBR (terrain, props, characters) and foliage PBR — sun `shadePbr` term multiplied by a 3×3 PCF shadow factor. Ambient + point lights stay unshadowed. Sky path undarkened.
- **Bias:** constant depth bias + normal offset + rasterizer slope scale — documented next to SSAO knobs / in `csm_shadows.h`.
- **SSAO:** unchanged post composite; shadows and AO coexist.

## Rendering practice notes (CSM submit)

Industry defaults that apply here:

1. **Depth-only shadow passes batch aggressively** — no materials/textures; group by mesh (and skin palette). One instanced draw beats thousands of single-instance draws on the CPU.
2. **Cull per cascade frustum** (already done) — do not submit every caster into every cascade.
3. **Later:** Hi-Z occlusion, shadow LOD / proxy meshes, per-cascade instance lists with overlap only at split edges. **Foliage lit path** now uses GPU frustum cull + `ExecuteIndirect` ([`gpu-instance-cull.md`](gpu-instance-cull.md) / TICKET-0276); prop/shadow caster GPU cull remains follow-on.

## Authoring / knobs

| Constant | Location | Default |
| --- | --- | --- |
| Cascade splits | `csm::k_split_distances` | 22 / 75 / 220 m |
| Map resolution | `csm::k_map_resolution` | 1024 |
| Depth / normal bias | `csm::k_depth_bias`, `k_normal_bias_meters` | 0.0018 / 0.05 m |
| PCF soft radius | `csm::k_pcf_soft_texels` | 1.35 |
| Slope depth bias | `csm::k_raster_slope_scaled_depth_bias` | 2.5 |
| Sun travel dir | `csm::k_sun_travel` (matches Frame CB) | (-0.40, -0.85, -0.30) |

## Out of scope (v1)

Point-light shadows, transparent/masked casters, VSM/ESM, temporal filtering, contact-hardening research quality.

## Related

- Ticket: [`../planning/tickets/TICKET-0219.md`](../planning/tickets/TICKET-0219.md)
- SSAO: TICKET-0042 (complementary)
- Visual direction: [`../art/visual-direction.md`](../art/visual-direction.md)
- Perf gate: [`../benchmarks/open-world-1440p.md`](../benchmarks/open-world-1440p.md)
