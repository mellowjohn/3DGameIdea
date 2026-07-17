# Animation Clip Assets

Engine-owned animation clips are imported from glTF 2.0 `.gltf` / `.glb` `animations[]` (TICKET-0102). This slice proves clip load, validation, CPU sampling, and validate-then-replace hot reload. Blend trees, root motion, GPU skinning playback, and production character art are follow-on work.

## Contract

### Imported types

| Type | Meaning |
| --- | --- |
| `AnimationClip` | Named clip with `duration_seconds` and TRS channels |
| `AnimationClipChannel` | Targets a node by index + name; `translation` / `rotation` / `scale` |
| `ImportedAnimationSet` | All clips from one source path |

### Supported

- `animations[]` with non-empty `channels` and `samplers`
- Channel paths: `translation` (VEC3), `rotation` (VEC4 xyzw), `scale` (VEC3)
- Interpolation: `LINEAR` and `STEP` only
- Sampler input: `FLOAT` `SCALAR` times, non-decreasing and finite
- Sampler output: `FLOAT` vectors matching path type; count equals input key count
- Assets with no `animations` array import successfully as an **empty** set (does not fail)
- Hot reload via `AnimationClipLibrary`: `load` → write-time `poll_changed` → `reload`; failed reload keeps the previous set

### Rejected (structured errors)

| Code | Condition |
| --- | --- |
| `ANIM-CLIP-READ` / `ANIM-CLIP-PARSE` | Unreadable or invalid glTF |
| `ANIM-CLIP-EMPTY` | Animation with no channels |
| `ANIM-CLIP-TARGET-MISSING` / `ANIM-CLIP-TARGET-RANGE` | Missing or out-of-range `target.node` |
| `ANIM-CLIP-SAMPLER-RANGE` | Channel sampler index out of range |
| `ANIM-CLIP-PATH-UNSUPPORTED` | Morph `weights` (or other unsupported path) |
| `ANIM-CLIP-INTERP-UNSUPPORTED` | `CUBICSPLINE` (or other non LINEAR/STEP) |
| `ANIM-CLIP-TIME-TYPE` / `ANIM-CLIP-TIME-ORDER` | Bad or unsorted input times |
| `ANIM-CLIP-VALUE-TYPE` / `ANIM-CLIP-VALUE-COUNT` | Bad output type or key count mismatch |
| `ANIM-CLIP-NONFINITE` | NaN/inf in times or values |
| `ANIM-CLIP-ACCESSOR-MISSING` / `ANIM-CLIP-EMPTY-KEYS` | Missing accessors or empty key arrays |
| `ANIM-CLIP-NOT-LOADED` | `get`/`reload` on a path never loaded |

### Explicitly out of this slice

- Compiled intermediate `.anim.json` (may be added later; glTF remains the accepted source)
- `CUBICSPLINE`, morph targets, sparse accessors
- Blend trees / state machines (shipped under TICKET-0103 — see [`animator-controller-assets.md`](animator-controller-assets.md)), root motion (0104), events (0105)
- GPU skinning upload / character playback in the viewport
- Production Squire (or other) rigged art — embedded test fixtures are enough

## API

- `import_gltf_animation_clips(path)` → `ImportedAnimationSet`
- `AnimationClipLibrary::{load,get,reload,poll_changed,reload_changed}`
- `sample_translation_channel(channel, t)` — CPU linear/step sample for tests and tooling

Headers: `include/engine/assets/animation_clip_asset.h`

## Related

- Skeletal/skin import: [`mesh-assets.md`](mesh-assets.md)
- Animator component + Lua drive: [`animator-controller-assets.md`](animator-controller-assets.md) · [`../features/animator.md`](../features/animator.md) · [DEC-0022](../decisions/index.md#dec-0022-c-animator-backend-with-lua-drive-api)
- Roadmap M5: [`../roadmap.md`](../roadmap.md)
