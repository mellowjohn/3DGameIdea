# Animator (planned)

Status: planned — design locked by [DEC-0022](../decisions/index.md#dec-0022-c-animator-backend-with-lua-drive-api). Implementation home: **TICKET-0103** (after TICKET-0102 clip import).

## Ownership split

| Layer | Owner | Responsibility |
| --- | --- | --- |
| Clips | C++ / assets | glTF TRS clip data ([`animation-clip-assets.md`](../formats/animation-clip-assets.md)) |
| Animator backend | C++ | Playback, blending, controller graph (states / transitions / parameters), missing-clip fallbacks |
| Animator component | Prefab / entity (authored) | References a controller asset; same inherit/override model as collider / `scriptBinding` ([DEC-0016](../decisions/index.md#dec-0016-entity-attached-components-and-dual-mcp-apply-paths) / [DEC-0017](../decisions/index.md#dec-0017-prefab-and-scene-component-authoring-with-unity-like-inheritance)) |
| Gameplay hooks | Lua | Movement, combat, interaction scripts **drive** parameters / request states and **react** to animation events — they do not author the transition graph |

## Intended pieces (TICKET-0103+)

1. **Controller asset** (versioned, text-friendly): named states → clips, transitions with conditions on parameters, optional layers.
2. **`animator` component** on prefabs/entities: controller path, default state / layer setup.
3. **Lua drive API** (sandbox-safe): set bool/float/trigger params, request or crossfade to a state, query current state.
4. **Events** (TICKET-0105): timeline markers invoke Lua handlers (combat hit frames, footstep cues, etc.).

## Out of scope for this design note

- GPU skinning / viewport preview polish (later M5 exit work).
- Root motion extraction (TICKET-0104).
- Lua-authored state machines (rejected unless a new decision supersedes DEC-0022).
- Production character art — placeholder clipped glTF is enough for engineering.

## Related

- Architecture animation goals: [`../architecture/overview.md`](../architecture/overview.md)
- Lua scripting: [`lua-scripting.md`](lua-scripting.md)
- Content vs engine: [`../architecture/content-vs-engine-workflows.md`](../architecture/content-vs-engine-workflows.md)
