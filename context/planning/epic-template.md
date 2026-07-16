# Epic documentation template

Use this structure on every Notion epic page body. Keep the authoritative ticket table and Status/Priority in [`epics.md`](epics.md); put the epic brief here so agents and humans share the same goal and boundaries.

## Required sections (Notion page body)

```markdown
## Goal

One or two sentences: what “epic done” means.

## Context links

- `context/planning/epics.md` (this epic)
- `context/roadmap.md` (milestone)
- Related decisions / story / architecture paths

## Acceptance for the epic

- [ ] Criterion that proves the epic’s goal is met (not every child ticket detail)

## Out of scope

Explicit non-goals for this epic.

## Child tickets

- TICKET-XXXX — title (status)
- …

## Dependencies

Blocked by / blocks / soft prerequisites (other epics or milestones).
```

## Required fields in `epics.md`

Every `EPIC-####` section must include:

| Field | Required |
| --- | --- |
| Status | proposed / ready / active / needs-approval / done / deferred |
| Goal | Clear one-liner |
| Roadmap home | Milestone or planning home (e.g. M5, M10 + story) |
| Priority guidance | Hold notes or ladder pointer when relevant |
| Ticket table | ID, Title, Status, Priority, Notes for every child |

## Creating or updating an epic

1. Add/update the section in `epics.md` first (full fields + ticket rows).
2. Mirror Status / Epic ID / Roadmap / Repo notes on the Notion Epics DB page.
3. Fill the Notion page body from this template (Goal, Context links, Acceptance, Out of scope, Child tickets, Dependencies).
4. Ensure each child ticket has a Notion page; add `tickets/TICKET-XXXX.md` for every `ready` / `active` / `needs-approval` ticket (prefer stubs for all children).
5. Keep Status and Priority synced between `epics.md` and Notion. Agents set `needs-approval` after verification; only the human owner sets `done`.

See also: [`ticket-template.md`](ticket-template.md), [`notion-sync.md`](notion-sync.md), [`.cursor/rules/epic-ticket-population.mdc`](../../.cursor/rules/epic-ticket-population.mdc).
