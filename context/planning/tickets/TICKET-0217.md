# TICKET-0217: Session delta replication R2 + unanimous fork confirm

- Epic: EPIC-0017
- Status: proposed
- Agent: unassigned
- Priority: P3
- Notion: _(create when mirroring EPIC-0017)_

## Goal

Ship **R2 replication**: reliable ordered patches for shared campaign state (quests, standing, morality, flags, discovery) plus **dual confirm** UI for major story forks ([DEC-0042](../../decisions/index.md#dec-0042-online-co-op-session-model-mode-locked-saves)).

## Context links

- [`context/features/co-op-sessions.md`](../../features/co-op-sessions.md) — R2, unanimous fork table
- TICKET-0216 (R1 session open), TICKET-0180, TICKET-0181

## Acceptance criteria

- [ ] Reliable channel message types: `SessionDelta` with typed payloads (`quest`, `standing`, `morality`, `flag`, `discovery`).
- [ ] Host applies deltas authoritatively; guest applies same patches for UI/runtime mirrors only.
- [ ] `ForkProposal` / `ForkConfirm` messages; host applies fork only when slot 0 and slot 1 confirms match current proposal id.
- [ ] Modal canvas `coop_fork_confirm.uicanvas.json` wired to proposal flow.
- [ ] Headless tests: dual confirm required; stale confirm rejected.

## Out of scope

- Full morality runtime if not yet implemented (schema-only patches OK).
- NPC replication (R4).

## Dependencies

- **Blocked by:** TICKET-0216.

## Verification

- Rebuild `engine`; `net_session_delta` suite pass.

## What changed

_(stub)_

## Agent notes

_(stub)_
