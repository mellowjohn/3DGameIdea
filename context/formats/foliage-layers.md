# Foliage Layer Palette

Ground-cover layer definitions for density-painted foliage. Stored at `assets/foliage/ground-cover.layers.json`.

## Schema v1

```json
{
  "schemaVersion": 1,
  "layers": [
    {
      "id": "grass",
      "label": "Grass",
      "meshKind": "grass_blade",
      "color": [0.227, 0.251, 0.157],
      "scaleMin": 0.9,
      "scaleMax": 1.45,
      "densityMultiplier": 0.16,
      "maxSlopeRatio": 0.55,
      "bendStrength": 0.5,
      "bendRadius": 1.5,
      "bladeHeight": 0.7,
      "disturbVfxId": "grass_walk"
    }
  ]
}
```

- `layers[]`: ordered palette entries referenced by `layer` indices in `assets/terrain/foliage-density.json`.
- `meshKind`: built-in stylized primitives generated at runtime (`grass_blade`, `grass_clump`, `flower_clump`, `bush`, `bush_wide`, `bush_tall`). Prefer `grass_blade` for ground cover — each instance is a **tuft** of ~9 tapered multi-segment blades sharing a root (BOTW-style clumps), height ~0.7. Use bush variants for discrete shrub layers. `grass_clump` remains a legacy multi-wedge primitive.
- `densityMultiplier`: scales painted sample density (0–255) into instance count per sample for `ground_cover` layers. Single blades typically need a higher multiplier than multi-blade clumps. Ignored for `discrete` layers.
- `maxSlopeRatio`: rejects samples where estimated terrain slope exceeds this ratio.
- `scatterMode` (optional, default `ground_cover`): `ground_cover` multiplies density into many instances per sample; `discrete` places at most one instance per sample when painted density meets `discreteMinDensity`.
- `discreteMinDensity` (optional, default `64`): minimum painted density (0–255) required before a `discrete` layer spawns an instance. Use higher values for larger props such as tall bushes.
- `bendStrength` (optional, default `0.35` grass / `0.1` flowers): vertex-shader bend amount when a `WorldInfluence` source is nearby. Bend is **tip-squared lean** away from the player plus a light **trample** (height squash under feet), not a flat XZ translate.
- `bendRadius` (optional, default `1.2`): influence radius in meters for bend falloff.
- `bladeHeight` (optional, default `0.55`): nominal blade height used to weight lean/trample/wind toward the tip; keep aligned with the `grass_blade` mesh height (~0.7).
- `disturbVfxId` (optional, default empty): forward hook for future particle/VFX disturb effects; inert until the VFX milestone.

## Runtime motion (foliage VS)

- **Interaction:** `WorldInfluenceBus` feeds position/velocity/radius/strength into Interaction CB `b2`. Lean rotates tip verts around the root; falloff is strongest at the tip (`t²`).
- **Ambient wind:** cheap tip flutter from `time_seconds` + instance XZ phase (scaled by `WindFieldParams::ambient_amp`). Amplitude is muted when interaction falloff is high so walk-through stays readable.
- **Gust trails (TICKET-0230):** traveling tip lean along a shared directional wind field (`windDir` / `gustWavelength` / `gustStrength` / `gustSharpness`). Envelope matches `engine::wind_gust_envelope` so meadows read as coherent sweeping bands.
- No skeletal bones and no scatter rebuild on player move (DEC-0013).

## Companion data

Painted density masks live in `assets/terrain/foliage-density.json` ([format](foliage-density.md)).

## Validation

`engine validate --project <project>` loads the layer palette when present and rejects malformed schema, duplicate ids, and invalid tuning ranges. When density data exists, layer indices must stay within the palette size.
