# Weapon sweep VFX assets (v1)

Status: active foundation (TICKET-0126 owner override, 2026-08-10).

`*.sweep-vfx.json` describes a short-lived, weapon-following additive ribbon. Runtime samples the equipped weapon's hilt and tip during an active attack, then emits world-space triangles between successive samples.

```json
{
  "schemaVersion": 1,
  "id": "ashfell_light_sweep",
  "texture": "assets/vfx/generated/sword_sweep.png",
  "color": [1.0, 0.78, 0.28, 1.0],
  "coreColor": [1.0, 0.98, 0.90, 1.0],
  "innerRadius": 0.10,
  "outerRadius": 1.85,
  "coreInnerRadius": 0.42,
  "coreOuterRadius": 1.38,
  "lightEmission": 0.95,
  "trailSeconds": 0.14,
  "maxSamples": 12
}
```

`texture` must be a project-relative existing PNG: traversal and missing files fail closed with `SWEEP-TEXTURE-PATH-INVALID` and `SWEEP-TEXTURE-MISSING`. `trailSeconds` is positive; `maxSamples` is 2–64.
