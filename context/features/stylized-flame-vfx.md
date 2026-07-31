# Stylized flame VFX

Status: **needs-approval** (TICKET-0243)

Agent-clonable fire looks matching owner refs (blob mesh + particle trio) via DEC-0049 masters + recipes — not a shader graph.

## References

| Family | Path | Engine approach |
| --- | --- | --- |
| Blob / mesh | [`../art/reference/stylized-flame-blob-mesh.png`](../art/reference/stylized-flame-blob-mesh.png) | Campfire mesh + `flame_core_emissive.material.json` (`emissive_magic` pulse + `emissiveMap`). True noise-displace/dissolve master deferred; masked/maps/bloom already help readability. |
| Particle trio | [`../art/reference/stylized-flame-particle-trio.png`](../art/reference/stylized-flame-particle-trio.png) | Layered recipes: molten / wispy / column + smoke + sparks. |

## Hero sample

Place: `assets/prefabs/Scene Assets/stylized_flame.prefab.json`

Production prefabs use the same look:
- `campfire.prefab.json` — molten + sparks + smoke + `flame_core_emissive`
- `wall_torch.prefab.json` — torch-scale `wall_torch_molten` + embers + smoke + emissive material

- Mesh: `campfire.gltf` with `flame_core_emissive`
- Particles: core + `stylized_flame_molten` + `hit_spark` + smoke
- Point light: warm orange
- Bloom (TICKET-0242) softens the emissive core and sparks

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
