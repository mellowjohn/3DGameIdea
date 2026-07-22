# TICKET-0110: M5 exit: animation tests + CLI/editor previews

- Epic: EPIC-0008
- Status: needs-approval
- Agent: cursor-agent
- Priority: P0
- Notion: (see Notion Tickets DB by Ticket ID)

## Goal

Close **M5** with automated animation/collision/interaction evidence and CLI/editor preview hooks so the milestone is verifiable without relying on owner mobile QA.

## Context links

- `context/planning/epics.md` (EPIC-0008)
- [`context/roadmap.md`](../../roadmap.md) — M5 exit criterion
- [`context/features/animator.md`](../../features/animator.md)
- TICKET-0101–0104 (done), 0105–0106 (needs-approval desktop QA)

## Acceptance criteria

- [x] Named suite coverage for: clip load/hot-reload, blend/state transitions, root-motion sync, animation events firing to Lua (or stub host when no script bound).
- [x] CLI or headless preview path documents sample player controller + clip playback (e.g. `engine` subcommand or suite artifact) with deterministic output.
- [x] Editor preview hook documented: where to inspect animator state during play test (Diagnostics or minimal panel — full Animation tools = TICKET-0135).
- [x] `context/roadmap.md` M5 section updated to reflect exit evidence; any new CLI documented in features/index.
- [ ] Rebuild `engine`; relevant suites green (`animator`, `character`, `scripting` as applicable).

## Out of scope

- miniaudio (TICKET-0107)
- Full IK solve (0106 metadata only)
- Animation tools panel UI (0135)
- M6 quest/dialogue runtime

## Dependencies

- Soft: 0105/0106 in needs-approval — tests should pass against shipped code; do not block on owner approval.

## Verification

Rebuild `engine`; run animation-related suites + new/extended tests. Set Status → `needs-approval` with suite counts and CLI sample output in **What changed**.

## What changed

- Summary: Added M5 exit gate CLI — `engine test --suite m5-exit` (animator+character+interaction+combat+scripting) and `engine animation-preview` headless controller tick with deterministic JSON. Sample project now ships `assets/animators/example.animator.json` + `assets/models/player_clips.gltf`. Documented editor preview via Scene/Prefab Inspector until TICKET-0135.
- Files / surfaces: `include/engine/animation/animation_preview.h`, `src/animation/animation_preview.cpp`, `src/automation/command.cpp`, `tests/suite_tests.cpp`, sample assets, `context/features/animator.md`, `context/roadmap.md`, `context/features/index.md`, `CMakeLists.txt`.
- Schema / API / format deltas: new CLI commands `animation-preview`; test pseudo-suite `m5-exit`; `run_animation_preview` / `find_default_animator_controller` API.
- Seed / sample data: `samples/open-world-rpg/assets/animators/example.animator.json`, `player_clips.gltf`.
- Tests / verification evidence: extended `animator` + `automation` suites with preview smoke. **Rebuild not run** in Linux cloud agent (Windows MSVC/D3D12 target).
- Decisions & tradeoffs: Live play-test animator Diagnostics deferred to TICKET-0135; headless CLI satisfies mobile-deferred owner QA per 2026-07-22 reprioritization.
- Leftover risk: Owner should run on Windows: `engine test --project samples/open-world-rpg --suite m5-exit` and `engine animation-preview --project samples/open-world-rpg --json`.

## Agent notes

2026-07-22: Implemented TICKET-0110. Cloud agent cannot rebuild `engine.exe`; suite counts pending owner Windows verification.
