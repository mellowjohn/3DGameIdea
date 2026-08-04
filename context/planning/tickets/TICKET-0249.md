# TICKET-0249: Subject picker + skinned preview + bottom timeline

- Epic: EPIC-0019
- Status: needs-approval
- Agent: cursor-agent
- Priority: P1
- Notion: https://app.notion.com/p/3b1d3efc5695811094a9eda70f6ebdc9

## Goal

Let authors pick a character/NPC for the Animation sandbox, play and scrub skinned controller/clip playback in the viewport, and drive that from a bottom timeline strip (states, time, keyframe markers).

## Context links

- [`context/features/animation-studio.md`](../../features/animation-studio.md)
- [`context/features/animator.md`](../../features/animator.md)
- [`context/formats/animator-controller-assets.md`](../../formats/animator-controller-assets.md)
- [`context/formats/animation-clip-assets.md`](../../formats/animation-clip-assets.md)
- Prior: TICKET-0248 (viewport + stage)

## Acceptance criteria

- [x] **Subject dropdown:** Combo of project skinned character/NPC prefabs (or equivalent catalog); include `(none)`; stale selection remains visible until changed (lookup-fields rule).
- [x] **Spawn into sandbox:** Selecting a subject instantiates it in the Animation stage only (not the world scene).
- [x] **Skinned playback:** Play / Pause / Step advance an `AnimatorRuntime` (or clip sample) and update GPU-skinned pose in the Animation viewport.
- [x] **Controller/clip pickers:** Dropdowns for controller and/or clip; invalid paths show fail-closed error text.
- [x] **Bottom timeline:** Scrubber shows current time / duration; lists visible keyframe or state markers for the active clip/controller motion; scrubbing seeks preview time.
- [x] **Parameter/state drive (minimal):** Ability to set at least one float/bool/trigger or request a named state for preview (reuse animator APIs).
- [x] **Docs + build:** Update animation-studio feature note; rebuild `engine`; validate sample project.

## Out of scope

- Gear swap (0250), hand attach (0251), timeline event authoring (0252), dual keyframe write-back (0253)
- Editing glTF topology

## Dependencies

- Blocked by: TICKET-0248
- Blocks: useful 0250–0253 preview

## Verification

```powershell
engine validate --project samples/open-world-rpg --json
engine test --project samples/open-world-rpg --suite animator --json
```

Desktop: Animation tab → Subject `player.prefab.json` → Play → skinned Idle/Attack; scrub seeks; Scene world untouched.

## What changed

- Summary: Animation Studio bottom strip can pick an animator prefab, attach a dedicated `anim_studio_runtime`, Play/Pause/Step/Stop/scrub with `AnimatorRuntime::seek`, and draw the GPU-skinned subject on the sandbox base plate.
- Files / surfaces: `render_app.cpp` (studio state, UI, skin upload, sandbox inject); `animator_runtime.h/.cpp` (`seek`); `editor_icons.h` (`ICON_FA_FORWARD`); docs.
- Schema / API / format deltas: `AnimatorRuntime::seek(entity, time, layer?)` with `ANIM-SEEK-*` errors.
- Seed / sample data: none (uses existing player/npc prefabs).
- Tests / verification evidence: `engine` rebuild OK; animator suite run after rebuild; validate on 0248 path.
- Decisions & tradeoffs: Separate studio runtime from play-test `animator_runtime`; synthetic entity id `animation-studio-subject`.
- Leftover risk / follow-ons: Timeline event markers on scrubber still visual-light (state/clip list); desktop QA for Attack pose; gear/attach next.

## Agent notes

Promoted after 0248 sandbox landed. Marker authoring remains 0252.
