# TICKET-0219: Cascaded directional sun shadows (CSM v1)

- Epic: EPIC-0005
- Status: needs-approval
- Agent: cursor-agent
- Priority: P1
- Notion: https://app.notion.com/p/3a6d3efc569581fe86a5e36411b53adc

## Goal

Add a **cheap cascaded shadow map** for the main directional sun so outdoor contact shading is real shadows, not only face-normal darkening / SSAO — the largest perceived quality jump called out in the 2026-07-23 quality/perf audit.

## Context links

- `context/planning/epics.md` (EPIC-0005)
- `context/art/visual-direction.md` — dark-fantasy outdoor lighting
- `context/features/csm-shadows.md` — **feature note**
- `context/features/index.md` — CSM + atmosphere rows
- `include/engine/rendering/csm_shadows.h` — cascade splits / bias knobs
- `src/rendering/render_app.cpp` — shadow pass + opaque/foliage sample
- Quality/perf audit (owner 2026-07-23): gap #2
- Informed by: TICKET-0139 budgets; TICKET-0042 SSAO (keep complementary)

## Acceptance criteria

- [x] Directional light casts cascaded shadow maps (3 cascades) sampled in opaque + foliage PBR paths.
- [x] Defaults tuned for stylized outdoor; bias documented in `csm_shadows.h` / feature note.
- [x] Terrain, placed prefabs, and character meshes receive shadows; sky undarkened. (Foliage receives; blade casting deferred for v1.)
- [x] Editor Game + Scene viewports show shadows; debug_world + editor smoke exit 0 (no D3D12 validation failures observed).
- [x] Feature note `context/features/csm-shadows.md` + `features/index.md` row; materials.md cross-link.
- [x] Desktop QA capture: `samples/open-world-rpg/out/captures/csm-shadows-game.png` (hillside contact / directional darkening).
- [x] Rebuild `engine` succeeds; `debug_world_smoke` / `editor_smoke` equivalents pass (exit 0).

## Out of scope

- Lumen / ray-traced GI / contact hardening research quality
- Point-light shadows
- Transparent/masked shadow casters (fail-closed OK for v1)
- Temporal shadow filtering / VSM / ESM research paths
- Instanced foliage blade casting (receive-only in v1)

## Dependencies

- Owner override 2026-07-23: new **active P1** on quality/perf gap track.
- Parallel OK with TICKET-0139 (measure before/after if harness exists).
- Do not regress SSAO composite; shadows + AO should coexist.

## Verification

Rebuild `engine` Debug. Run:

- `engine run --project samples/open-world-rpg --debug-world --frames 30 --hidden true --json` → exit 0
- `engine editor --project samples/open-world-rpg --frames 2 --hidden true --json` → exit 0
- Capture: `engine editor --project samples/open-world-rpg --frames 50 --hidden true --viewport game --output .../csm-shadows-game.ppm`

## What changed

### Summary

Shipped **CSM v1**: 3 cascades (1024² array), depth-only caster pass for terrain + placed meshes, 3×3 PCF sample on opaque + foliage sun lighting, bias constants in `csm_shadows.h`. SSAO composite unchanged. Desktop Game-viewport capture shows directional hillside shading.

### Files / surfaces

- `include/engine/rendering/csm_shadows.h` (new)
- `src/rendering/render_app.cpp` — shadow resources/PSO/pass; Frame/root sig extensions; opaque+foliage sample
- `context/features/csm-shadows.md`, `context/features/index.md`, `context/formats/materials.md`
- `context/planning/epics.md`, this stub
- QA: `samples/open-world-rpg/out/captures/csm-shadows-game.{ppm,png}`

### Schema / API

No authored asset schema change. Runtime shadow CB register `b3`, shadow map `t1` / comparison `s1`.

### Verification evidence

- Rebuild Debug OK (C4996 getenv only)
- debug_world 30f exit 0 (~0.6 ms GPU labeled)
- editor 2f exit 0
- Capture 50f Game play-test: GPU ~3.2 ms (CSM cost vs ~0.6–1.7 ms prior paths); PNG written

### Decisions

- Sphere-fit cascades + texel snap (stable stylized outdoor)
- Foliage receive-only for v1 (cheap; trees still cast as placed meshes)
- Hidden frame-limit hard-exit also covers capture paths (smoke/teardown hang)

### Leftover risk

- Cascade swimming / acne may need bias retune on 4070-class Release
- No foliage-blade casting; dense grass won’t self-shadow
- Dual Scene+Game editor pass doubles CSM cost when both viewports draw

## Agent notes

Ready for owner approval. Next gap ticket: TICKET-0220 (foliage frustum cull + mesh LOD).
