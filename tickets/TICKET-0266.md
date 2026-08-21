# TICKET-0266: sample_series + event-labeled contact sheet

- Epic: EPIC-0019
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: (mirror when board is updated)

## Goal

Agents can read joint (and optional held grip) world/local positions across multiple times in one call, and contact sheets label slots with clip time + matching timeline events (e.g. hitFrame).

## Context links

- `context/features/animation-studio.md`
- `skills/author-character-animation/SKILL.md`
- TICKET-0261 / TICKET-0262

## Acceptance criteria

- [x] `sample_series` (alias `tip_series`): `times[]` + optional `joints[]` → per-time samples; optional held grip from weld joint when world pose present.
- [x] `seek_times` with `labelEvents` (default true): viewport status + `slotLabels` metadata include time + event names.
- [x] Docs + MCP schema + suite coverage where GPU-free.

## Out of scope

- Bitmap font burned into PNG pixels
- Full mesh tip AABB without weld joint

## Dependencies

TICKET-0261 sample_pose, TICKET-0262 seek_times, TICKET-0252 events.

## Verification

- Suite animator 415/415.
- Live Attack `seek_times` → `slotLabels` includes `t=0.52 | hitFrame`.

## What changed

- Summary: Added multi-time pose series helper and event-aware contact-sheet slot labels for combat-window review.
- Files: `include/engine/animation/anim_studio_agent_ops.h`, `src/animation/anim_studio_agent_ops.cpp`, `src/rendering/render_app.cpp`, `src/automation/mcp_server.cpp`, tests, docs.
- MCP: `sample_series` / `tip_series`; `seek_times` `labelEvents` + `slotLabels`.
- Leftover: grip entry needs skinned world sample (`test_skinned_mesh_asset`); channel-local series still returns poses.

## Agent notes

Bundled with TICKET-0265. Prior Attack polish (free-arm offsets, hitFrame@0.52, ease) already on disk via earlier MCP pass.
