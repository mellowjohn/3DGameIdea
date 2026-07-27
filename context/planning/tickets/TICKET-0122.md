# TICKET-0122: CPU/GPU emitter foundation + pooling

- Epic: EPIC-0010
- Status: needs-approval
- Agent: cursor-agent
- Priority: P1
- Notion: https://app.notion.com/p/39ad3efc569581179a19d89f5790ae9d

## Goal

Ship a Roblox-shaped particle emitter MVP (CPU pool + billboard draw) so Act 0 campfire/torch flames and siege VFX have a spawn/update/draw path. Owner override 2026-07-27 for Act 0 ahead of M8 hold.

## Context links

- `context/planning/epics.md` (EPIC-0010)
- `context/features/particles.md`
- `context/formats/particle-emitter-assets.md`
- [Roblox ParticleEmitter](https://create.roblox.com/docs/effects/particle-emitters)
- Related: TICKET-0123–0126, Act 0 `coding_particle_system_mvp` / `effects_campfire_flame`

## Acceptance criteria

- [x] `*.particle.json` loads/validates with Roblox-like Rate/Lifetime/Speed/Spread/Shape/Color/Size/Transparency/LightEmission
- [x] CPU emitter pooling + deterministic seed; spawn/update produces draw instances
- [x] D3D12 soft-disc billboards draw after water / before SSAO
- [x] Prefab `particle` attachment; campfire sample wired
- [x] `particles` CTest suite + project validate covers `*.particle.json`
- [x] Context feature/format docs updated

## Out of scope

Effect graphs, GPU compute, collision/LOD budgets, flipbooks, custom texture upload, editor VFX preview, timeline emit→spawn, torch mesh (campfire only for sample).

## Dependencies

Owner override of P3/M5 hold. No shader-graph prerequisite.

## Verification

- Rebuilt `engine` + `engine_suite_tests` (MSVC Debug)
- `engine_suite_tests --suite particles --json` → 16/16 passed
- MCP/editor process reset after rebuild; build lease released

## What changed

- Summary: Added Roblox-shaped CPU particle emitters with soft additive billboards. Campfire prefab now emits a low-poly flame via `campfire_flame.particle.json`.
- Files / surfaces: `particle_emitter_asset` + `ParticleSystem`; render draw after water; prefab `particle` field; sample VFX + campfire wire-up; `particles` suite.
- Schema / API / format deltas: `*.particle.json` schema v1; prefab optional `particle.asset` / `offset` / `enabled`; validate loads particle assets.
- Seed / sample data: `samples/open-world-rpg/assets/vfx/campfire_flame.particle.json`; campfire prefab attachment. Torch still open.
- Tests / verification evidence: particles suite 16/16; engine rebuild succeeded (existing getenv/shadow warnings only).
- Decisions & tradeoffs: Procedural soft disc (no PNG yet); SoftLight blend SRC_ALPHA/ONE; CPU billboards not GPU compute; SSAO still darkens particles slightly.
- Leftover risk / follow-ons: custom textures/flipbooks (0123+), torch emitter, timeline emit→spawn, editor preview (0125/0137), siege particle content.

## Agent notes

Owner override P1 for Act 0. Lease token released after kill→rebuild→MCP restart.
