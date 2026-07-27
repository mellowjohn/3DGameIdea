# TICKET-0214: Co-op lobby UI canvases (host/join/ready/reconnect)

- Epic: EPIC-0017
- Status: needs-approval
- Agent: unassigned
- Priority: P3
- Notion: https://app.notion.com/p/3a5d3efc56958190aaa6dce6f84e3c51

## Goal

Ship player-facing **UI canvases** for the co-op lobby flow (host create, guest join, dual ready, reconnect pause) wired to `GameSession` state ([DEC-0042](../../decisions/index.md#dec-0042-online-co-op-session-model-mode-locked-saves), [DEC-0025](../../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp)).

## Context links

- [DEC-0042](../../decisions/index.md#dec-0042-online-co-op-session-model-mode-locked-saves)
- [DEC-0025](../../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp)
- [`context/features/co-op-sessions.md`](../../features/co-op-sessions.md) — lobby UX flow
- [`context/features/ui-canvas.md`](../../features/ui-canvas.md)
- Blocked by: TICKET-0212 (`GameSession` state hooks)

## Acceptance criteria

- [x] Versioned canvas assets under sample project: `coop_lobby_host.uicanvas.json`, `coop_lobby_join.uicanvas.json`, `coop_ready_room.uicanvas.json`, `coop_reconnect.uicanvas.json` (names stable in docs).
- [x] **Host flow:** main menu → Co-op → host lobby canvas → ready room when guest connected (local mock guest OK for v1).
- [x] **Guest flow:** join canvas with invite/code field (local mock accepts dev code) → ready room.
- [x] **Ready room:** per-slot character summary panel, **Ready** toggle per player, host-only **Start** button enabled iff `host.ready && guest.ready && guest.connected`.
- [x] **Reconnect overlay:** shown in `paused_waiting_guest`; message “Waiting for partner…”; host **End session** button calls `GameSession::end_session()`.
- [x] Canvas stack integration: lobby canvases push over main menu; pop on start/end; HUD remains on always-on layer during play.
- [x] Lua or C++ thin bindings: `coop_set_ready(slot, bool)`, `coop_host_start()`, `coop_end_session()` → `GameSession` transitions.
- [x] Desktop verification steps documented; headless: canvas JSON validates via `engine_project_validate`.

## Out of scope

- Real online invite/NAT (TICKET-0215).
- Full character creation fields (reuse archetype picker stub from DEC-0009 until creation ticket lands).
- Unanimous fork modal (TICKET-0217).
- Solo menu changes beyond adding **Co-op** entry.

## Dependencies

- **Blocked by:** TICKET-0212.
- **Blocks:** TICKET-0215 (R0 replaces local mock join with net join).

## Verification

- Rebuild `engine`.
- `engine validate --project samples/open-world-rpg` — new canvas assets pass.
- Desktop QA (required): host create → mock guest join → both ready → start → playing; disconnect guest → reconnect overlay → host end session.
- Document desktop steps in **What changed**.

## What changed

### Summary

Shipped co-op lobby UI canvases and Lua/editor wiring on top of `GameSession` ready gates + local mock invite (`COOP-LOCAL`). Host Start launches dual-slot play-test via blackboard; guest disconnect auto-pushes reconnect overlay.

### Files / surfaces

- **Canvases:** `samples/open-world-rpg/assets/ui/coop_lobby_host.uicanvas.json`, `coop_lobby_join.uicanvas.json`, `coop_ready_room.uicanvas.json`, `coop_reconnect.uicanvas.json`; `main_menu.uicanvas.json` (**Co-op**).
- **Lua:** `samples/open-world-rpg/assets/scripts/ui_handlers.lua` — full lobby button flow.
- **Session:** `GameSession` — `ready` on slots, `set_ready` / `can_host_start` / `mock_guest_join` / invite code; fixed `begin_solo` brace-init after `ready` field (`device_index` was shifted).
- **Lua runtime:** `coop_*` APIs + `ui_canvas_set_enabled` / `ui_canvas_set_text`; `set_game_session`.
- **Editor:** registers four coop canvases; `process_coop_lobby_editor_hooks` for play-test restart + reconnect overlay.
- **HUD:** `widget_display_label` prefers runtime `set_text(bind)` so Ready labels refresh.
- **Docs:** `context/features/co-op-sessions.md` lobby UI section.
- **Tests:** `game_session` suite covers ready gate + bad invite (46/46).

### Schema / API

- Lua: `coop_begin_host_lobby`, `coop_mock_guest_join`, `coop_set_ready`, `coop_toggle_ready`, `coop_host_start`, `coop_end_session`, `coop_can_host_start`, `coop_invite_code`.
- Blackboard: `coop.request_play_test`, `coop.request_end_test`.
- Errors: `GAME-SESSION-NOT-CONNECTED`, `GAME-SESSION-BAD-INVITE`.

### Verification evidence

- MSBuild Debug: `engine_core`, `engine_suite_tests`, `engine` — success (pre-existing C4996 `getenv` in `render_app.cpp`).
- `engine_suite_tests --suite game_session` → 46/46.
- `engine validate --project samples/open-world-rpg` → valid.
- Desktop QA path (owner): Esc → Main Menu → **Co-op** → **Simulate Guest Join** → Ready host + guest → **Start** → dual-slot play; **F8** → reconnect overlay → **End Session**.

### Decisions

- Mock invite only until TICKET-0215; Start still connection-authoritative in C++, UI gated by `can_host_start()`.

### Leftover risk

- Desktop QA not run in this agent session (no interactive click-through). Character summaries are stub Squire/Archer strings.
- Real net join / NAT still open (0215).

## Agent notes

Use mock/local guest connection until TICKET-0215 wires real join. Character create in lobby can be minimal (archetype dropdown + display name).
