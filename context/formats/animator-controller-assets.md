# Animator Controller Assets

Versioned `*.animator.json` assets describe C++-owned animation graphs: parameters, layers, states, transitions, and 1D blend trees ([DEC-0022](../decisions/index.md#dec-0022-c-animator-backend-with-lua-drive-api), TICKET-0103). Clips remain glTF sources ([`animation-clip-assets.md`](animation-clip-assets.md)).

## Contract

| Field | Meaning |
| --- | --- |
| `schemaVersion` | `1` |
| `kind` | `animatorController` |
| `id` | Stable controller id |
| `applyRootMotion` | When `true`, weighted root deltas drive the capsule ([DEC-0030](../decisions/index.md#dec-0030-animation-driven-root-motion)) |
| `rootJoint` | Optional joint name (fallback `Root`, then `Hip`) |
| `rootMotionY` | When `true`, Y comes from root; default `false` (gravity/jump stay on controller) |
| `parameters[]` | `name`, `type` (`float` / `bool` / `trigger`), optional `default` |
| `layers[]` | Named layers with `defaultState`, `blendMode` (`override` only in v1), `states`, `transitions` |
| `timelineEvents[]` | Optional markers ([DEC-0031](../decisions/index.md#dec-0031-controller-authored-animation-timeline-events)): `state`, `time` (seconds into state), `name`, optional `layer`, optional `payload` object |

### Default state convention

- Each layer’s `defaultState` should be an **idle** (rest pose) state unless the controller is specialty-only (e.g. a one-shot cinematic).
- Prefer the state id `idle` (snake_case), matching other authored ids.
- The entity/prefab `animator` component may optionally override with `defaultState`; in the editor this is a **dropdown** of states from the selected controller (plus `(controller default)`), not free text. Idle is sorted to the top of that list.

### States / motion

- `motion.type = "clip"`: `clipSource` (project-relative glTF/GLB), `clip` (animation name), optional `loop` / `speed`
- `motion.type = "blendTree1D"`: float `parameter` + sorted `children[]` with `threshold` + clip fields

### Transitions

- `from`: state name or `"*"` (any)
- `to`: state name
- `duration`: crossfade seconds (`0` = instant)
- `hasExitTime` / `exitTime`: optional normalized source time gate
- `conditions[]`: `{ parameter, op, value? }` — ops `greater` / `greaterOrEqual` / `less` / `lessOrEqual` / `equal` / `notEqual` / `trigger`

### Timeline events

```json
"timelineEvents": [
  { "state": "attack", "time": 0.35, "name": "hitFrame", "layer": "base", "payload": { "volume": "sword" } }
]
```

- Fired once per state cycle when playback crosses `time` (loop-aware; mask resets on state change / loop wrap).
- Empty `layer` matches any layer that owns `state`.
- Invalid `state` / missing `layer` / negative `time` / empty `name` fail closed at validate (`ANIM-CTRL-EVENT-*`).

### Rejected (structured)

| Code | Condition |
| --- | --- |
| `ANIM-CTRL-*` | Schema / id / layers / states / params / transitions / timeline events invalid |
| Missing clips at runtime | `ANIM-CLIP-*` / `ANIM-CLIP-MISSING` — fail closed; prior state kept when transition target cannot resolve |

## Animator component

Prefab/entity authored component `type: "animator"`:

```json
{
  "id": "animator-0",
  "type": "animator",
  "data": {
    "controller": "assets/animators/player.animator.json",
    "defaultState": "idle"
  }
}
```

`defaultState` is optional and overrides the first layer default when attaching. Same inherit/override model as collider / `scriptBinding` ([DEC-0016](../decisions/index.md#dec-0016-entity-attached-components-and-dual-mcp-apply-paths) / [DEC-0017](../decisions/index.md#dec-0017-prefab-and-scene-component-authoring-with-unity-like-inheritance)).

## Runtime / Lua

- C++: `AnimatorRuntime` — attach, set params, evaluate transitions / blend trees, `tick`, `take_fired_events`, `status` (active clip weights), root-motion delta when `applyRootMotion`
- Character sync: `sync_character_root_motion` / `CharacterController::move_root_motion` ([DEC-0030](../decisions/index.md#dec-0030-animation-driven-root-motion))
- Lua drive (sandbox): `engine.animator_set_float/bool/trigger`, `engine.animator_crossfade`, `engine.animator_get_state`
- Lua react: after `tick`, drain `take_fired_events()` → `LuaRuntime::dispatch_animation_event` → global `on_animation_event` (optional; missing handler is silent)
- Lua does **not** author the graph

Headers: `include/engine/assets/animator_controller_asset.h`, `include/engine/animation/animator_runtime.h`, `include/engine/animation/root_motion.h`

## Out of scope (follow-ons)

- Auto-enable combat volumes from events (scripts/MCP may); IK/retarget (0106)
- Visual in-place root zeroing for GPU skinning / viewport preview polish
- Additive layers, 2D blend trees
- Lua-authored transition graphs (rejected by DEC-0022)

## Related

- Feature note: [`../features/animator.md`](../features/animator.md)
- Clips: [`animation-clip-assets.md`](animation-clip-assets.md)
- Prefabs: [`prefab-assets.md`](prefab-assets.md)
