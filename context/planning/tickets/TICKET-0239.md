# TICKET-0239: Masked cutout material draw path

- Epic: EPIC-0005
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/3abd3efc5695814bb7e2ee14fd66c3fa

## Goal

Draw materials with `opacityMode: masked` using `opacityCutoff` so agents can author leaves, banners, and cutout props instead of fail-closed skip.

## Context links

- `context/formats/materials.md` (opacityMode / opacityCutoff)
- TICKET-0143 (masked deferred)
- TICKET-0238 (shader profiles)
- [DEC-0049](../../decisions/index.md#dec-0049-agent-writable-material-shader-profiles)

## Acceptance criteria

- [x] Prefab parts with validated masked materials draw (no longer skipped solely for `masked`).
- [x] Pixel/alpha test uses `opacityCutoff`; fragments below cutoff are discarded.
- [x] Opaque materials unchanged; blended still fail-closed or routed only if water path already applies.
- [x] Sample masked material + at least one suite or desktop screenshot note.
- [x] Docs: materials.md runtime support + features index.
- [x] Rebuild `engine` succeeds.

## Out of scope

- Full blended transparency sorting (non-water).
- Shader graphs.
- Material map slots beyond what 0240 adds (may soft-depend on albedo alpha from texture).

## Dependencies

Soft: TICKET-0238 for `shader` field consistency. Soft: TICKET-0240 if cutout needs texture alpha (may use vertex color alpha interim — document choice).

## Verification

- Rebuild `engine` + `engine_suite_tests`: OK (existing warnings only).
- `engine_suite_tests.exe --suite assets`: 86/86 passed.
- `engine validate --project samples/open-world-rpg`: valid.
- Desktop: assign `leaf_cutout` to a textured mesh with alpha for visible holes; untextured uses `baseColor.a`.

## What changed

- Summary: Masked materials draw with GPU alpha clip. Texture alpha (or baseColor.a) vs opacityCutoff. Blended non-water still skipped. Sample `leaf_cutout.material.json`.
- Files / surfaces touched: `pbr_lighting.h`, `render_app.cpp` (pack + mesh/prop PS), suite tests, materials docs, features index, sample material.
- Schema / API / format deltas: runtime behavior only; schema fields already existed.
- Seed / sample data: `assets/materials/leaf_cutout.material.json`.
- Tests / verification evidence: assets 86/86; validate OK.
- Decisions & tradeoffs: clip in existing opaque PSOs (no separate masked PSO); alpha from albedo tex or packed baseColor.a until material maps (0240).
- Leftover risk / follow-ons: visible leaf cutouts need alpha textures; stylized flame goal 0242/0243 queued.

## Agent notes

Editor killed for rebuild; build lease released. Stylized flame goal tickets 0242/0243 added this session.
