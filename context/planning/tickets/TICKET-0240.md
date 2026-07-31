# TICKET-0240: Material texture slots (albedo/emissive maps)

- Epic: EPIC-0005
- Status: needs-approval
- Agent: cursor-agent-0240
- Priority: P2
- Notion: https://app.notion.com/p/3abd3efc569581e1bd3ed904c1e3aabc

## Goal

Allow `.material.json` to reference project-relative albedo and emissive texture paths so MCP/agents can tint and glow surfaces without embedding maps only in glTF.

## Context links

- `context/formats/materials.md`
- TICKET-0191 (glTF albedo — complementary)
- TICKET-0238 (shader profiles)
- [DEC-0049](../../decisions/index.md#dec-0049-agent-writable-material-shader-profiles)

## Acceptance criteria

- [x] Optional `albedoMap` / `emissiveMap` (or equivalent documented names) on materials; empty = current behavior.
- [x] Paths validated as project-relative; missing file fails closed with stable error at validate/load (or documented deferred GPU white + diagnostic — pick one and test it).
- [x] Opaque (and masked if 0239 landed) draws sample albedo when set; emissive map modulates or adds per docs.
- [x] Sample material using a project PNG; `assets` suite round-trip + invalid path rejection.
- [x] Docs + features index; MCP `engine_asset_apply` material writes support the fields.
- [x] Rebuild `engine` succeeds.

## Out of scope

- Normal / metallic-roughness maps (follow-on).
- Shader graphs.
- Atlasing / mip policy beyond existing mesh albedo path.

## Dependencies

Soft: TICKET-0191 albedo sampler infrastructure. Soft: TICKET-0238. Parallel OK with TICKET-0239.

## Verification

- Rebuild `engine` + `engine_suite_tests`: OK (existing warnings only).
- `engine_suite_tests.exe --suite assets`: 92/92 passed.
- `engine validate --project samples/open-world-rpg`: valid.

## What changed

- Summary: Materials may set optional project-relative `albedoMap` / `emissiveMap` PNGs. Validate/MCP fail closed on bad or missing paths. Runtime samples albedo maps (prefers material over mesh albedo); emissiveMap v1 multiplies authored emissive by average RGB of the PNG.
- Files / surfaces touched: `material_asset.h/.cpp`, `command.cpp` validate, `editor_session` MCP material apply + cache, `render_app.cpp` upload/bind/batch, suite tests, materials docs, samples `leaf_cutout` / `rune_glow`.
- Schema / API / format deltas: `albedoMap`, `emissiveMap` strings; errors `MATERIAL-MAP-PATH-INVALID`, `MATERIAL-MAP-MISSING`.
- Seed / sample data: `leaf_cutout` → `assets/vfx/wind_streak.png`; `rune_glow` → `assets/vfx/fire_flipbook_4x4.png`.
- Tests / verification evidence: assets 92/92; validate OK.
- Decisions & tradeoffs: fail-closed missing maps; emissiveMap UV sampling deferred — average RGB modulate for agent-friendly v1.
- Leftover risk / follow-ons: UV-sampled emissive maps; normal/MR maps; particle MCP (0241); bloom/flame (0242/0243).

## Agent notes

Build lease acquired/released (`cursor-agent-0240`, token `a0c582d660a8-1`). Editor/MCP killed for rebuild and restarted after release.
