---
name: engine-ticket-workflow
description: >-
  Find, pick up, implement, and close assigned engine tickets under DEC-0015 hybrid
  tracking (repo epics.md + Notion Wrathful Conquest), including Priority (P0–P3).
  Use when the user assigns a TICKET-####, asks to work the backlog, Ready/Active
  board, agent queue, priority order, or Notion Work Board; or when starting planned
  epic/ticket work.
---

# Engine Ticket Workflow

Repo-first scope ([DEC-0015](../../context/decisions/index.md#dec-0015-hybrid-project-tracking)): `context/planning/epics.md` is authoritative for IDs, **Status**, **Priority**, and acceptance. Notion is for assignment, discussion, and day-to-day kanban.

## When to use

- User names a `TICKET-####` or says work the Ready/Active / agent queue / highest priority.
- Starting planned work from the Engine Planning Board.
- Closing or syncing ticket status/priority after implementation.

## Priority ladder

| Priority | Meaning | Typical work |
| --- | --- | --- |
| P0 | Critical path — pick first when Ready | M5 skeletal import (TICKET-0101) |
| P1 | Parallel now | Story (EPIC-0003), nav/map design (EPIC-0004), World Forge scope (0010) |
| P2 | Ready/next, not ahead of P0 | PBR (0040/0143), M5 follow-ons, World Forge schemas after 0021 |
| P3 | Held/later — need owner override to start early | M6/M7/UI, M8–M11, deferred |

Full table lives in [`epics.md`](../../context/planning/epics.md#priority-ladder).

## Owner assignment (humans) — 3 steps

1. Open [Work Board](https://app.notion.com/p/39ad3efc569581b0a9c9cd4fa0d38868?v=39ad3efc569581bc94d5000c60541f4a) or [Ready & Active](https://app.notion.com/p/8689089ead5242f1936074a4ed1d759b?v=39ad3efc569581f9b80d000c54ead3a2).
2. Set **Status** → `ready` (or `active`), **Agent** → `cursor-agent` (or `human`), and **Priority** → `P0`–`P3` per the ladder (mirror the same Priority in `epics.md`).
3. Paste the ticket ID / Notion URL into the agent chat (e.g. “Work TICKET-0101”).

Full rules: [`notion-sync.md`](../../context/planning/notion-sync.md). Template: [`ticket-template.md`](../../context/planning/ticket-template.md).

## Owner approval (humans)

When an agent finishes implementation + verification, the ticket lands in **`needs-approval`** (not `done`). Review the work, then drag the card to **`done`** on the [Work Board](https://app.notion.com/p/39ad3efc569581b0a9c9cd4fa0d38868?v=39ad3efc569581bc94d5000c60541f4a) or [Approval](https://app.notion.com/p/8689089ead5242f1936074a4ed1d759b?v=39ad3efc56958148b28e000cafe4dd99) view (and mirror `done` in `epics.md`). Only the human owner moves tickets to `done`.

## Agent pickup checklist

```
Ticket progress:
- [ ] Confirm ID in epics.md (Status ready|active; Priority set; not invented scope)
- [ ] Read Notion page body + Acceptance property
- [ ] Read repo stub context/planning/tickets/TICKET-XXXX.md if present
- [ ] Read linked context/ docs and decisions
- [ ] Interview blocking decisions if needed (skills/interview-engine-decisions)
- [ ] Set Notion + epics.md Status → active; Agent → cursor-agent
- [ ] Implement narrowly; do not invent new product scope
- [ ] Verify (rebuild engine after C++; suites / validate as applicable)
- [ ] Update ticket docs (Acceptance met; fill **What changed** per ticket-template; Agent notes)
- [ ] Set Status → needs-approval in epics.md then Notion (do NOT set done)
```

### Find work when no ID given

1. Read `context/planning/epics.md` Priority ladder + Suggested work order.
2. Prefer Notion/repo tickets with **Agent** = `cursor-agent` and Status `ready`.
3. Among candidates, pick the **lowest Priority number** (P0 before P1 before P2). Tie-break: Suggested work order, then lower Ticket ID.
4. Skip blocked tickets (Notes say blocked / hold) and all **P3** unless the user explicitly overrides.
5. Do not start P2 engineering (e.g. PBR) ahead of an available P0 Ready ticket.
6. Fetch the Notion ticket page via MCP. Do not invent scope from the title alone.

### Read before coding

- `AGENTS.md`
- Ticket row in `epics.md` + Notion page / repo stub sections
- Linked `context/` paths and `context/decisions/index.md`
- `context/architecture/content-vs-engine-workflows.md` for C++ vs MCP routing

### Decision interviews

Use `skills/interview-engine-decisions/SKILL.md` when a missing choice would materially change architecture, scope, compatibility, or acceptance. Stop on blocking unknowns; otherwise state the safe assumption and continue.

### Status / priority sync

| Event | `epics.md` | Notion |
| --- | --- | --- |
| Pickup | Status → `active` | Status → `active`, Agent → `cursor-agent` |
| Priority change | Update Priority column | Mirror Priority property |
| Implementation + verification finished | Status → `needs-approval`; **What changed** filled | Status → `needs-approval`; **What changed** + Agent notes on page |
| Owner approved / shipped | Status → `done` (human only) | Status → `done` (human only) |
| Park | Status → `deferred` | Status → `deferred` + reason |

Edit `epics.md` first for material status/scope/priority changes, then mirror Notion.

**Agents must not set Status to `done`.** After verification, set `needs-approval` and stop. The human owner drags to `done` when ready to treat the work as shipped.

### Implementation guardrails

- No Notion-only cards as planned work until they appear in `epics.md`.
- No product-scope expansion beyond the ticket + linked context.
- C++ / shaders / CMake: rebuild `engine` before claiming implementation complete.
- Content batches: `engine_project_validate` when applicable.
- Tests with behavior changes; context indexes in the same change.
- No git commit unless the user asks.

### Definition of done (agent handoff)

Matches `AGENTS.md` verification bar (builds, relevant tests pass, user-visible behavior documented, affected context indexes current), then Status → **`needs-approval`**. If verification is blocked, state exactly what was not run and leave Status `active` (or note the blocker). **`done` is owner-only** after approval.

## Notion properties (Tickets DB)

| Property | Use |
| --- | --- |
| Status | `proposed` / `ready` / `active` / `needs-approval` / `done` / `deferred` |
| Agent | `unassigned` / `cursor-agent` / `human` |
| Priority | `P0` / `P1` / `P2` / `P3` |
| Acceptance | Short summary (full criteria on page + repo stub) |
| Ticket ID | Stable `TICKET-####` |
| Repo notes | Pointer into `context/` |

## Additional resources

- [`ticket-template.md`](../../context/planning/ticket-template.md)
- [`tickets/`](../../context/planning/tickets/)
- [`epics.md`](../../context/planning/epics.md)
- [`notion-sync.md`](../../context/planning/notion-sync.md)
