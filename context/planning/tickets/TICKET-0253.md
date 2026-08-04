# TICKET-0253: Dual-edit clip keyframes (override + sync to glTF)

- Epic: EPIC-0019
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: (see Notion Tickets DB by Ticket ID)

## Goal

Authors can edit bone TRS keyframes in the Animation Studio timeline; the engine persists an override it always understands, and an explicit Sync writes those channels back into the source glTF so the art file stays good ([DEC-0052](../../decisions/index.md#dec-0052-dual-edit-animation-clips)).

## Context links

- [DEC-0052](../../decisions/index.md#dec-0052-dual-edit-animation-clips)
- [`context/features/animation-studio.md`](../../features/animation-studio.md)
- [`context/formats/animation-clip-assets.md`](../../formats/animation-clip-assets.md)
- Prior: TICKET-0249 (timeline UI); soft 0251 for attach while posing

## Acceptance criteria

- [x] **Override format:** Versioned `*.anim.json` sidecar (`mesh.ClipName.anim.json`) with validate error codes (`ANIM-OV-*`).
- [x] **Runtime merge:** `AnimationClipLibrary` load/reload merges sidecars (override wins).
- [x] **Timeline key edit:** Joint + channel dropdowns; edit/insert/delete keys; live preview via `replace_clip`.
- [x] **Save override:** Atomic write (copy-overwrite); fail-closed on nonfinite / unsorted times.
- [x] **Sync to source:** Explicit Sync writes LINEAR/STEP TRS into `.gltf`; fail-closed for `.glb` / missing channels.
- [x] **Re-import conflict:** Reload keeps override by default; **Replace from source** deletes sidecar.
- [x] **Suite:** `animator` covers override-wins sampling + sync round-trip + GLB reject.
- [x] **Docs + rebuild `engine`.**

## Out of scope

- CUBICSPLINE / morph weights
- Mesh topology editing
- Automatic sync on every keystroke
- Sync into `.glb`

## Dependencies

- Blocked by: TICKET-0249
- Decision: DEC-0052 (accepted)

## Verification

```powershell
engine test --project samples/open-world-rpg --suite animator
engine validate --project samples/open-world-rpg --json
```

Desktop: Animation tab → edit a Hip translation key → Save override → Play uses new pose → Sync to source → Replace from source clears override.

## What changed

### Summary

Shipped DEC-0052 dual-edit: `AnimationClipOverrideAsset`, library merge, Sync-to-`.gltf`, Animation Diagnostics keyframe UI, and animator suite coverage.

### Files / surfaces

- `include/engine/assets/animation_clip_asset.h`, `src/assets/animation_clip_asset.cpp`
- `src/rendering/render_app.cpp` — Clip keyframes section
- `tests/suite_tests.cpp` — 0253 checks in `animator`
- `context/formats/animation-clip-assets.md`, `animation-studio.md`, this stub, `epics.md`

### Schema / API

- Sidecar: `<stem>.<ClipName>.anim.json` beside the glTF
- `apply_animation_clip_override` / `sync_animation_clip_override_to_gltf` / `AnimationClipLibrary::replace_clip`

### Verification evidence

- `animator` suite: 383/383 passed after Windows atomic-write fix (copy-overwrite)
- `engine` rebuild OK

### Decisions

- Copy-overwrite for atomic writes on Windows (rename-replace throws / ACCESS_DENIED).

### Leftover risk

- Sync only for `.gltf` JSON; `.glb` rejected with `ANIM-OV-SYNC-GLB`.
- Sync updates existing TRS channels only (does not invent new glTF channels).

## Agent notes

Initial suite crash was an uncaught `filesystem_error` from Windows rename-over-existing during Sync.
