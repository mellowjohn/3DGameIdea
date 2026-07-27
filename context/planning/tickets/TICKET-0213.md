# TICKET-0213: PartyRuntime companion caps by session mode

- Epic: EPIC-0017
- Status: needs-approval
- Agent: cursor-agent
- Priority: P3
- Notion: _(create when mirroring EPIC-0017)_

## Goal

Ship **`PartyRuntime`** with mode-dependent companion limits (solo 1+3, co-op 2+2, party cap 4) tied to `GameSession::sessionMode` ([DEC-0042](../../decisions/index.md#dec-0042-online-co-op-session-model-mode-locked-saves)).

## Context links

- [DEC-0042](../../decisions/index.md#dec-0042-online-co-op-session-model-mode-locked-saves)
- [DEC-0032](../../decisions/index.md#dec-0032-open-world-travel-discovery-map-and-dual-soft-gates) — solo party language
- [`context/features/co-op-sessions.md`](../../features/co-op-sessions.md)
- [`context/formats/rpg-save.md`](../../formats/rpg-save.md) — `sharedCampaign.party`
- Blocked by: TICKET-0212 (`GameSession`)

## Acceptance criteria

- [x] `PartyRuntime` with `maxHumans()`, `maxCompanions()`, `maxPartySize()` derived from bound `GameSession` mode (solo: 1/3/4; co-op: 2/2/4).
- [x] `add_companion(id)` / `remove_companion(id)` / `list_active()` with fail-closed `PARTY-OVER-CAP` when at companion limit.
- [x] Persist shape matches `sharedCampaign.party.activeCompanionIds` in rpg-save doc (serialize hooks for TICKET-0114).
- [x] Camp staging API: `party_members_for_camp()` returns up to 4 ids (humans + active companions) for DEC-0033 handoff.
- [x] Headless **`party`** suite: solo cap 3 companions; co-op cap 2; recruitment blocked at limit; invalid duplicate id rejected.

## Out of scope

- Companion recruitment quests / dialogue.
- Companion follow AI (host replication — TICKET-0216).
- Mount party sizing (DEC-0032 deferred).

## Dependencies

- **Blocked by:** TICKET-0212 (`GameSession` mode).
- **Used by:** TICKET-0114 save hydrate.

## Verification

- Rebuild `engine` — succeeded.
- `engine_suite_tests --suite party` — **19/19**.
- `engine test --project samples/open-world-rpg --suite party` — passed.

## What changed

### Summary

Companion recruitment now respects solo vs co-op caps. Camp staging returns both humans (co-op) plus companions up to party size 4. Saves hydrate companion lists through `set_active_companions`.

### Files / surfaces

- Created: `include/engine/party/party_runtime.h`, `src/party/party_runtime.cpp`
- Wired into `RpgSaveDocument::capture_from` / `hydrate_into`

### Schema / API / format deltas

- Errors: `PARTY-OVER-CAP`, `PARTY-DUPLICATE`, `PARTY-INVALID-ID`, `PARTY-NOT-FOUND`
- `set_human_member_ids` for camp staging labels

### Tests / verification evidence

- `party` **19/19**

### Leftover risk / follow-ons

- No AI follow yet; human ids are labels until character-creation ids exist.

## Agent notes

_(none)_
