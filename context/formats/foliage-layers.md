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
      "color": [0.14, 0.22, 0.1],
      "scaleMin": 0.55,
      "scaleMax": 1.0,
      "densityMultiplier": 0.15,
      "maxSlopeRatio": 0.55,
      "bendStrength": 0.35,
      "bendRadius": 1.2,
      "bladeHeight": 0.55,
      "disturbVfxId": "grass_walk"
    }
  ]
}
```

- `layers[]`: ordered palette entries referenced by `layer` indices in `assets/terrain/foliage-density.json`.
- `meshKind`: built-in stylized primitives generated at runtime (`grass_blade`, `grass_clump`, `flower_clump`, `bush`, `bush_wide`, `bush_tall`). Prefer `grass_blade` for faceted single-strand ground cover; use bush variants for discrete shrub layers.
- `densityMultiplier`: scales painted sample density (0–255) into instance count per sample for `ground_cover` layers. Single blades typically need a higher multiplier than multi-blade clumps. Ignored for `discrete` layers.
- `maxSlopeRatio`: rejects samples where estimated terrain slope exceeds this ratio.
- `scatterMode` (optional, default `ground_cover`): `ground_cover` multiplies density into many instances per sample; `discrete` places at most one instance per sample when painted density meets `discreteMinDensity`.
- `discreteMinDensity` (optional, default `64`): minimum painted density (0–255) required before a `discrete` layer spawns an instance. Use higher values for larger props such as tall bushes.
- `bendStrength` (optional, default `0.35` grass / `0.1` flowers): vertex-shader bend amount when a `WorldInfluence` source is nearby.
- `bendRadius` (optional, default `1.2`): influence radius in meters for bend falloff.
- `bladeHeight` (optional, default `0.55`): nominal blade height used to weight bend toward the tip.
- `disturbVfxId` (optional, default empty): forward hook for future particle/VFX disturb effects; inert until the VFX milestone.

## Companion data

Painted density masks live in `assets/terrain/foliage-density.json` ([format](foliage-density.md)).

## Validation

`engine validate --project <project>` loads the layer palette when present and rejects malformed schema, duplicate ids, and invalid tuning ranges. When density data exists, layer indices must stay within the palette size.
