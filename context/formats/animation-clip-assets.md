# Animation Clip Assets

Engine-owned animation clips are imported from glTF 2.0 `.gltf` / `.glb` `animations[]` (TICKET-0102). Dual-edit overrides (`*.anim.json`) land under [DEC-0052](../decisions/index.md#dec-0052-dual-edit-animation-clips) / TICKET-0253.

## Contract

### Imported types

| Type | Meaning |
| --- | --- |
| `AnimationClip` | Named clip with `duration_seconds` and TRS channels |
| `AnimationClipChannel` | Targets a node by index + name; `translation` / `rotation` / `scale` |
| `ImportedAnimationSet` | All clips from one source path |
| `AnimationClipOverrideAsset` | Engine sidecar override for one `(clipSource, clipName)` |

### Supported

- `animations[]` with non-empty `channels` and `samplers`
- Channel paths: `translation` (VEC3), `rotation` (VEC4 xyzw), `scale` (VEC3)
- Interpolation: `LINEAR` and `STEP` only
- Sampler input: `FLOAT` `SCALAR` times, non-decreasing and finite
- Sampler output: `FLOAT` vectors matching path type; count equals input key count
- Assets with no `animations` array import successfully as an **empty** set (does not fail)
- Hot reload via `AnimationClipLibrary`: `load` → write-time `poll_changed` → `reload`; failed reload keeps the previous set
- After each successful glTF import, matching `*.anim.json` sidecars are merged (**override wins**)

### Engine override (`*.anim.json`)

Sidecar next to the glTF: `hero_clips.gltf` + clip `Idle` → `hero_clips.Idle.anim.json` (`animation_clip_override_path`).

```json
{
  "schemaVersion": 1,
  "kind": "animationClipOverride",
  "clipSource": "assets/models/hero_clips.gltf",
  "clipName": "Idle",
  "durationSeconds": 1.0,
  "channels": [
    {
      "targetNodeName": "Hip",
      "targetNodeIndex": 0,
      "path": "translation",
      "interpolation": "LINEAR",
      "times": [0.0, 1.0],
      "values": [0, 0, 0, 0, 1, 0]
    }
  ]
}
```

| Action | Behavior |
| --- | --- |
| Save override | Atomic write sidecar; library reload re-merges |
| Sync to source | Writes LINEAR/STEP TRS accessors into `.gltf` JSON; fail-closed for `.glb` / CUBICSPLINE / missing nodes |
| Replace from source | Deletes sidecar and reloads glTF (DEC-0052 conflict choice — keep override is the default on re-import) |

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
| `ANIM-CLIP-NOT-LOADED` | `get`/`reload`/`replace_clip` on a path never loaded |
| `ANIM-OV-*` | Override parse/validate/write failures |
| `ANIM-OV-SYNC-GLB` | Sync attempted on `.glb` |
| `ANIM-OV-SYNC-CLIP` / `ANIM-OV-SYNC-CHANNEL` / `ANIM-OV-SYNC-INTERP` | Sync target missing or unsupported |

### Explicitly out of this slice

- `CUBICSPLINE`, morph targets, sparse accessors
- Automatic sync on every keystroke (explicit Sync only)
- Sync into `.glb` binary containers

## API

- `import_gltf_animation_clips(path)` → `ImportedAnimationSet`
- `AnimationClipLibrary::{load,get,reload,replace_clip,poll_changed,reload_changed}`
- `AnimationClipOverrideAsset::{load,parse,save_atomic,from_clip,to_clip}`
- `animation_clip_override_path` / `apply_animation_clip_override` / `sync_animation_clip_override_to_gltf`
- `sample_translation_channel` / `sample_rotation_channel` / `sample_scale_channel`

Headers: `include/engine/assets/animation_clip_asset.h`

## Related

- Skeletal/skin import: [`mesh-assets.md`](mesh-assets.md)
- Animator + Animation Studio: [`animator-controller-assets.md`](animator-controller-assets.md) · [`../features/animator.md`](../features/animator.md) · [`../features/animation-studio.md`](../features/animation-studio.md)
- Dual-edit decision: [DEC-0052](../decisions/index.md#dec-0052-dual-edit-animation-clips)
