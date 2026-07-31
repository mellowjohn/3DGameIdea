# TICKET-0238: Material shader profiles + emissive pulse

- Epic: EPIC-0005
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/3abd3efc56958126aaf9fe2d324ac851

## Goal

Let MCP/agents author richer opaque looks by selecting an engine-owned `shader` profile and pulse parameters in `.material.json`, without writing HLSL or a node graph.

## Context links

- [DEC-0049](../../decisions/index.md#dec-0049-agent-writable-material-shader-profiles)
- `context/formats/materials.md`
- `context/features/material-shader-profiles.md`
- `context/architecture/content-vs-engine-workflows.md`
- Related: TICKET-0040 (opaque PBR), TICKET-0041 (strategy)

## Acceptance criteria

- [x] `.material.json` supports optional `shader` enum: at least `stylized_opaque` (default) and `emissive_magic`; unknown values fail validation with a stable `MATERIAL-SHADER-*` error code.
- [x] `emissive_magic` materials accept optional `emissivePulseHz` (≥0) and `emissivePulseMin` in `[0,1]`; when Hz > 0, runtime emissive intensity pulses over time (visible on prefab parts using the material).
- [x] Existing materials without `shader` round-trip unchanged and render as today’s opaque PBR (`stylized_opaque`).
- [x] Sample asset: `samples/open-world-rpg/assets/materials/rune_glow.material.json` using `emissive_magic`.
- [x] Named `assets` suite covers: default shader parse, unknown shader rejected, pulse field validation, `from_material` pulse params.
- [x] `context/formats/materials.md` + feature note + features index updated; `engine_asset_apply` kind `material` continues to create/update these fields.
- [x] Rebuild `engine` succeeds.

## Out of scope

- Node-based shader graphs.
- Masked/blended draw (TICKET-0239).
- Material texture map paths (TICKET-0240).
- Particle MCP / recipes (TICKET-0241).
- New lighting models (toon ramps, subsurface).
- Runtime Lua `set_material_param` overrides (follow-on).

## Dependencies

Blocked by: TICKET-0041 decision (DEC-0049) — satisfied. Soft: TICKET-0040 opaque PBR. Parallel OK with TICKET-0191.

## Verification

- Rebuild `engine` + `engine_suite_tests`: succeeded (3 existing warnings unrelated).
- `engine_suite_tests.exe --suite assets`: 84/84 passed.
- `engine validate --project samples/open-world-rpg`: valid.
- Desktop pulse check: optional owner QA — assign `rune_glow` to a compositional prefab part.

## What changed

- Summary: Materials can select `shader` profiles for MCP/agent look authorship. `emissive_magic` pulses emissive over time via CPU packing into object/instance constants. Sample `rune_glow.material.json` ships for cloning.
- Files / surfaces touched: `material_asset.h/.cpp`, `pbr_lighting.h`, `render_app.cpp` (pack paths), `suite_tests.cpp`, formats/features docs, sample material.
- Schema / API / format deltas: `shader`, `emissivePulseHz`, `emissivePulseMin`; errors `MATERIAL-SHADER-UNKNOWN`, `MATERIAL-PULSE-*`.
- Seed / sample data: `assets/materials/rune_glow.material.json` (not auto-assigned to a world prop).
- Tests / verification evidence: assets 84/84; project validate OK; engine rebuild OK.
- Decisions & tradeoffs: CPU-side pulse (not Frame CB time) keeps HLSL unchanged; pulse params only apply for `emissive_magic`.
- Leftover risk / follow-ons: masked (0239), material maps (0240), particle MCP (0241); editor inspector may not yet expose new fields in ImGui (JSON/MCP still work).

## Agent notes

Editor/MCP process was reset after rebuild; build lease released.
