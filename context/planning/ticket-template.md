# Ticket documentation template

Use this structure on every Notion ticket page body and in repo stubs under `tickets/TICKET-XXXX.md`. Keep scope authoritative in `epics.md`; put the detailed brief here so agents can execute without inventing requirements.

## Required sections

```markdown
# TICKET-XXXX: <title>

- Epic: EPIC-XXXX
- Status: proposed | ready | active | needs-approval | done | deferred
- Agent: unassigned | cursor-agent | human
- Priority: P0 | P1 | P2 | P3  (see epics.md Priority ladder)
- Notion: <url>

## Goal

One or two sentences: what owner-approved “done” means for this ticket. Agents finish at `needs-approval`; only the human moves to `done`.

## Context links

- `context/...` paths and decision IDs the agent must read
- Related tickets (blockers / follow-ons)

## Acceptance criteria

- [ ] Testable criterion 1
- [ ] Testable criterion 2

## Out of scope

Explicit non-goals so agents do not expand the ticket.

## Dependencies

Blocked by / blocks / soft prerequisites.

## Verification

How to prove implementation is complete before `needs-approval` (suites, rebuild `engine`, MCP validate, doc-only review, etc.). Fill concrete results when moving to needs-approval (suite names + pass counts, targets rebuilt, blockers).

## What changed

**Required before Status → `needs-approval`.** Owner-facing transparency of the actual delivery — not a restatement of acceptance checkboxes.

- Summary: 2–4 sentences of behavior/product impact in plain language.
- Files / surfaces touched: paths or subsystems (created vs modified). Prefer groups over a 50-line dump.
- Schema / API / format deltas: enums, fields, command names, MCP tools, error codes — only what an owner or next agent must know.
- Seed / sample data: what was authored and what was deliberately left open.
- Tests / verification evidence: suite names and outcomes; rebuild status; anything not run and why.
- Decisions & tradeoffs: choices made during implementation (and pointers to DEC-* if recorded).
- Leftover risk / follow-ons: known gaps, locked binaries, deferred tickets.

Doc-only or research tickets still fill this section (docs edited, conclusions, open questions).

## Agent notes

Working log while Status is `active`: assumptions, interviews opened, mid-flight blockers. Prefer moving durable delivery facts into **What changed** when requesting approval. Clear or archive when the owner moves the ticket to `done`.
```

## Property vs body

| Place | Holds |
| --- | --- |
| `epics.md` row | ID, title, status, one-line notes |
| Notion properties | Status, Agent, Priority, Acceptance (short), Repo notes, Epic relation |
| Notion page body + repo stub | Full Goal / Context / Acceptance / Out of scope / Dependencies / Verification / **What changed** / Agent notes |

## Creating a new ticket

1. Add the row to `epics.md` first.
2. Mirror the card in Notion with the same `TICKET-####` ID.
3. Fill the page body from this template (and add `tickets/TICKET-XXXX.md` when the ticket is `ready` or `active`).
4. Before `needs-approval`, complete **What changed** on both repo stub and Notion body so review does not depend on chat history.
