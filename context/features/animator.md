# Animator

Status: active (TICKET-0103) — design locked by [DEC-0022](../decisions/index.md#dec-0022-c-animator-backend-with-lua-drive-api).

## Ownership split

| Layer | Owner | Responsibility |
| --- | --- | --- |
| Clips | C++ / assets | glTF TRS clip data ([`animation-clip-assets.md`](../formats/animation-clip-assets.md)) |
| Animator backend | C++ | Playback timing, 1D blend trees, controller graph (states / transitions / parameters), missing-clip fail-closed diagnostics |
| Animator component | Prefab / entity (authored) | References a controller asset; same inherit/override model as collider / `scriptBinding` ([DEC-0016](../decisions/index.md#dec-0016-entity-attached-components-and-dual-mcp-apply-paths) / [DEC-0017](../decisions/index.md#dec-0017-prefab-and-scene-component-authoring-with-unity-like-inheritance)) |
| Gameplay hooks | Lua | Movement, combat, interaction scripts **drive** parameters / request states and **react** to animation events — they do not author the transition graph |

## Shipped pieces (TICKET-0103)

1. **Controller asset** (`*.animator.json`): named states → clips or 1D blend trees, transitions with conditions, override layers. Contract: [`../formats/animator-controller-assets.md`](../formats/animator-controller-assets.md).
2. **`animator` component** on prefabs/entities: `controller` path, optional `defaultState`.
3. **`AnimatorRuntime`**: attach/detach, param setters, automatic transitions, crossfade, `tick`, status with active clip weights.
4. **Lua drive API**: `animator_set_float` / `animator_set_bool` / `animator_set_trigger` / `animator_crossfade` / `animator_get_state`.

## M5 exit verification (TICKET-0110)

Headless evidence (no desktop viewport required):

```text
engine test --project samples/open-world-rpg --suite m5-exit
engine test --project samples/open-world-rpg --suite animator
engine animation-preview --project samples/open-world-rpg --json
```

- **`m5-exit`** runs `animator`, `character`, `interaction`, `combat`, and `scripting` CTest suites.
- **`animation-preview`** ticks the sample `assets/animators/example.animator.json` controller and prints deterministic JSON (`initialState`, `finalState`, key frames, timeline event count, root-motion sum).

### Editor preview

- **Scene Inspector:** select an entity with an **Animator** component to inspect/edit its controller path and default state.
- **Prefab Editor:** same fields on prefab `animator` components.
- **Play test:** locomotion/combat/interaction still visible via Diagnostics movement console, interaction feed, and combat hit feed; GPU-skinned poses in Game view.
- **Animation Studio (EPIC-0019):** dedicated **Animation** viewport tab + sandbox stage for isolated preview, gear, attach, timeline events, and dual-edit keyframes — see [`animation-studio.md`](animation-studio.md). **TICKET-0135** (Diagnostics-adjacent text panel) is **deferred / superseded**.

Sample project ships `assets/animators/example.animator.json` + `assets/models/player_clips.gltf` (referenced from `vertical-slice.world.json`).

## Root motion (TICKET-0104 / DEC-0030, retarget TICKET-0199)

Controllers may set `applyRootMotion` (+ optional `rootJoint` / `rootMotionY`). `AnimatorRuntime::tick` accumulates weighted root translation deltas.

- **Rigidbody path (preferred):** `sync_rigidbody_root_motion(world, body, …)` / `apply_rigidbody_root_motion` set horizontal linear velocity on a dynamic `CollisionBody` (TICKET-0199).
- **CharacterVirtual fallback:** `sync_character_root_motion` still drives `CharacterController::move_root_motion` for debug-world / non-Rigidbody callers.

## Timeline events (TICKET-0105 / DEC-0031)

Controllers may author `timelineEvents[]` (state + time + name + optional layer/payload). `AnimatorRuntime::tick` fires loop-aware crossings into `take_fired_events()`; hosts dispatch to Lua `on_animation_event`. Engine does not auto-enable combat volumes in v1.

## Out of scope

- Play-test **GPU LBS skinning** (TICKET-0227 / DEC-0047): per-entity `AnimatorRuntime` attach for every scene entity with an `animator` component (propagate prefab components first so `npc_test` inherits its controller); CPU skin matrices upload into a **16-slot bone CB ring**; prop/shadow draws bind the entity’s slot via `skin_entity_id`. Locomotion/combat params still drive only the spawn entity. Player visual uses yaw π (mesh −Z vs loco +Z); because the right-handed Blockbench glTF is imported verbatim into the left-handed runtime, `sample_clip_pose_for_joint` samples the sagittal counterpart joint and reflects the local pose through the YZ plane, so Attack reads right-handed in-engine exactly as it does in Blockbench (see `context/testing/findings.md`, 2026-07-31). Catalog-wide / Animation tools viewport polish remain follow-on.
- Sample `player.animator.json` drives Idle/Walk/Run + Jump/Fall/Land, Attack/Block (one-handed melee hotbar gate), directional Dodge (Shift + stamina spend + scripted dash + dodge_dust burst), HitReact/Death/Revive, Interact/InteractPickup from play-test input + locomotion grounded/speed.
- Runtime IK solve (metadata only today — [`../formats/rig-assets.md`](../formats/rig-assets.md), TICKET-0106).
- Auto combat-volume enable from events; play-session animator wiring polish.
- Lua-authored state machines (rejected unless a new decision supersedes DEC-0022).
- Production character art — fixture glTF is enough for engineering.
- Editor Animation Studio viewport — **EPIC-0019** / TICKET-0248+ (TICKET-0135 superseded).

## Related

- [`../formats/animation-clip-assets.md`](../formats/animation-clip-assets.md)
- [`../formats/animator-controller-assets.md`](../formats/animator-controller-assets.md)
- [`../formats/rig-assets.md`](../formats/rig-assets.md)
- Architecture animation goals: [`../architecture/overview.md`](../architecture/overview.md)
- Lua scripting: [`lua-scripting.md`](lua-scripting.md)
- Content vs engine: [`../architecture/content-vs-engine-workflows.md`](../architecture/content-vs-engine-workflows.md)
