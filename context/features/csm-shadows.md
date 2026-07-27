# Cascaded directional sun shadows (CSM v1)

Status: needs-approval (TICKET-0219)

Cheap outdoor cascaded shadow maps for the main directional sun so contact under characters, trees, and terrain reads as real shadow rather than SSAO-only darkening.

## Behavior

- **3 cascades** (sphere-fit around the camera, texel-snapped) at ~22 / 75 / 220 m split distances (`include/engine/rendering/csm_shadows.h`).
- **1024²** `Texture2DArray` depth atlas; depth-only caster pass before each world pass.
- **Casters (v1):** terrain cells + placed prefab/character meshes. Each cascade conservatively rejects caster AABBs outside its light-space frustum before submission. Instanced foliage blades receive shadows but do not cast (fail-closed for cheap v1).
- **Receivers:** opaque PBR (terrain, props, characters) and foliage PBR — sun `shadePbr` term multiplied by a 3×3 PCF shadow factor. Ambient + point lights stay unshadowed. Sky path undarkened.
- **Bias:** constant depth bias + normal offset + rasterizer slope scale — documented next to SSAO knobs / in `csm_shadows.h`.
- **SSAO:** unchanged post composite; shadows and AO coexist.

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
