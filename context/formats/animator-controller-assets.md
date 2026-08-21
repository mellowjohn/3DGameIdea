# Animator Controller Assets

Versioned `*.animator.json` assets describe C++-owned animation graphs: parameters, layers, states, transitions, and 1D/2D blend trees ([DEC-0022](../decisions/index.md#dec-0022-c-animator-backend-with-lua-drive-api), [DEC-0061](../decisions/index.md#dec-0061-masked-override-animator-layers-upper-body-overlay), TICKET-0103 / TICKET-0282 / TICKET-0283). Clips remain glTF sources ([`animation-clip-assets.md`](animation-clip-assets.md)).

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
| `layers[]` | Named layers with `defaultState`, `blendMode` (`override` only), optional `weight` (default `1`), optional `mask`, `states`, `transitions` |
| `timelineEvents[]` | Optional markers ([DEC-0031](../decisions/index.md#dec-0031-controller-authored-animation-timeline-events)): `state`, `time` (seconds into state), `name`, optional `layer`, optional `payload` object |

### Default state convention

- Each layer’s `defaultState` should be an **idle** (rest pose) state unless the layer is an overlay. Overlay passthrough uses state id `empty` with `motion.type = "none"`.
- Prefer the state id `idle` (snake_case) on the **base** layer, matching other authored ids.
- The entity/prefab `animator` component may optionally override with `defaultState`; that override applies only to the **first** layer. In the editor this is a **dropdown** of states from the selected controller (plus `(controller default)`), not free text. Idle is sorted to the top of that list.

### Layer mask (TICKET-0283 / DEC-0061)

- Optional `mask.joints[]` of skin joint names. Empty / omitted = the layer writes the whole skeleton (base-layer behavior).
- `mask.includeChildren` (default `true`) also overrides descendants of listed joints using the imported skin parent links. Typical upper-body overlay lists `Spine` (plus shoulder/arm roots if those are siblings of Spine).
- Later override layers lerp masked joints toward that layer’s pose by clip weight (crossfade) × `layer.weight`. Unmasked joints keep the previous layer. Sample player: Block + Attack/Attack2/Attack3 + BowDraw/BowAim/BowRelease on `upperBody`.
- Masked layers do **not** contribute root motion.
- `motion.type = "none"`: no clips; used for overlay `empty` so the base layer shows through.

### States / motion

- `motion.type = "none"`: passthrough (no clip). Overlay idle/empty.
- `motion.type = "clip"`: `clipSource` (project-relative glTF/GLB), `clip` (animation name), optional `loop` / `speed`
- `motion.type = "blendTree1D"`: float `parameter` + sorted `children[]` with `threshold` + clip fields
- `motion.type = "blendTree2D"`: float `parameterX` / `parameterY` + `children[]` with `position: [x, y]` + clip fields (TICKET-0282). Runtime Delaunay-triangulates positions at parse; samples barycentric weights inside a triangle (up to 3 clips) or closest-edge 1D lerp outside the hull. Invalid trees fail closed (`ANIM-CTRL-BLEND-*`).

Example locomotion freeform tree (facing-relative `moveX` / `moveZ`, same normalized space as `speed`):

```json
{
  "type": "blendTree2D",
  "parameterX": "moveX",
  "parameterY": "moveZ",
  "children": [
    { "position": [0.0, 0.35], "clipSource": "assets/models/player.gltf", "clip": "Walk", "loop": true },
    { "position": [-0.35, 0.0], "clipSource": "assets/models/player.gltf", "clip": "WalkStrafeLeft", "loop": true },
    { "position": [0.35, 0.0], "clipSource": "assets/models/player.gltf", "clip": "WalkStrafeRight", "loop": true },
    { "position": [0.0, -0.35], "clipSource": "assets/models/player.gltf", "clip": "Walk", "loop": true },
    { "position": [0.0, 1.0], "clipSource": "assets/models/player.gltf", "clip": "Run", "loop": true },
    { "position": [-1.0, 0.0], "clipSource": "assets/models/player.gltf", "clip": "RunStrafeLeft", "loop": true },
    { "position": [1.0, 0.0], "clipSource": "assets/models/player.gltf", "clip": "RunStrafeRight", "loop": true }
  ]
}
```

Idle remains a separate state; do not put Idle in the 2D tree. Walk-radius points (~0.35) cover A/D at walk; run-radius (±1,0) cover full-speed lateral with lean.

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
- **Authoring (TICKET-0252):** Animation Studio Diagnostics edits this array and Save writes the controller. Preview: `name == "footstep"` → `assets/vfx/footstep_dust.particle.json`; optional `payload.particle` (or `payload.effect`) path to any `*.particle.json` for other event names.

### Rejected (structured)

| Code | Condition |
| --- | --- |
| `ANIM-CTRL-*` | Schema / id / layers / states / params / transitions / timeline events invalid |
| `ANIM-CTRL-MASK-JOINT` | `mask.joints` entry missing or empty |
| `ANIM-CTRL-LAYER-WEIGHT` | `weight` is NaN or `< 0` |
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
- Additive layers (delta-from-reference). Masked **override** overlay is in ([DEC-0061](../decisions/index.md#dec-0061-masked-override-animator-layers-upper-body-overlay)).
- Lua-authored transition graphs (rejected by DEC-0022)

## Related

- Feature note: [`../features/animator.md`](../features/animator.md)
- Clips: [`animation-clip-assets.md`](animation-clip-assets.md)
- Prefabs: [`prefab-assets.md`](prefab-assets.md)
