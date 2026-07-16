# TICKET-0040: Dynamic material / PBR rendering slice

- Epic: EPIC-0005
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ad3efc569581a0858fc43b186f030e

## Goal

Implement the pending PBR lighting path so validated material fields (roughness, metallic, emissive, and related) affect rendering — without silently faking unsupported modes.

## Context links

- `context/formats/materials.md` (schema v1; opaque PBR runtime documented)
- `include/engine/rendering/pbr_lighting.h` (CPU BRDF reference)
- `context/roadmap.md` (M4: opaque PBR done; transparency pending)
- `context/features/index.md` (Material assets)
- Same workstream tracking: TICKET-0143
- Priority note: **P2** — owner pulled ahead of AO (TICKET-0042)

## Acceptance criteria

- [x] Opaque PBR lighting uses roughness/metallic (and documented emissive behavior) for materials that already validate.
- [x] Unsupported opacity modes still fail closed or remain non-rendered per materials.md — no silent fake transparency.
- [x] Tests and/or editor/CLI verification cover at least one material asset path.
- [x] `context/formats/materials.md` and features index updated for runtime support.
- [x] Rebuild `engine` succeeds; relevant suites pass. *(engine_core + suite_tests rebuilt; `engine.exe` relink blocked while editor PID held the file — close editor and rebuild to pick up the binary.)*

## Out of scope

- Full post-process AO stack (TICKET-0042).
- Shader graph authoring decision (TICKET-0041) unless required for this slice.
- Typography / screenshot regression (0144/0145).
- Masked/blended dedicated pipelines (deferred; fail closed).

## Dependencies

- Materials schema already exists.
- Coordinate with TICKET-0143 (tracking twin) — prefer one implementation change set.

## Verification

- `engine_suite_tests --suite assets` (PBR BRDF + fail-closed opacity checks).
- Rebuild `engine_core` / `engine_suite_tests` succeeded.
- Relink `engine.exe` after closing the running editor process.

## Agent notes

- Cook-Torrance GGX in main + foliage pixel shaders; per-draw Object CB carries roughness/metallic/emissive.
- Prefab parts with masked/blended materials are skipped at instance expand.
- Terrain uses opaque PBR when opacityMode is opaque; otherwise dielectric defaults (still draws, no transparency).
