# TICKET-0104: Root motion extraction and character sync

- Epic: EPIC-0008
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ad3efc569581d28b8ad6f69c40b626

## Goal

Extract root-joint translation deltas from clips and sync them to `CharacterController` under animation-driven locomotion ([DEC-0030](../../decisions/index.md#dec-0030-animation-driven-root-motion)).

## Context links

- Decision: [DEC-0030](../../decisions/index.md#dec-0030-animation-driven-root-motion)
- Format: [`../../formats/animator-controller-assets.md`](../../formats/animator-controller-assets.md)
- Feature: [`../../features/animator.md`](../../features/animator.md), [`../../features/character-controller.md`](../../features/character-controller.md)
- Prior: TICKET-0103 (animator runtime)

## Acceptance criteria

- [x] Documented `applyRootMotion` / `rootJoint` / `rootMotionY` on controller assets + DEC-0030.
- [x] `extract_clip_root_motion_delta` handles linear deltas and loop wrap; missing root fails soft (zero delta).
- [x] `AnimatorRuntime::tick` accumulates weighted root deltas when enabled.
- [x] `CharacterController::move_root_motion` + `sync_character_root_motion` drive the capsule (no wish-velocity walk when applied).
- [x] Named tests; rebuild `engine`; context indexes updated.

## Out of scope

- GPU skinning / in-place visual root zeroing.
- Animation events (0105), IK/retarget (0106).
- Full play-session WASD→param wiring for every sample character (API + sync helper shipped).

## Dependencies

- TICKET-0103 done / approved.

## Verification

`animator` suite green (includes root-motion cases); `character` suite green; `engine` rebuilt after kill/reset. Status → `needs-approval` — never `done`.

## What changed

### Summary

Shipped animation-driven root motion (DEC-0030): clip root extraction, controller flags, animator tick deltas, and capsule sync via `move_root_motion` / `sync_character_root_motion`.

### Files / surfaces

- `extract_clip_root_motion_delta` in animation clip API
- Controller JSON: `applyRootMotion`, `rootJoint`, `rootMotionY`
- `AnimatorRuntime` root delta accumulation
- `CharacterController::move_root_motion`
- `include/engine/animation/root_motion.h` sync helper
- Docs + DEC-0030

### Verification evidence

- `animator` suite: 93/93 (includes root-motion extraction + capsule sync)
- `character` suite: 738/738
- `engine.exe` rebuilt Debug MSVC after process kill

### Leftover risk

- Play/debug-world still uses wish-velocity `move` until a character with `applyRootMotion` is attached in-session.
- Visual mesh root cancel not yet applied (no GPU skinning playback).

## Agent notes

- 2026-07-16: Owner chose animation-driven sync (option 1) → DEC-0030. Implemented; Status → `needs-approval`.
