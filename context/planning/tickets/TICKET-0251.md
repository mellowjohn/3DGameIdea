# TICKET-0251: Bone / hand-attach authoring in Animation view

- Epic: EPIC-0019
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: (see Notion Tickets DB by Ticket ID)

## Goal

Authors can place bows/weapons on bones inside the Animation sandbox (weld gizmo + joint picker + Save handAttach) so attach polish happens in the studio instead of only during Game play-test.

## Context links

- [`context/features/animation-studio.md`](../../features/animation-studio.md)
- [`context/features/gearing-system.md`](../../features/gearing-system.md) — handAttach / BoneWeld
- TICKET-0246 weld toolset
- Prior: TICKET-0250

## Acceptance criteria

- [x] **Attach tools in Animation tab:** Joint dropdown, grip offset/euler/scale, move/rotate/scale gizmo operate on sandbox subject + studio held mesh.
- [x] **Save handAttach:** Persists to item catalog JSON with same fields as Inspector path.
- [x] **Animated vs paused:** Socket freeze-on-drag + scrub/pause keep weld consistent with sampled joint.
- [x] **Docs:** Animation Studio is preferred attach authoring surface; Game Inspector remains fallback.
- [x] **Rebuild `engine`.**

## Out of scope

- Full IK solver
- Clip keyframe editing (0253)
- Multi-character simultaneous attach sessions

## Dependencies

- Blocked by: TICKET-0250 (equipped mesh present)
- Soft: TICKET-0246 approved/landed

## Verification

Desktop: Animation tab → equip bow → adjust grip on RightHand during Idle/Attack scrub → Save → reload studio → attach matches.

## What changed

### Summary

`draw_held_weapon_attach_inspector` accepts an optional studio item id; Animation Diagnostics hosts the same weld tools. Gizmo uses Animation camera matrices. Save writes `handAttach` for the studio-selected item.

### Files / surfaces

- `src/rendering/render_app.cpp` — studio attach inspector, Animation viewport gizmo, editor draw camera switch
- `context/features/gearing-system.md`, `animation-studio.md`, this stub, `epics.md`

### Schema / API

- Unchanged item `handAttach` fields.

### Verification evidence

- `engine` rebuild under lease
- Play-test Inspector path still calls `draw_held_weapon_attach_inspector(state)` with default nullptr

### Decisions

- Prefer Animation Studio for weld polish; keep Game path.

### Leftover risk

- Gizmo is one frame behind skin sample (same as play-test editor order).

## Agent notes

Shipped with TICKET-0250 in one pass.
