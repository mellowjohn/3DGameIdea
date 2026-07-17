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

## Root motion (TICKET-0104 / DEC-0030)

Controllers may set `applyRootMotion` (+ optional `rootJoint` / `rootMotionY`). `AnimatorRuntime::tick` accumulates weighted root translation deltas; `sync_character_root_motion` drives `CharacterController::move_root_motion` so locomotion is animation-owned while input drives params/facing.

## Timeline events (TICKET-0105 / DEC-0031)

Controllers may author `timelineEvents[]` (state + time + name + optional layer/payload). `AnimatorRuntime::tick` fires loop-aware crossings into `take_fired_events()`; hosts dispatch to Lua `on_animation_event`. Engine does not auto-enable combat volumes in v1.

## Out of scope

- GPU skinning / viewport character playback polish (later M5 exit work).
- Auto combat-volume enable from events; play-session animator wiring polish.
- Lua-authored state machines (rejected unless a new decision supersedes DEC-0022).
- Production character art — fixture glTF is enough for engineering.

## Related

- Architecture animation goals: [`../architecture/overview.md`](../architecture/overview.md)
- Lua scripting: [`lua-scripting.md`](lua-scripting.md)
- Content vs engine: [`../architecture/content-vs-engine-workflows.md`](../architecture/content-vs-engine-workflows.md)
