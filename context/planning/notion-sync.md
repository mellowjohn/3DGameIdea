# Notion Sync Checklist

External hub: [Wrathful Conquest](https://app.notion.com/p/Wrathful-Conquest-30bba218df874253b6493ddfca75cffa)  
Engine board: [Engine Planning Board](https://app.notion.com/p/39ad3efc569581309306e0d8e84cb026)  
Epics DB: [Epics](https://app.notion.com/p/c8333c00395a445aa06a22807929fe45)  
Tickets DB: [Tickets](https://app.notion.com/p/8689089ead5242f1936074a4ed1d759b)  
Authoritative backlog: [`epics.md`](epics.md)  
Ticket briefs: [`tickets/`](tickets/) · Templates: [`epic-template.md`](epic-template.md) · [`ticket-template.md`](ticket-template.md)  
Population rule: [`.cursor/rules/epic-ticket-population.mdc`](../../.cursor/rules/epic-ticket-population.mdc)  
Agent skill: [`skills/engine-ticket-workflow/SKILL.md`](../../skills/engine-ticket-workflow/SKILL.md)  
Decision: [DEC-0015](../decisions/index.md#dec-0015-hybrid-project-tracking)

## Kanban board

Day-to-day drag board (Status columns) on the Engine Planning page:

**[Work Board](https://app.notion.com/p/39ad3efc569581b0a9c9cd4fa0d38868?v=39ad3efc569581bc94d5000c60541f4a)**

Also on the Tickets database:

| View | Purpose | URL |
| --- | --- | --- |
| Kanban | All tickets by Status | [open](https://app.notion.com/p/8689089ead5242f1936074a4ed1d759b?v=39ad3efc569581e5afe3000cf52235b5) |
| Ready & Active | Only startable / in-progress | [open](https://app.notion.com/p/8689089ead5242f1936074a4ed1d759b?v=39ad3efc569581f9b80d000c54ead3a2) |
| Approval | Waiting for owner approval (`needs-approval`) | [open](https://app.notion.com/p/8689089ead5242f1936074a4ed1d759b?v=39ad3efc56958148b28e000cafe4dd99) |
| By Epic | Columns = epics (swimlane-style) | [open](https://app.notion.com/p/8689089ead5242f1936074a4ed1d759b?v=39ad3efc569581458c53000c55d252a2) |
| By Priority | Board grouped by Priority | [open](https://app.notion.com/p/8689089ead5242f1936074a4ed1d759b?v=39ad3efc5695813ea970000c269ebb30) |
| Agent Queue | Ready/Active where Agent = cursor-agent | [open](https://app.notion.com/p/8689089ead5242f1936074a4ed1d759b?v=39ad3efc569581968049000c7c9400f9) |

### Status columns (drag targets)

Flow: `proposed` → `ready` → `active` → `needs-approval` → `done` (`deferred` is a side path).

| Status | Meaning |
| --- | --- |
| `proposed` | Backlog — not ready to start |
| `ready` | Can start now |
| `active` | In progress |
| `needs-approval` | Agent finished implementation + verification; waiting for human owner approval |
| `done` | Owner approved / ready to ship (or already shipped) |
| `deferred` | Parked on purpose |

**Approval policy:** agents set `needs-approval` when work is verified; only the human owner moves cards to `done` (drag on Work Board or Approval view). Mirror the same Status in `epics.md`.

**Ready column policy:** only tickets that can start without waiting on M5 animation exit or World Forge schema. As of 2026-07-10 sync: story/World Forge scope, navigation design, PBR, skeletal import. Hold M6/M7/UI in `proposed` unless overridden.

### Priority (P0–P3)

| Priority | Meaning |
| --- | --- |
| P0 | Engineering critical path (M5 animation clips — TICKET-0102) |
| P1 | Parallel now: story, nav/map design, World Forge scope |
| P2 | Ready/next but not ahead of P0 (e.g. PBR, M5 follow-ons) |
| P3 | Held/later (M6/M7/UI, M8–M11) unless owner overrides |

Authoritative ladder and per-ticket values: [`epics.md`](epics.md#priority-ladder). Mirror Priority in Notion whenever it changes in the repo.

Notion board views support one grouping axis. Use **Kanban** / **Work Board** for Status drag; use **By Epic** or **By Priority** for those groupings.

## Assigning work to agents

1. Set **Status** to `ready` (or `active`), **Agent** to `cursor-agent`, and **Priority** per the ladder (also update `epics.md`).
2. Ensure the Notion page body (and `tickets/TICKET-XXXX.md` when Ready/Active) follows [`ticket-template.md`](ticket-template.md).
3. Tell the agent the `TICKET-####` ID or Notion URL. Agents follow [`skills/engine-ticket-workflow/SKILL.md`](../../skills/engine-ticket-workflow/SKILL.md).

Agents without an ID pick the highest-priority Ready ticket (P0 → P1 → P2), preferring `Agent = cursor-agent`, and skip P3 holds.

## Ticket documentation

Every Ready/Active/`needs-approval` ticket should have:

- Full page body: Goal, Context links, Acceptance criteria, Out of scope, Dependencies, Verification, Agent notes ([`ticket-template.md`](ticket-template.md))
- Matching repo stub under [`tickets/`](tickets/)
- Short **Acceptance** property optional; detailed criteria stay in body + stub + linked context

Every epic Notion page should follow [`epic-template.md`](epic-template.md). Agents must keep both sides populated per [`.cursor/rules/epic-ticket-population.mdc`](../../.cursor/rules/epic-ticket-population.mdc).

## Rules

1. Edit `context/planning/epics.md` (and linked story/roadmap docs) first — including Priority.
2. Mirror the same `EPIC-####` / `TICKET-####` IDs, Status, and Priority in Notion.
3. Notion holds discussion, Agent assignment, and day-to-day Status drag; acceptance criteria and scope stay in repo context (+ ticket stubs).
4. New Notion-only ideas become tickets in `epics.md` before they count as planned work.
5. Drag Status on the Work Board freely for day-to-day flow; sync material status/priority changes back to `epics.md`.

## Live board inventory (synced 2026-07-10)

| Notion object | URL |
| --- | --- |
| Engine Planning Board | https://app.notion.com/p/39ad3efc569581309306e0d8e84cb026 |
| Work Board (kanban) | https://app.notion.com/p/39ad3efc569581b0a9c9cd4fa0d38868?v=39ad3efc569581bc94d5000c60541f4a |
| Epics database | https://app.notion.com/p/c8333c00395a445aa06a22807929fe45 |
| Tickets database | https://app.notion.com/p/8689089ead5242f1936074a4ed1d759b |

Seeded: EPIC-0001…0013 and tickets through TICKET-0151, with Ticket→Epic relations. Assignment properties: Agent, Priority, Acceptance (added 2026-07-10). TICKET-0147–0151 added 2026-07-13/15 (component authoring through exposing existing colliders).

## Database properties

### Epics

| Property | Type | Values / notes |
| --- | --- | --- |
| Name | Title | `EPIC-0002: World Forge` |
| Epic ID | Text | `EPIC-0002` |
| Status | Select | proposed, ready, active, needs-approval, done, deferred |
| Roadmap | Text | e.g. M10, M6/M7 |
| Repo notes | Text | short pointer to context docs |

### Tickets

| Property | Type | Values / notes |
| --- | --- | --- |
| Name | Title | `TICKET-0010: Define World Forge scope…` |
| Ticket ID | Text | `TICKET-0010` |
| Epic | Relation | → Epics |
| Status | Select | proposed, ready, active, needs-approval, done, deferred |
| Agent | Select | unassigned, cursor-agent, human |
| Priority | Select | P0, P1, P2, P3 |
| Acceptance | Text | short acceptance summary |
| Repo notes | Text | short pointer to context docs / `tickets/TICKET-XXXX.md` |

## Agent access

Notion MCP (`plugin-notion-workspace-notion`) can create/update pages and views after workspace auth. Prefer updating `epics.md` first, then mirroring Status/Priority/Agent in Notion. Agents set `needs-approval` after verification; humans set `done`.
