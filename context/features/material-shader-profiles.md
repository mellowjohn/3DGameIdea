# Material shader profiles

Status: active (TICKET-0238)

Agent- and MCP-facing surface looks use **engine-owned master shaders** selected by a JSON `shader` field — not a node graph ([DEC-0049](../decisions/index.md#dec-0049-agent-writable-material-shader-profiles)).

## Profiles (v1)

| `shader` | Behavior |
| --- | --- |
| `stylized_opaque` | Default. Lit PBR (TICKET-0040). Works with `opacityMode` opaque or masked. |
| `emissive_magic` | Lit PBR plus optional emissive pulse (`emissivePulseHz`, `emissivePulseMin`). |

Unknown profiles fail validation (`MATERIAL-SHADER-UNKNOWN`).

Masked cutout uses `opacityMode: masked` + `opacityCutoff` (TICKET-0239), independent of `shader`.

## Authoring

- Create/update via `engine_asset_apply` (`kind: material`) or Asset Browser.
- Prefer cloning a sample (e.g. `assets/materials/rune_glow.material.json`, `leaf_cutout.material.json`) and tweaking colors/pulse/cutoff.
- Screenshot loop: apply → `engine_editor_screenshot` → adjust JSON.

## Texture maps (TICKET-0240)

Optional `albedoMap` / `emissiveMap` project-relative PNGs on `.material.json`. Missing files fail closed. Albedo maps are sampled at draw; emissiveMap v1 multiplies authored emissive by average RGB (see `context/formats/materials.md`).

## Follow-ons

- Particle MCP + recipes: TICKET-0241 (shipped — see `particles.md`)
- Bloom + stylized flame hero: TICKET-0242 / 0243 ([stylized-flame-goal.md](../art/stylized-flame-goal.md))
