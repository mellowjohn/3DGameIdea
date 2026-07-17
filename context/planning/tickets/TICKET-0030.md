# TICKET-0030: Open-world navigation design notes

- Epic: EPIC-0004
- Status: needs-approval
- Agent: cursor-agent
- Priority: P1
- Notion: https://app.notion.com/p/39ad3efc569581d0a38ec251bb05de19

## Goal

Write design notes for player traversal in the seamless 4×4 km world: fast travel, roads, barriers, and soft gates — acceptance-oriented, not a new navmesh implementation.

## Context links

- DEC-0001, DEC-0021
- [`../features/open-world-navigation.md`](../../features/open-world-navigation.md) (**deliverable**)
- `context/features/navigation-grid.md`
- `context/art/visual-direction.md`
- Pair: TICKET-0031; feeds TICKET-0032, TICKET-0061

## Acceptance criteria

- [x] Design doc covers travel loop, FT policy, roads, hard vs soft gates, relationship to navigation grid.
- [x] Out of scope explicit (Recast = TICKET-0109, mini-map UI, final layouts).
- [x] Engineering acceptance bullets for later tickets.
- [x] Open preferences routed to `context/interviews/open-questions.md` (not silent assumptions).
- [x] `epics.md` Notes link the doc.

## Out of scope

- Implementing Recast/detour, new streaming code, or mini-map UI.
- Authoring final region layouts (World Forge later).

## Dependencies

- Builds on shipped navigation grid.
- Soft pair with TICKET-0031.

## Verification

- Doc-only review against acceptance checklist.

## What changed

- Summary: Documented the intended overland travel loop (foot → roads → soft gates → discovery FT → rare instances), how the M4 navigation grid relates to player locomotion vs AI assist, hard barriers vs soft/story gates, and a recommended fast-travel policy for owner confirmation.
- Files / surfaces: created `context/features/open-world-navigation.md`; updated `features/index.md`, `epics.md`, open-questions, this stub.
- Schema / API: none (design only).
- Seed / sample data: none.
- Tests / verification: doc review vs ticket acceptance.
- Decisions & tradeoffs: Defaults align with DEC-0021; FT discovery-gated after first hub, no v1 cost; preferences left open rather than inventing a DEC.
- Leftover risk: Owner may revise FT cost/unlock; engineering tickets not yet filed from the acceptance seed list.

## Agent notes

Delivered with TICKET-0031 in the same session.
