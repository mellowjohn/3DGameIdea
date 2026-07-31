# TICKET-0241: Particle MCP apply + VFX recipes

- Epic: EPIC-0005
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/3abd3efc56958140a30dc4be708e4dfd

## Goal

Let agents create/update `*.particle.json` through MCP and pick from a small documented recipe catalog so VFX composition matches materials’ agent-friendly JSON workflow.

## Context links

- `context/formats/particle-emitter-assets.md`
- `context/features/particles.md`
- `context/features/mcp-live-editor.md`
- [DEC-0049](../../decisions/index.md#dec-0049-agent-writable-material-shader-profiles) (same authorship philosophy)

## Acceptance criteria

- [x] `engine_asset_apply` (or documented sibling) supports `kind: particle` create/update with validation + catalog refresh.
- [x] Invalid particle JSON returns stable error codes; valid write hot-reloads or refreshes like materials.
- [x] Recipe catalog doc (or JSON index under `assets/vfx/recipes/`) lists at least: torch, campfire layer, hit spark, corrupt aura — each pointing at sample emitter paths agents may clone.
- [x] Context: particles.md + mcp-live-editor.md + features index updated.
- [x] Suite or validate covers particle apply path when testable headless.
- [x] Rebuild `engine` if C++ changed.

## Out of scope

- GPU compute particles.
- Shader graphs for VFX.
- Full visual effect graph editor.

## Dependencies

Soft: existing particle MVP (TICKET-0122 / 0230). Parallel OK with TICKET-0238–0240.

## Verification

- Rebuild `engine` + `engine_suite_tests`: OK (existing warnings only).
- `engine_suite_tests.exe --suite particles`: 64/64 passed.
- `engine validate --project samples/open-world-rpg`: valid.

## What changed

- Summary: MCP `engine_asset_apply` supports `kind: particle`; texture paths fail closed; live `ParticleSystem` hot-registers on write; recipe catalog + hit_spark/corrupt_aura samples.
- Files / surfaces touched: `particle_emitter_asset`, `editor_session` apply path, `command` validate, `mcp_server` description, `render_app` session wiring, suite tests, particles/mcp docs, samples.
- Schema / API / format deltas: `PARTICLE-TEXTURE-PATH-INVALID`, `PARTICLE-TEXTURE-MISSING`; `validate_texture(project_root)`.
- Seed / sample data: `assets/vfx/recipes/vfx_recipes.json`, `hit_spark.particle.json`, `corrupt_aura.particle.json`.
- Tests / verification evidence: particles 64/64; validate OK.
- Decisions & tradeoffs: recipe index is JSON under `assets/vfx/recipes/` (not a runtime asset type); torch/campfire recipes point at existing layered emitters.
- Leftover risk / follow-ons: bloom (0242), stylized flame hero (0243); one-shot emit API still host/stub.

## Agent notes

Build lease acquired/released (`cursor-agent-0241`, token `a11b2cb75898-2`). Editor/MCP killed for rebuild and restarted after release.
