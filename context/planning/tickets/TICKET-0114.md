# TICKET-0114: Versioned RPG save format + migrations

- Epic: EPIC-0017 (also EPIC-0006 M6)
- Status: needs-approval
- Agent: cursor-agent
- Priority: P3
- Notion: https://app.notion.com/p/39ad3efc5695811bb80affb2725fac3f

## Goal

Ship versioned player save/load for solo campaigns and **mode-locked co-op saves** ([DEC-0042](../../decisions/index.md#dec-0042-online-co-op-session-model-mode-locked-saves)). Persist `QuestRuntime`, `StandingRuntime`, and shared campaign state; hydrate per-player profiles. Format contract: [`context/formats/rpg-save.md`](../../formats/rpg-save.md).

## Context links

- [`context/formats/rpg-save.md`](../../formats/rpg-save.md) — schema sketch
- [`context/features/co-op-sessions.md`](../../features/co-op-sessions.md) — lobby + load gates
- [`context/decisions/index.md`](../../decisions/index.md) — DEC-0042, DEC-0028, DEC-0029
- TICKET-0180 (QuestRuntime), TICKET-0181 (StandingRuntime), TICKET-0213 (PartyRuntime)

## Acceptance criteria

- [x] `*.save.json` with `schemaVersion: 1` validates per format doc (`RPG-SAVE-*` errors).
- [x] Root fields: `sessionMode` (`solo`|`coop`), `hostProfile`, `guestProfile` (co-op only), `sharedCampaign`, `worldAnchor`.
- [x] Solo save: load → hydrate runtimes → spawn host at `worldAnchor`; no guest required.
- [x] Co-op save: load without guest → lobby-only (fail closed to `playing`); with guest → both profiles hydrate.
- [x] `sharedCampaign.quests.instances[]` round-trips `QuestRuntime` state (status + completed objective ids).
- [x] `sharedCampaign.standing.scores[]` round-trips `StandingRuntime` scores + `lockInFactionId`.
- [x] `sharedCampaign.party.activeCompanionIds` enforces mode cap (solo ≤3, co-op ≤2 companions).
- [x] Atomic write (temp + rename); corrupt save returns structured error without crashing.
- [x] Migration hook exists for future `schemaVersion` bumps with at least one unit test stub.
- [x] Context indexes current (`rpg-save.md`, feature inventory).

## Out of scope

- Online transport / lobby (separate tickets; see co-op-sessions implementation order).
- Morality runtime implementation (store schema fields; hydrate when runtime lands).
- Per-player gold purses (shared `economy.gold` default).
- Cloud save sync.

## Dependencies

- QuestRuntime (TICKET-0180) and StandingRuntime (TICKET-0181) session APIs stable enough to serialize.
- GameSession (TICKET-0212) for `apply_rpg_save_to_session` load gates.
- PartyRuntime (TICKET-0213) for companion hydrate (landed same pass).

## Verification

- Rebuild `engine` — succeeded.
- `engine_suite_tests --suite rpg_save` — **39/39**.
- `engine test --project samples/open-world-rpg --suite rpg_save` — passed.

## What changed

### Summary

Players can persist and reload mode-locked solo/co-op campaign saves. Quest and standing state round-trip; co-op saves refuse `playing` without a guest via `apply_rpg_save_to_session`.

### Files / surfaces

- Created: `include/engine/save/rpg_save.h`, `src/save/rpg_save.cpp`
- Extended: `QuestRuntime::list_instances` / `restore_instance`
- Docs: `context/formats/rpg-save.md` marked active

### Schema / API / format deltas

- `RpgSaveDocument` load/save/validate/capture_from/hydrate_into
- `migrate_rpg_save_json` (v1 identity; unsupported → `RPG-SAVE-UNSUPPORTED-VERSION`)
- `apply_rpg_save_to_session(doc, session, guest_connected)`
- Errors: `RPG-SAVE-*` per format doc

### Tests / verification evidence

- `rpg_save` **39/39**; engine CLI suite pass

### Leftover risk / follow-ons

- Editor menu Save/Load UI not wired; morality/discovery/camp blobs are forward-compatible placeholders.
- TICKET-0214 lobby canvases consume load gates next.

## Agent notes

Suite covers corrupt JSON, mode validation, party over-cap, hydrate round-trip, lobby-only coop apply.
