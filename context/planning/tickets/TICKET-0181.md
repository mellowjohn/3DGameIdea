# TICKET-0181: Faction standing / reputation (schema + runtime)

- Epic: EPIC-0006
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39fd3efc569581929432f57e1bd11eaa

## Goal

Ship continuous faction standing ([DEC-0029](../../decisions/index.md#dec-0029-continuous-faction-standing-with-hostility-transfer)): World Forge authoring + session `StandingRuntime` with hostility fallout from rival/opposes edges, Lua/MCP, quest gates/rewards fields. Morality and save deferred.

## Context links

- [DEC-0029](../../decisions/index.md#dec-0029-continuous-faction-standing-with-hostility-transfer)
- [`context/formats/world-forge-factions.md`](../../formats/world-forge-factions.md)
- Soft: TICKET-0114 save, TICKET-0062 HUD, morality track

## Acceptance criteria

- [x] Faction standing config (tracksPlayer, min/max, ranks, lockIn)
- [x] Relationship standingTransfer on rival/opposes faction edges
- [x] Quest standingRequirements / standingRewards
- [x] StandingRuntime adjust with hostility fallout + clamp + lock-in
- [x] Lua `engine.standing_*` + MCP `engine_standing_call`
- [x] World Forge editor fields
- [x] Suites + rebuild engine; no invented story thresholds

## Out of scope

Morality; destruction/reform; inventing Cristallo/Arrotrebae numbers; auto QuestRuntime→rewards; full journal UI.

## Dependencies

Owner override of M6 hold. Builds on TICKET-0011/0012/0050.

## Verification

- Rebuilt `engine` (Debug MSVC) — succeeded after stopping two locked `engine.exe` processes
- `world_forge`: 128/128 passed
- `automation`: 73/73 passed (includes `standing_call` MCP bridge)

## What changed

- Summary: Session faction standing is continuous and authored. Adjusting one faction applies hostility fallout through rival/opposes edges with `standingTransfer`. Ranks and lock-in thresholds gate content; quest assets can declare requirements/rewards. Morality and save remain deferred. Quest complete does not auto-apply standing rewards in v1 (explicit Lua/MCP).
- Files / surfaces: `StandingRuntime` (`include/engine/standing/`, `src/standing/`); World Forge faction/relationship/quest asset parse+validate; Lua `engine.standing_*`; MCP `engine_standing_call` + editor session bridge; World Forge editor standing/transfer/quest lists; format + feature + story docs.
- Schema / API: optional `standing` on factions; `standingTransfer` on edges; `standingRequirements` / `standingRewards` on quests (schemaVersion still 1). Errors `STANDING-RUNTIME-*`, `WORLD-FORGE-FACTION-STANDING-*`, `WORLD-FORGE-REL-STANDING-TRANSFER`.
- Seed / sample data: no invented Cristallo/Arrotrebae numbers. Removed stray sample `test_quest` so seed count matches docs (3).
- Tests: headless hostility/clamp/rank/lock-in + rank-order/transfer validation in `world_forge`; `standing_call` in `automation`.
- Decisions: DEC-0029 option A; rewards applied by caller, not QuestRuntime auto-hook.
- Leftover risk: session-only until TICKET-0114; sample factions still lack standing configs until owner fills thresholds; QuestRuntime→reward auto-wire deferred.

## Agent notes

2026-07-16: Implemented option A plan; status → needs-approval.
