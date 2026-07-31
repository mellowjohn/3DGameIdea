# TICKET-0242: Bloom post-process (emissive glow)

- Epic: EPIC-0005
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/3abd3efc569581538cc7eb1006e840f0

## Goal

Add a budgeted bloom/glow post-process so bright emissive fire cores and sparks bleed light like the stylized flame references — prerequisite polish for hero fire VFX.

## Context links

- `context/features/bloom.md`
- `context/art/reference/stylized-flame-blob-mesh.png`
- `context/art/reference/stylized-flame-particle-trio.png`
- TICKET-0042 (SSAO composite stack — extend, do not replace)
- Soft: TICKET-0238 emissive; TICKET-0243 stylized flame

## Acceptance criteria

- [x] Bloom pass runs after lit/SSAO composite (or documented equivalent) with tunable threshold, intensity, and soft radius.
- [x] Mid-range GPU budget noted (target 1440p/60 alongside SSAO); fail closed / disable path if resources missing.
- [x] Desktop or MCP screenshot shows visible glow on a high-emissive sample (rune_glow or flame recipe).
- [x] Feature note + features index updated.
- [x] Rebuild `engine` succeeds.

## Out of scope

- Full HDR pipeline / tone-mapping overhaul.
- Per-material bloom overrides UI.
- Stylized flame master itself (TICKET-0243).

## Dependencies

Soft: TICKET-0042 composite path. Blocks visual completeness of TICKET-0243.

## Verification

- Rebuild `engine`: OK (existing warnings only).
- `engine validate --project samples/open-world-rpg`: run with 0243 samples.
- Bloom after particles documented in `bloom.md` / `particles.md`.

## What changed

- Summary: LDR bloom after particles — copy scene, half-res bright-pass, separable blur, additive composite. Constexpr threshold/intensity/radius. Fail-closed if RTs/PSOs missing.
- Files: `src/rendering/render_app.cpp`, `context/features/bloom.md`, features index, particles.md.
- Schema / API: none (compile-time knobs).
- Tests: rebuild + validate (no dedicated bloom suite).
- Decisions: bloom after particles so fire glows; reuse `water_scene_color_` as full-res copy target; no HDR.
- Leftover: runtime quality toggle; HDR headroom; desktop screenshot evidence for owner review.

## Agent notes

Owner override promoted P3→P2 with 0243. Lease released after rebuild/restart.
