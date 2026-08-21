# TICKET-0261: MCP inspect — list_keys / sample_pose / diff_pose

- Epic: EPIC-0019
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: (mirror when board is updated)

## Goal

Agents can dump open-clip keyframes and sample local/world joint poses (including sagittal channel vs skin names) without guessing from screenshots alone.

## Context links

- `context/features/animation-studio.md`
- `context/art/animation-craft.md`
- `skills/author-character-animation/SKILL.md`
- DEC-0052 dual-edit

## Acceptance criteria

- [x] `engine_animation_call` `list_keys` / `get_keys` returns channel keys (times, values, eulerDeg for rotation) with `channelName` + `skinName`.
- [x] `sample_pose` returns local TRS (+ world when skin resolved) at `time` (filtered joints optional).
- [x] `diff_pose` compares two times (or vs reference clip/time) with local/world deltas.
- [x] Suite coverage for key dump / pose sample math where GPU-free.
- [x] Docs + MCP schema updated.

## Out of scope

- Contact sheet / multi-seek (TICKET-0262)
- Relative edit kinds (TICKET-0263)
- CUBICSPLINE / Bezier runtime

## Dependencies

Soft: TICKET-0253 dual-edit, TICKET-0259 armature/list_joints.

## Verification

- `engine_suite_tests --suite animator` 412/412.
- Live: `edit_clip` Attack → `list_keys` RightHand → `sample_pose` @ 0.3 → `loop_report`.

## What changed

- Summary: New MCP inspect kinds dump keys and sample/diff poses with sagittal channel/skin names so agents can polish numerically.
- Files: `include/engine/animation/anim_studio_agent_ops.h`, `src/animation/anim_studio_agent_ops.cpp`, `src/rendering/render_app.cpp`, `src/automation/mcp_server.cpp`, docs/skills, CMakeLists + vcxproj.
- MCP: `list_keys`, `get_keys`, `sample_pose`, `diff_pose`.
- Tests: animator suite asserts for list_keys/sample/diff/ease/shift/loop/contact composite.
- Leftover: world XYZ on sample_pose requires resolved subject skin (`test_skinned_mesh_asset`); channel-local always works.

## Agent notes

Shipped with 0262–0264 in one rebuild pass.
