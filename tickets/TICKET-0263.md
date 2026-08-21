# TICKET-0263: Relative clip edits — offset / shift / set_pose

- Epic: EPIC-0019
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: (mirror when board is updated)

## Goal

Agents can adjust poses with deltas, time-shift tracks for overlap, and set a block pose in one round-trip — reusing the same sagittal upsert path as `upsert_key`.

## Context links

- `context/features/animation-studio.md`
- DEC-0052
- TICKET-0261

## Acceptance criteria

- [x] `offset_key` / `offset_keys` add euler/translation/scale deltas at time(s); create from sampled pose if missing.
- [x] `shift_keys` time-shifts joint track(s) by `dt` (clamp to duration).
- [x] `set_pose` batch-sets joints at `time` (absolute or from rest / `fromTime` / `fromClip`).
- [x] `copy_pose_at` samples at `fromTime` and pastes at `toTime` (or clipboardOnly sample).
- [x] Docs + MCP schema updated.

## Out of scope

- IK solvers (TICKET-0106)
- Bezier curves

## Dependencies

TICKET-0261 inspect recommended for verify loop.

## Verification

- Suite: shift_keys / ease helpers in animator suite.
- Live inspect path exercised; relative ops wired via same upsert path.

## What changed

- Summary: Relative MCP edit kinds for deltas, time-shift, and block poses.
- Files: `anim_studio_agent_ops.cpp` (`shift_clip_keys`), `render_app.cpp` handlers.
- Leftover: Prefer live polish pass with screenshots on next content task.

## Agent notes

Shipped with 0261/0262/0264.
