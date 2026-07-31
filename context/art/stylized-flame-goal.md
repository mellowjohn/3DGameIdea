# Stylized flame VFX (goal)

Status: needs-approval (TICKET-0243) — see [`../features/stylized-flame-vfx.md`](../features/stylized-flame-vfx.md)

Owner references for post–material-path VFX polish:

| Reference | Path | Intent |
| --- | --- | --- |
| Blob / mesh fire | [`reference/stylized-flame-blob-mesh.png`](reference/stylized-flame-blob-mesh.png) | Volume mesh + noise displace, banded emissive, dissolve shred, bloom, ember particles |
| Particle trio | [`reference/stylized-flame-particle-trio.png`](reference/stylized-flame-particle-trio.png) | Layered billboard flame + smoke + sparks; wispy / molten / column variants |

## Engine approach (DEC-0049)

- **Not** a shader graph — code-first masters + JSON particle recipes agents can MCP-clone.
- Building blocks: masked/dissolve (0239), material maps/noise (0240), particle MCP/recipes (0241), bloom (0242), hero prefab (0243).
- Hero: `assets/prefabs/Scene Assets/stylized_flame.prefab.json`

## Related

- [`../features/stylized-flame-vfx.md`](../features/stylized-flame-vfx.md)
- [`../features/bloom.md`](../features/bloom.md)
- [`../features/material-shader-profiles.md`](../features/material-shader-profiles.md)
- [`../features/particles.md`](../features/particles.md)
