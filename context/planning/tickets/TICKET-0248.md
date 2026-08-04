# TICKET-0248: Animation viewport tab + isolated sandbox stage

- Epic: EPIC-0019
- Status: needs-approval
- Agent: cursor-agent
- Priority: P1
- Notion: https://app.notion.com/p/3b1d3efc5695812c81d4dcea51f92d6f

## Goal

Add an **Animation** viewport tab peer to Scene / Game that shows an isolated sandbox stage (void + optional base plate) with its own camera — not the open-world scene — so later studio tickets have a place to spawn and preview characters.

## Context links

- [`context/features/animation-studio.md`](../../features/animation-studio.md)
- [`context/features/editor-mvp.md`](../../features/editor-mvp.md) — existing ViewportTab pattern
- [`context/features/animator.md`](../../features/animator.md)
- [`context/architecture/content-vs-engine-workflows.md`](../../architecture/content-vs-engine-workflows.md) — C++ editor UI
- Implementation: `EditorState::ViewportTab` + `draw_editor()` tab bar in `src/rendering/render_app.cpp`
- Supersedes scope of [`TICKET-0135.md`](TICKET-0135.md) (deferred)

## Acceptance criteria

- [x] **Viewport tab chrome:** Editor viewport tab bar includes **Animation** (icon + label) beside existing Scene / Sculpt / Game / UI / World Forge / Design Docs tabs; selecting it sets `active_viewport_tab` to a new `ViewportTab::Animation` (or equivalent) and does not leave Scene/Game selection sticky incorrectly.
- [x] **Isolated stage:** While Animation tab is active, the main viewport does **not** draw the open-world scene / terrain / placed entities as the primary stage. Clear color (or simple void) + optional authored base-plate primitives (e.g. ground quad/grid) only.
- [x] **Dedicated camera:** Animation tab has its own orbit/pan/zoom camera state (or a clearly separated camera), independent of Scene camera; switching away and back restores Animation camera without forcing Scene camera into the sandbox.
- [x] **No world mutation:** Entering/leaving Animation tab does not dirty or rewrite `vertical-slice.world.json` (or active scene document).
- [x] **Bottom strip hook:** When Animation tab is active, bottom-center panel shows an **Animation** support tab (or placeholder pane) distinct from Diagnostics content; Diagnostics remains reachable and unchanged when selected.
- [x] **Docs:** [`animation-studio.md`](../../features/animation-studio.md) + [`editor-mvp.md`](../../features/editor-mvp.md) note the new tab; [`features/index.md`](../../features/index.md) row updated.
- [x] **Build:** Rebuild `engine` succeeds.

## Out of scope

- Subject picker, skinned playback, gear, hand attach, timeline events, keyframe editing (0249–0253)
- MCP tools for Animation studio (follow-on unless trivial status exposure)
- Writing clip/controller assets
- Play-test session / Game tab behavior changes

## Dependencies

- Soft: GPU skinning / animator runtime already shipped (TICKET-0227 / 0103) — not blocked on 0110 `done` (owner override for studio work).
- Blocks: TICKET-0249+ (need the stage).
- Supersedes: TICKET-0135 (deferred).

## Verification

```powershell
# Rebuild after C++ changes (acquire build lease first on shared checkout)
# MSBuild engine target on build\windows-msvc-debug\AIRpgEngine.sln
engine validate --project samples/open-world-rpg --json
engine editor --project samples/open-world-rpg --frames 2 --hidden true --initial-viewport animation --json
```

**Desktop QA (required for approval):**

1. Open editor → **Animation** tab visible.
2. Stage is void/base-plate only (no world fortress/meadow).
3. Orbit camera works; switch to Scene and back — cameras stay independent.
4. Bottom strip shows Animation support placeholder; Diagnostics still works.
5. Scene document unchanged on disk after visiting Animation tab.

## What changed

- Summary: Added an **Animation** viewport tab with an isolated sandbox (dark void + base-plate quad), its own free-cam, and a bottom **Animation** support tab. Open-world terrain/foliage/water/entities are skipped while this tab is active; Scene camera pose is untouched.
- Files / surfaces: `src/rendering/render_app.cpp` (ViewportTab, sandbox WorldPassParams, cameras, Diagnostics tab); `include/engine/editor/editor_icons.h` (`ICON_FA_FILM`); docs under `context/features/animation-studio.md`, `editor-mvp.md`, `animator.md`, `features/index.md`.
- Schema / API / format deltas: MCP `viewportTab` / `--initial-viewport` accept `animation`; `WorldPassParams::sandbox_stage`.
- Seed / sample data: none.
- Tests / verification evidence: `engine` Debug rebuild OK (pre-existing C4996/C4456 warnings only); `engine validate` exit 0; hidden editor `--initial-viewport animation --frames 2` exit 0 (drawCalls=4 sandbox pass).
- Decisions & tradeoffs: Scene RT reused with sandbox gate (not a third RT); free-cam DebugCamera rather than OrbitCamera for empty stage framing.
- Leftover risk / follow-ons: Terrain may still stream in the background while Animation is open (not drawn); subject/skinning is TICKET-0249. Desktop camera independence should be eyeballed by owner.

## Agent notes

Owner brief 2026-08-03: isolated sandbox, not world-linked; primitives OK as base plate. Lease released after rebuild; MCP relaunched.
