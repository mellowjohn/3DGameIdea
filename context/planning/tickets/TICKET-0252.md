# TICKET-0252: Timeline events + particle triggers in Animation view

- Epic: EPIC-0019
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: (see Notion Tickets DB by Ticket ID)

## Goal

Authors can view and edit controller `timelineEvents` on the Animation Diagnostics timeline and preview particle/VFX triggers at those markers while scrubbing or playing the sandbox subject.

## Context links

- [`context/features/animation-studio.md`](../../features/animation-studio.md)
- [`context/features/animator.md`](../../features/animator.md)
- [`context/formats/animator-controller-assets.md`](../../formats/animator-controller-assets.md)
- Prior: TICKET-0249

## Acceptance criteria

- [x] **Event markers on timeline:** `timelineEvents[]` for the selected controller appear as marker buttons + list; selecting shows name/time/layer/particle fields.
- [x] **Add/edit/remove events:** Writes back to `*.animator.json` via validate + `save_atomic`; fail-closed with stable codes.
- [x] **Particle preview:** Play/Step fire via `take_fired_events`; scrub-forward and marker click / Preview button; `footstep` → dust; optional `payload.particle`.
- [x] **Lookup UX:** State / layer / particle dropdowns (not free-text ids for lookups).
- [x] **Docs + rebuild `engine`.**

## Out of scope

- Full particle graph editor
- Auto combat-volume enable from events
- Clip TRS keyframe dual-edit (0253)

## Dependencies

- Blocked by: TICKET-0249
- Soft: particle recipes / TICKET-0105 events

## Verification

Desktop: player controller → attack hitFrame / land footstep markers → Play crosses marker → particles in sandbox; Save updates animator JSON.

## What changed

### Summary

Animation Diagnostics hosts timelineEvents CRUD + marker strip. Studio Play dispatches particle bursts; scrub-forward and Preview also fire. Save reloads controller into runtime.

### Files / surfaces

- `src/rendering/render_app.cpp` — working copy, UI, dispatch helpers
- `context/features/animation-studio.md`, `formats/animator-controller-assets.md`, this stub, `epics.md`

### Schema / API

- Optional `payload.particle` / `payload.effect` convention for studio preview (opaque JSON still valid for Lua).

### Verification evidence

- `engine` rebuild under lease
- `animator` suite previously green; capture smoke if run

### Leftover risk

- Seek alone does not fire (by design); scrub must move forward through a marker time.

## Agent notes

—
