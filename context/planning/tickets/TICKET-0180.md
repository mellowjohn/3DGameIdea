# TICKET-0180: Quest progression runtime (API + Lua + MCP)

- Epic: EPIC-0006
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39fd3efc569581819a40df0671159306

## Goal

Ship session-only `QuestRuntime` with explicit start/complete/abandon so dialogue, collect/kill scripts, and agents all fulfill objectives through one API ([DEC-0028](../../decisions/index.md#dec-0028-explicit-quest-progression-runtime)).

## Context links

- [DEC-0028](../../decisions/index.md#dec-0028-explicit-quest-progression-runtime)
- [DEC-0026](../../decisions/index.md#dec-0026-quest-owned-dialogue-hooks-multi-stage)
- [`context/formats/world-forge-quests.md`](../../formats/world-forge-quests.md)
- Soft: TICKET-0114 (save), TICKET-0062 (journal/markers); TICKET-0112 schema covered by 0050

## Acceptance criteria

- [x] `QuestRuntime` bind/start/complete_objective/abandon/status/list_active/dialogue_for_stage + `QUEST-RUNTIME-*` errors
- [x] Ordered objectives only; out-of-order complete rejected
- [x] Lua: `quest_start`, `quest_complete_objective`, `quest_abandon`, `quest_status`, `quest_dialogue_hook`
- [x] MCP `engine_quest_call` (live editor) kinds: start, complete_objective, abandon, status, list
- [x] Sample HUD bind `quest.objectiveText`
- [x] Headless suite coverage; rebuild `engine`
- [x] Session-only (no save format)

## Out of scope

Auto-complete from DialogueRuntime; story events; RPG save (0114); full journal UI (0062); schemaVersion bump.

## Dependencies

Owner override of M6 hold. Depends on TICKET-0050 quest asset.

## Verification

- Rebuild `engine` — succeeded (pre-existing C4996 getenv/sscanf in `render_app.cpp`)
- `engine_suite_tests --suite world_forge` — **114/114**
- `engine_suite_tests --suite automation` — **68/68** (includes headless `quest_call`)
- `scripting` 37/37, `hud` 96/96
- Live MCP `engine_quest_call` against running editor not exercised this session (headless bridge path covered)

## What changed

### Summary

Players and agents can start a quest, complete objectives in order, abandon, and query status through one session `QuestRuntime`. Scripts and MCP share that API; dialogue hooks are lookups only. Current objective summary shows on the player HUD via `quest.objectiveText`.

### Files / surfaces

- `include/engine/quest/quest_runtime.h`, `src/quest/quest_runtime.cpp`
- Lua host in `src/scripting/lua_runtime.cpp`; editor bind in `src/rendering/render_app.cpp`
- MCP `engine_quest_call` + `quest_call` bridge in `mcp_server.cpp` / `editor_session.cpp`
- Sample `player.hud.json` objective text widget
- Docs: DEC-0028, quests format Runtime section, lua-scripting, mcp-live-editor, features index

### Schema / API / format deltas

- Runtime errors `QUEST-RUNTIME-*`
- Lua: `engine.quest_start` / `quest_complete_objective` / `quest_abandon` / `quest_status` / `quest_dialogue_hook`
- MCP: `engine_quest_call` kinds `start|complete_objective|abandon|status|list`
- HUD bind `quest.objectiveText`
- Quest asset schemaVersion unchanged (still 1)

### Seed / sample data

- Removed accidental empty `test_quest_for_testing_functionality` from sample quests (restored 3 catalog seeds)

### Tests / verification evidence

- world_forge 114/114; automation 68/68; scripting 37/37; hud 96/96; `engine` rebuilt

### Decisions & tradeoffs

- [DEC-0028](../../decisions/index.md#dec-0028-explicit-quest-progression-runtime): explicit API only; session-only; MCP for agent tests

### Leftover risk / follow-ons

- No RPG save (TICKET-0114); no journal/markers (0062); no story-event auto-wire; reload Cursor MCP after rebuild to pick up `engine_quest_call`

## Agent notes

2026-07-16: Implemented per approved plan (option A + MCP).
