# TICKET-0030: Open-world navigation design notes

- Epic: EPIC-0004
- Status: ready
- Agent: unassigned
- Priority: P1
- Notion: https://app.notion.com/p/39ad3efc569581d0a38ec251bb05de19

## Goal

Write design notes for player traversal in the seamless 4×4 km world: fast travel, roads, barriers, and soft gates — acceptance-oriented, not a new navmesh implementation.

## Context links

- DEC-0001 (4×4 km open world)
- `context/features/navigation-grid.md` (M4 grid: 128 m cells, walkability)
- `context/roadmap.md` (M4 done; M5 Recast deferred as TICKET-0109)
- `context/art/visual-direction.md` (landmarks / readability)
- Feeds: World Forge map assets, mini-map (TICKET-0061), TICKET-0032

## Acceptance criteria

- [ ] Design doc under `context/` covers: intended travel loop, fast-travel policy, road/path role, hard barriers vs soft gates, relationship to existing navigation grid.
- [ ] Explicitly states what remains out of scope for v1 (e.g. Recast/detour = TICKET-0109 deferred).
- [ ] Acceptance criteria bullets suitable for later engineering tickets.
- [ ] Open owner decisions routed to interview skill / open-questions — not silently assumed.
- [ ] `epics.md` Notes link the doc.

## Out of scope

- Implementing Recast/detour, new streaming code, or mini-map UI.
- Authoring final region layouts (World Forge later).

## Dependencies

- Builds on shipped navigation grid.
- Soft pair with TICKET-0031 (map language).

## Verification

- Doc-only review against acceptance checklist.

## Agent notes

_(empty)_
