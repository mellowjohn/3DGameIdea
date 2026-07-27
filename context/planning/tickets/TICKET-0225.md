# TICKET-0225: Session FlagRuntime (quest stage / Act 0 flags)

- Epic: EPIC-0006
- Status: needs-approval
- Agent: cursor-agent
- Priority: P0
- Notion: https://app.notion.com/p/3a7d3efc569581b1b8dfd4d22d6f19cd

## Goal

Ship session `FlagRuntime` so Act 0 can set/query story and fork outcome flags from dialogue, Lua, and MCP — completing World Forge readiness item `coding_quest_runtime_flags` without pulling journal UI or soft-gate systems.

## Context links

- Act 0 readiness: `coding_quest_runtime_flags` in `samples/.../act0_mvp_readiness.worldforge.json`
- [DEC-0028](../../decisions/index.md#dec-0028-explicit-quest-progression-runtime) — QuestRuntime already shipped (TICKET-0180)
- [DEC-0026](../../decisions/index.md#dec-0026-quest-owned-dialogue-hooks-multi-stage) — dialogue `setFlags` / quest fork `outcomeFlags`
- [DEC-0046](../../decisions/index.md#dec-0046-session-story-flag-runtime) — this ticket’s decision
- Soft follow-ons: soft-gate plumbing (`coding_soft_gate_system`); journal (`ui_quest_journal` / TICKET-0062)

## Acceptance criteria

- [x] `FlagRuntime` set/clear/has/list/reset/restore + `FLAG-RUNTIME-EMPTY` on empty ids
- [x] Dialogue choice `setFlags` apply into session `FlagRuntime` (same path as standing apply)
- [x] `QuestRuntime::resolve_fork(questId, forkId, outcomeFlag, FlagRuntime&)` validates authored outcomes, clears sibling fork flags, sets chosen flag
- [x] Lua: `flag_set` / `flag_clear` / `flag_has` / `flag_list` + `quest_resolve_fork`
- [x] MCP: `engine_flag_call` (set|clear|has|list); `engine_quest_call` kind `resolve_fork`
- [x] RPG save `sharedCampaign.outcomeFlags` capture/hydrate via FlagRuntime
- [x] Headless suites: world_forge / automation / game_session / rpg_save coverage; rebuild `engine`
- [x] Docs: DEC-0046, quests format, lua-scripting, mcp-live-editor, features index; mark readiness item done

## Out of scope

- Soft-gate / region pressure (`coding_soft_gate_system`)
- Quest journal / pause UI (`ui_quest_journal`)
- Auto-complete quests from dialogue finish
- Co-op replication of flags (TICKET-0217)

## Dependencies

Depends on TICKET-0180 QuestRuntime. Owner override Act 0 MVP P0 for `coding_quest_runtime_flags`.

## Verification

- Rebuild `engine` + `engine_suite_tests` — succeeded (pre-existing C4996 getenv in `render_app.cpp`)
- `world_forge` **265/265**; `automation` **112/112**; `game_session` **46/46**; `rpg_save` **39/39**; `scripting` **37/37**

## What changed

### Summary

Act 0 now has a session story/outcome flag store. Dialogue `setFlags` and explicit quest fork resolution write into it; Lua/MCP can set/query flags; RPG save round-trips `outcomeFlags`. Soft-gates and journal remain separate.

### Files / surfaces

- Created: `include/engine/flag/flag_runtime.h`, `src/flag/flag_runtime.cpp`
- Modified: quest resolve_fork; dialogue choose apply flags; Lua/MCP; GameSession bind; RPG save capture/hydrate; editor session wiring; suite tests
- Docs: DEC-0046, quests format, lua-scripting, mcp-live-editor, features index, rpg-save; readiness `coding_quest_runtime_flags` → done

### Schema / API / format deltas

- Errors `FLAG-RUNTIME-*`; quest `QUEST-RUNTIME-UNKNOWN-FORK` / `QUEST-RUNTIME-FORK-FLAG`
- Lua: `engine.flag_set|clear|has|list`, `engine.quest_resolve_fork`
- MCP: `engine_flag_call`; `engine_quest_call` kind `resolve_fork` (+ `forkId`, `outcomeFlag`)
- Save: `capture_from` / `hydrate_into` now take `FlagRuntime&` and fill `sharedCampaign.outcomeFlags`

### Seed / sample data

- No new quest seeds; uses existing `mq_act0_calrenoth` fork `larrell_save_vs_flee`

### Tests / verification evidence

- Suites listed above all pass; `engine` rebuilt

### Decisions & tradeoffs

- [DEC-0046](../../decisions/index.md#dec-0046-session-story-flag-runtime): FlagRuntime only; journal deferred

### Leftover risk / follow-ons

- Soft-gate consumers still need `coding_soft_gate_system`
- Journal UI still `ui_quest_journal` / TICKET-0062
- Reload Cursor MCP after rebuild to pick up `engine_flag_call`

## Agent notes

Owner chose 1A+2A (2026-07-24): FlagRuntime only; journal deferred to its own ticket.
