# TICKET-0212: GameSession runtime + solo/co-op mode fork

- Epic: EPIC-0017
- Status: needs-approval
- Agent: cursor-agent
- Priority: P3
- Notion: _(create when mirroring EPIC-0017)_

## Goal

Introduce a headless **`GameSession`** that owns solo vs co-op campaign mode, session state transitions, and player slots so co-op rules from [DEC-0042](../../decisions/index.md#dec-0042-online-co-op-session-model-mode-locked-saves) are enforceable before networking lands.

## Context links

- [DEC-0042](../../decisions/index.md#dec-0042-online-co-op-session-model-mode-locked-saves)
- [`context/features/co-op-sessions.md`](../../features/co-op-sessions.md) — state machine + implementation order
- [`context/formats/rpg-save.md`](../../formats/rpg-save.md) — `sessionMode` (hydrate hook only; full save in TICKET-0114)
- Blocks: TICKET-0214 (lobby UI), TICKET-0215 (R0 net)

## Acceptance criteria

- [x] `GameSession` C++ type with `sessionMode` (`solo` | `coop`), `playerSlot` 0/1 assignment, and states: `menu`, `solo_loading`, `coop_lobby`, `coop_loading`, `playing`, `paused_waiting_guest`, `ended` (names may be enum `GameSessionState`).
- [x] Solo path: `sessionMode=solo` → `solo_loading` → `playing` with one active human slot (slot 0); existing F5 play-test behavior preserved when no co-op flag.
- [x] Co-op path (local/offline prove-out): `sessionMode=coop` requires slot 0 **and** slot 1 marked connected before `playing`; fail closed with stable error `GAME-SESSION-COOP-NEEDS-GUEST` if guest absent.
- [x] Local dual-slot play test: spawn **two** player entities in scene (or second spawn beside host); route input device 0 → slot 0, device 1 → slot 1; shared camera leash stub (midpoint or follow host — document choice in feature doc).
- [x] `paused_waiting_guest`: entering from `playing` freezes world sim tick (quest/standing/Lua stepping paused); resume to `playing` when slot 1 reconnect flag set; `end_session()` → `ended` without changing `sessionMode` to solo.
- [x] `GameSession` owns references or bind hooks to existing `QuestRuntime` / `StandingRuntime` (single shared instances per session).
- [x] Headless **`game_session`** CTest suite: solo happy path; co-op blocked without guest; co-op playing with two local slots; pause/resume/end transitions.
- [x] Update [`co-op-sessions.md`](../../features/co-op-sessions.md) with API names and error codes.

## Out of scope

- Network transport (TICKET-0215).
- Lobby UI canvases (TICKET-0214).
- Save/load persistence (TICKET-0114).
- `PartyRuntime` companion caps (TICKET-0213).
- Unanimous story fork UI (TICKET-0217).
- Split-screen / couch co-op.

## Dependencies

- Soft: TICKET-0180 / TICKET-0181 runtimes exist (session binds them).
- Parallel OK with TICKET-0114 schema work; hydrate from save is follow-on in 0114.

## Verification

- Rebuild `engine` target — succeeded (pre-existing C4996 getenv warning in `render_app.cpp` may still appear).
- `engine_suite_tests --suite game_session` — **38/38**.
- `engine test --project samples/open-world-rpg --suite game_session` — CTest suite passed.
- Solo F5 play-test: default path uses `begin_solo` → `start_playing` (no `--coop-local`).
- Desktop: `engine editor --project samples/open-world-rpg --coop-local` → F5 dual players (WASD + arrows); F8/F9 disconnect/reconnect.

## What changed

### Summary

Campaign sessions now have a first-class `GameSession` with mode-locked solo vs co-op rules: co-op cannot enter playing without a guest, guest disconnect freezes simulation, and ending never downgrades co-op to solo. Editor `--coop-local` proves dual-slot input and midpoint camera before networking.

### Files / surfaces

- Created: `include/engine/session/game_session.h`, `src/session/game_session.cpp`
- Modified: `CMakeLists.txt`, `src/automation/command.cpp` (suite list + `--coop-local`), `include/engine/rendering/render_app.h`, `src/rendering/render_app.cpp`, `tests/suite_tests.cpp`
- Docs: `context/features/co-op-sessions.md` Engine API section

### Schema / API / format deltas

- Enums: `SessionMode`, `GameSessionState`, `CameraLeashMode::Midpoint`
- Errors: `GAME-SESSION-COOP-NEEDS-GUEST`, `GAME-SESSION-SOLO-NO-GUEST`, `GAME-SESSION-INVALID-STATE`, `GAME-SESSION-INVALID-SLOT`, `GAME-SESSION-HOST-REQUIRED`
- CLI: `--coop-local` on editor/run; suite name `game_session`

### Seed / sample data

- None (runtime-only)

### Tests / verification evidence

- `game_session` suite **38/38**; `engine test --suite game_session` pass; `engine` rebuilt

### Decisions & tradeoffs

- Camera leash = **midpoint** (documented). Guest local body is a second `CharacterController` offset +2m (host may remain Rigidbody). Arrow keys = device 1 until lobby UI/net.

### Leftover risk / follow-ons

- Desktop dual-player QA still recommended once owner is at machine.
- TICKET-0214 lobby canvases; TICKET-0114 save hydrate; TICKET-0215 net R0.

## Agent notes

Local dual-slot: `engine editor --project samples/open-world-rpg --coop-local`. F5 start; WASD host; arrows guest; F8 pause guest; F9 resume; Shift+F5 end.
