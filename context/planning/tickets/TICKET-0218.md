# TICKET-0218: Co-op reconnect snapshot R6

- Epic: EPIC-0017
- Status: proposed
- Agent: unassigned
- Priority: P3
- Notion: _(create when mirroring EPIC-0017)_

## Goal

Ship **R6 reconnect**: untimed `paused_waiting_guest` resume via full session snapshot so guest can rejoin an in-progress co-op save without host solo fallback ([DEC-0042](../../decisions/index.md#dec-0042-online-co-op-session-model-mode-locked-saves)).

## Context links

- [`context/features/co-op-sessions.md`](../../features/co-op-sessions.md) — R6 row
- TICKET-0114 (save hydrate), TICKET-0215, TICKET-0216

## Acceptance criteria

- [ ] On guest disconnect: host → `paused_waiting_guest`; world sim frozen (0212).
- [ ] Guest reconnect with valid token/save slot → host sends `ReconnectSnapshot` (sharedCampaign + both profiles + entity states + quest/standing runtime).
- [ ] Guest idempotent resume → `playing` without duplicate quest advances.
- [ ] Host **End session** path saves at checkpoint and returns both to menu.
- [ ] Headless: disconnect/reconnect loopback test.

## Out of scope

- Mid-combat reconnect policy beyond freeze (document behavior).
- Cloud relay for NAT.

## Dependencies

- **Blocked by:** TICKET-0215, TICKET-0216, TICKET-0114 (hydrate).

## Verification

- Rebuild `engine`; reconnect suite pass; desktop two-instance reconnect optional.

## What changed

_(stub)_

## Agent notes

_(stub)_
