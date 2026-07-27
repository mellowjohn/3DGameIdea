# TICKET-0216: Player body replication R1 (host sim + guest input)

- Epic: EPIC-0017
- Status: proposed
- Agent: unassigned
- Priority: P3
- Notion: _(create when mirroring EPIC-0017)_

## Goal

Ship **R1 replication**: guest sends movement/combat input; host simulates **both** player bodies; guest renders interpolated host state ([`co-op-sessions.md`](../../features/co-op-sessions.md) R1, [DEC-0042](../../decisions/index.md#dec-0042-online-co-op-session-model-mode-locked-saves)).

## Context links

- [DEC-0042](../../decisions/index.md#dec-0042-online-co-op-session-model-mode-locked-saves)
- [`context/features/co-op-sessions.md`](../../features/co-op-sessions.md) — R1 row
- TICKET-0212 (`GameSession`, dual entity spawn), TICKET-0215 (open net session)
- Character locomotion: [`character-controller.md`](../../features/character-controller.md) / TICKET-0198 Rigidbody path

## Acceptance criteria

- [ ] Host assigns entity ids to slot 0 and slot 1; guest receives mapping on session start snapshot.
- [ ] **Guest → host:** unreliable input frames `{ slot, moveX, moveY, yaw, buttons, sequence }` at tick rate; host applies to correct locomotion controller.
- [ ] **Host → guest:** snapshots `{ slot, position, yaw, animatorStateHash, health }` at send rate; guest interpolates for render (no local physics sim on guest for player bodies).
- [ ] Host runs full Jolt/locomotion for both slots; guest slot 0 input from host machine, slot 1 from network.
- [ ] Combat/interaction requests from guest validated on host (range, state) — reject with `NET-INPUT-DENIED` when invalid.
- [ ] Loopback test: two peers in sample scene — both avatars move; guest input affects guest avatar on host; guest render tracks host snapshots within tolerance.
- [ ] Enter `paused_waiting_guest` stops applying guest input and freezes guest body state broadcast appropriately.
- [ ] Headless **`net_replication`** or extended suite with deterministic input script.

## Out of scope

- NPC/companion replication (R4).
- Session delta quest/standing patches (TICKET-0217 / R2).
- Client-side prediction / lag compensation beyond interpolation.
- Full reconnect snapshot (TICKET-0218 / R6).

## Dependencies

- **Blocked by:** TICKET-0212, TICKET-0215.
- Soft: TICKET-0198 player on Rigidbody (use current locomotion path if 0198 not done — document).

## Verification

- Rebuild `engine`.
- `engine test --suite net_replication` — loopback pass.
- Desktop QA: two instances localhost — host + guest move both characters; guest disconnect triggers pause (with 0214 overlay if merged).

## What changed

_(stub — fill before needs-approval)_

## Agent notes

Keep snapshot schema versioned (`replicationVersion: 1`) for forward compatibility.
