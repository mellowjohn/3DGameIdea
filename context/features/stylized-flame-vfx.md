# Stylized flame VFX

Status: **needs-approval** (TICKET-0243)

Agent-clonable fire looks matching owner refs (blob mesh + particle trio) via DEC-0049 masters + recipes — not a shader graph.

## References

| Family | Path | Engine approach |
| --- | --- | --- |
| Blob / mesh | [`../art/reference/stylized-flame-blob-mesh.png`](../art/reference/stylized-flame-blob-mesh.png) | Campfire / torch mesh keeps glTF albedo. **Do not** apply `flame_core_emissive.material.json` to the whole prop — that path floods every face with pulsed emissive and reads as a solid yellow model. Bright core comes from flipbook particle layers + warm point light. True noise-displace/dissolve master deferred. |
| Particle trio | [`../art/reference/stylized-flame-particle-trio.png`](../art/reference/stylized-flame-particle-trio.png) | Layered recipes: molten / wispy / column + smoke + sparks. |

## Hero sample

Place: `assets/prefabs/Scene Assets/stylized_flame.prefab.json`

Production prefabs use the same look:
- `campfire.prefab.json` — molten + sparks + smoke (mesh keeps glTF texture)
- `wall_torch.prefab.json` — torch-scale `wall_torch_molten` + embers + smoke (mesh keeps glTF texture)

- Mesh: `campfire.gltf` / `wall_torch.gltf` (default baked textures; no whole-mesh emissive override)
- Particles: textured core + `stylized_flame_molten` / `wall_torch_molten` + sparks + smoke
- Point light: warm orange (moderate radius/strength so it does not wash the prop)
- Bloom (TICKET-0242) softens sparks; keep mesh lit by albedo + light only

## Recipes

See `samples/open-world-rpg/assets/vfx/recipes/vfx_recipes.json`:

| Recipe id | Intent |
| --- | --- |
| `stylized_flame_molten` | Blobby crossed billboards (hero default) |
| `stylized_flame_wispy` | Narrow ribbon / wind-blown |
| `stylized_flame_column` | Tall cylinder column |

Clone via `engine_asset_apply` (`kind: particle`) or swap emitters on the hero prefab.

## Building blocks

- Masked cutout: TICKET-0239
- Material maps: TICKET-0240
- Particle MCP: TICKET-0241
- Bloom: TICKET-0242 / [`bloom.md`](bloom.md)

## Provenance

Flipbooks already recorded in [`../resources/index.md`](../resources/index.md) (project-owned `gen_fire_flipbook.py`). No new third-party textures.

## Follow-ons

- Dedicated `stylized_flame` master with noise displace + dissolve bands (closer to blob-mesh ref).
- One-shot burst emit API for hit sparks.
