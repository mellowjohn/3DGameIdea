# TICKET-0215: Online session handshake R0 (lobby sync)

- Epic: EPIC-0017
- Status: proposed
- Agent: unassigned
- Priority: P3
- Notion: _(create when mirroring EPIC-0017)_

## Goal

Ship **R0 replication**: online lobby create/join/ready/leave/start with host-authoritative lobby state synced to guest, replacing local mock join in TICKET-0214 ([`co-op-sessions.md`](../../features/co-op-sessions.md) R0).

## Context links

- [DEC-0042](../../decisions/index.md#dec-0042-online-co-op-session-model-mode-locked-saves)
- [`context/features/co-op-sessions.md`](../../features/co-op-sessions.md) — R0 handshake table
- TICKET-0212 (`GameSession`), TICKET-0214 (lobby UI bindings)
- Transport library choice **open** — document selected dependency + license in `context/resources/index.md`

## Acceptance criteria

- [ ] **`NetSession`** (or equivalent) abstraction: reliable ordered channel + connection lifecycle; implementation may be loopback TCP for v1 if Steam/EOS deferred — must be swappable.
- [ ] Messages (JSON or binary schema documented): `LobbyCreate`, `LobbyJoin`, `LobbyState`, `SetReady`, `Leave`, `StartSession`, `SessionDenied` with stable error codes (`NET-LOBBY-*`).
- [ ] Host owns lobby truth; guest receives `LobbyState` snapshots (host profile summary, guest connected, ready flags, save hash/id).
- [ ] `StartSession` only accepted when both ready; guest receives slot assignment `playerSlot=1`.
- [ ] Integrates with `GameSession`: successful start → `coop_loading` → `playing` (world load may be stub if 0216 not done).
- [ ] Headless **loopback** test: two in-process peers or two CLI processes on localhost complete create → join → dual ready → start without UI.
- [ ] CLI or automation command: `engine coop-lobby --project … --role host|join --json` for agent testing (DEC-0003).
- [ ] License/provenance for any third-party net dependency recorded.

## Out of scope

- Player body replication (TICKET-0216 / R1).
- NAT traversal / Steam lobby browser (follow-on ticket).
- Encrypted auth / account system.
- Co-op save upload — host loads local save only in v1.

## Dependencies

- **Blocked by:** TICKET-0212, TICKET-0214 (UI bindings exist).
- **Blocks:** TICKET-0216 (R1 needs open session).

## Verification

- Rebuild `engine`.
- `engine test --suite net_lobby` (or `automation` subset) — loopback handshake passes.
- `engine coop-lobby --project samples/open-world-rpg --json` documented in feature doc with sample output.
- Two-machine test optional; loopback required.

## What changed

_(stub — fill before needs-approval)_

## Agent notes

Prefer loopback-first proof; document how to swap transport backend later.
