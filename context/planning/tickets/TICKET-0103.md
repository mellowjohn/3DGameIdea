# TICKET-0103: Blend trees + layered animation state machines

- Epic: EPIC-0008
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ad3efc5695819fa7c2cd2b3f8d6a26

## Goal

Ship the C++ animator backend and authored `animator` component: controller assets with states, transitions, and parameters (blend trees / layered state machines as needed), plus a small Lua API so movement/combat/interaction scripts can drive animation without owning the graph ([DEC-0022](../../decisions/index.md#dec-0022-c-animator-backend-with-lua-drive-api)).

## Context links

- Design: [`../../features/animator.md`](../../features/animator.md)
- Format: [`../../formats/animator-controller-assets.md`](../../formats/animator-controller-assets.md)
- Decision: [DEC-0022](../../decisions/index.md#dec-0022-c-animator-backend-with-lua-drive-api)
- Clips: [`../../formats/animation-clip-assets.md`](../../formats/animation-clip-assets.md) (TICKET-0102)
- Components: DEC-0016 / DEC-0017
- Follow-ons: TICKET-0104 (root motion), 0105 (events → Lua), 0110 (M5 exit)

## Acceptance criteria

- [x] Documented animator controller asset + `animator` component contract.
- [x] Runtime evaluates states/transitions/parameters in C++; missing clips/transitions fail closed with entity-aware diagnostics.
- [x] Lua can set parameters and request/crossfade states from sandbox handlers (movement/combat/interaction).
- [x] Prefab/entity authoring follows existing component inherit/override paths (or a clearly staged subset).
- [x] Named tests cover transitions, bad references, and Lua drive smoke; rebuild `engine`; context indexes updated.

## Out of scope

- Root motion (0104), animation events → Lua (0105) beyond a stub hook if required for smoke.
- IK/retarget (0106), audio (0107).
- Lua-authored transition graphs (rejected by DEC-0022).
- Production character art / final Squire rig.
- GPU skinning / viewport playback polish.

## Dependencies

- TICKET-0102 (clip import + hot reload) done / approved.
- Blocks useful character feedback for combat/movement vertical slice work.

## Verification

Rebuild `engine_core` + `engine_suite_tests`; `animator` suite 23/23; `world` 54/54. `engine.exe` link blocked (file locked — close running editor to refresh the executable). Status → `needs-approval` — never `done`.

## What changed

### Summary

Shipped C++ animator controller assets, runtime evaluation (states / transitions / 1D blend trees / override layers), authored `animator` component on prefabs/entities, and Lua drive API per DEC-0022.

### Files / surfaces

- `include/engine/assets/animator_controller_asset.h`, `src/assets/animator_controller_asset.cpp`
- `include/engine/animation/animator_runtime.h`, `src/animation/animator_runtime.cpp`
- Authored components + prefab load/save + inspector Add Animator
- Lua: `engine.animator_set_float/bool/trigger`, `animator_crossfade`, `animator_get_state`
- Docs: `context/formats/animator-controller-assets.md`, `context/features/animator.md`, prefab/lua/index/coverage updates
- Tests: new `animator` CTest suite in `tests/suite_tests.cpp`

### Schema / API deltas

- New `*.animator.json` schema v1 (`kind: animatorController`)
- Prefab/entity component `type: "animator"` with `controller` + optional `defaultState`
- Lua host requires `LuaRuntime::set_animator_runtime`

### Samples

- Fixture-only (temp glTF + controller in `animator` suite). No production sample character yet.

### Verification evidence

- `animator` suite: 23/23
- `world` suite: 54/54 (authored-component regression)
- `engine_core.lib` rebuilt Debug MSVC
- `engine.exe` link: **LNK1168** — executable locked by running process

### Decisions

- Used existing DEC-0022; v1 layers are override-only; blend trees are 1D float thresholds.

### Leftover risk

- No live play-session attach of `AnimatorRuntime` to placed entities yet (API + tests ready; editor/runtime frame hook is a follow-on).
- GPU skinning / viewport pose playback still absent.
- Close the running `engine.exe` and rebuild to refresh the editor binary.

## Agent notes

- 2026-07-15: Scope expanded from stub using DEC-0022.
- 2026-07-16: Owner approved 0102; implemented; Status → `needs-approval`.
