# TICKET-0246: Bone weld toolset for held item attach

- Epic: EPIC-0018
- Status: needs-approval
- Agent: cursor-agent
- Priority: P1
- Notion: <pending>

## Goal

A weapon selected on the hotbar stays welded to the authored character joint through an entire animation, and the
Inspector gizmo that authors that weld writes back exactly what was dragged. Owner reported both halves broken
during a live play-test session.

## Context links

- `context/features/gearing-system.md` — hand attach / hotbar equip behavior
- `context/formats/mesh-assets.md`, `context/formats/rig-assets.md` — skin joints and rig roles
- `context/testing/findings.md` — RH→LH handedness note for `Right*` joint names
- `include/engine/animation/bone_attachment.h`, `src/animation/bone_attachment.cpp`
- `src/rendering/render_app.cpp` — play-test skin tick, weld render, Inspector, ImGuizmo call sites
- `src/world/transform_utils.cpp` — joint matrix → TRS
- Related: TICKET-0237 (hand attach was explicitly out of scope there), TICKET-0132 (prefab part gizmos)

## Acceptance criteria

- [x] Held weapon tracks the animated joint on the visible body, including the character's authored prefab scale
- [x] Authored `handAttach.joint` is what the runtime attaches to (no silent fallback to a different bone)
- [x] Pausing the play-test freezes body and weapon in the same pose
- [x] Gizmo rotate on two or more axes writes back the orientation that was dragged
- [x] Gizmo drags solve against a frozen socket instead of a joint that keeps animating
- [x] Weld supports move / rotate / scale with optional snapping
- [x] Joint picker lists the joints of the live skin, not a hard-coded subset
- [x] `gripScale` round-trips through item JSON and defaults to 1
- [x] `animator` suite covers euler round trip, socket chain scale, and inverse solve

## Out of scope

- Skinned weapon meshes, bow draw rigs, two-handed IK
- Welds outside the play-test path (standalone runtime `debug_character` visual still has no held mesh)
- Modular character kit slots (hair/outfit) — separate World Forge todo
- Lua / MCP bindings for weld authoring

## Dependencies

- Soft prerequisite: TICKET-0237 inventory hotbar (already landed)

## Verification

- `engine_suite_tests --suite animator` (weld math, euler round trip, item `gripScale`)
- Rebuild the `engine` target under the shared build lease
- Live play-test: select the Ashfell sword, run/attack, confirm the blade stays in the hand; pause and confirm
  body and blade freeze together; drag the weld gizmo and confirm the mesh lands where the handle was released

## What changed

- Summary: Held items were welding to a broken joint frame — the joint matrix was transposed on the way into a
  transform (dropping its translation entirely), the character's prefab part scale was left out of the chain, and
  the authored joint name was overwritten by a stale default. The gizmo compounded it by decomposing Euler angles
  in ImGuizmo's axis order and rebuilding them in DirectX's. All of that now runs through a single
  `engine::BoneWeld` API with a Motor6D-shaped socket → weld → world chain and an exact inverse solve.
- Files / surfaces touched: created `include/engine/animation/bone_attachment.h` and
  `src/animation/bone_attachment.cpp`; modified `src/world/transform_utils.cpp`, `src/rendering/render_app.cpp`,
  `src/assets/item_catalog_asset.cpp`, `include/engine/assets/item_catalog_asset.h`, `CMakeLists.txt`,
  `tests/suite_tests.cpp`.
- Schema / API deltas: `handAttach.gripScale` (float3, default `[1,1,1]`) added to item catalog JSON. New public
  API: `BoneWeld`, `BoneSocketChain`, `bone_socket_world`, `weld_world_transform`, `weld_from_world_transform`,
  `quaternion_from_euler_deg`, `euler_deg_from_quaternion`.
- Seed / sample data: none authored; `assets/items/act0_landfall_items.json` keeps its existing sword weld and
  gains `gripScale` on the next Inspector save.
- Tests / verification evidence: `animator` suite green (new cases for euler round trip incl. gimbal lock, socket
  chain with a scaled character, weld inverse solve, `transform_from_column_major` translation, `gripScale`
  parse/default). `engine` target rebuilt under the shared lease and the editor relaunched. Live editor capture
  `samples/open-world-rpg/out/weld-verify-02-*.png` shows the sword in the right hand at character scale.
  Defect writeup in `context/testing/findings.md`.
- Decisions & tradeoffs: the weld now inherits the character scale rather than forcing scale 1, which is the
  physically consistent behavior for a held prop; `gripScale` exists so an oversized bake can be compensated per
  item. Default joint for an unauthored item moved from `RightMiddle1` (a finger) to `RightHand`.
- Leftover risk / follow-ons: the standalone runtime `debug_character` path still draws no held mesh.
  `weld_from_world_transform` assumes a uniformly scaled socket. Because welds ride the sagittal clip mirror,
  `RightHand` is the on-screen right hand while a clip plays but flips on a raw bind pose — author welds during
  play-test.

## Agent notes

Grip gizmo hotkeys are now G / R / T / X (T added for scale). The socket freeze on drag makes authoring usable
while a locomotion clip is playing, but pausing (F6) is still the recommended workflow.
