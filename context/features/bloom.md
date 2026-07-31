# Bloom post-process (v1)

Status: **needs-approval** (TICKET-0242)

LDR bright-pass glow after SSAO composite and particle draw so fire cores, sparks, and emissive materials bleed soft light.

## Pipeline order

```
world → water → SSAO composite → particles → bloom → present/ImGui
```

Bloom after particles is intentional: stylized fire billboards must contribute to the bright-pass.

## Algorithm

1. Copy destination into `water_scene_color_` (free after water) for sampling.
2. Half-res extract: luma above `k_bloom_threshold` → `bloom_a_`.
3. Separable Gaussian-ish blur (`bloom_a_` ↔ `bloom_b_`).
4. Additive composite onto destination with `k_bloom_intensity`.

Tunables (compile-time, `render_app.cpp`): `k_bloom_threshold` (0.52), `k_bloom_intensity` (0.42), `k_bloom_soft_radius` (1.35).

## Budget / fail-closed

- Half-res R8G8B8A8 ping-pong (same size as SSAO AO target) — aimed at mid-range 1440p/60 with SSAO.
- Missing bloom PSO/RTs → `apply_bloom` no-ops (editor stays usable).
- No HDR / tonemap overhaul (out of scope).

## Related

- TICKET-0042 SSAO stack
- Stylized flame: [`stylized-flame-vfx.md`](stylized-flame-vfx.md), TICKET-0243
