# TICKET-0182: Editor Design Docs tab (read-only context MD)

- Epic: EPIC-0009
- Status: needs-approval
- Agent: cursor-agent
- Priority: P0
- Notion: https://app.notion.com/p/39fd3efc5695816c8ab0f082d1cfc420

## Goal

Add a Viewports **Design Docs** tab so designers can read authoritative repo markdown (`context/features`, plus story/art) inside the editor — read-only, no second source of truth.

## Context links

- `context/features/index.md`
- `context/features/editor-mvp.md`
- DEC-0015 hybrid tracking (Notion/`epics.md` stay the backlog; this is a reader)

## Acceptance criteria

- [x] Viewports tab **Design Docs** (disabled during play test, like World Forge).
- [x] Lists markdown under `context/features/`, `context/story/`, `context/art/` from `ENGINE_REPOSITORY_ROOT`.
- [x] Filter by doc `Status:` line (All / active / planned / complete / other).
- [x] Selecting a file shows read-only body from disk (lightweight markdown display).
- [x] Refresh button + periodic rescan.
- [x] Context + epics updated; rebuild `engine`.

## Out of scope

- Editing markdown in-engine
- Notion sync / ticket kanban inside the editor
- Full GFM (images, rich tables)

## Dependencies

None. Owner P0 override.

## Verification

- Rebuilt `engine` (MSVC Debug) after kill.
- Manual: open editor → **Design Docs** → filter/status → open `open-world-navigation.md` / `streaming-lod-budgets.md`.

## What changed

- Summary: New Viewports **Design Docs** tab browses repo context markdown read-only with status filter and simple markdown rendering for designer transparency.
- Files: `render_app.cpp` (tab + browser), `editor_icons.h` / `game_fonts.cpp` (book icon), `editor-mvp.md`, features index, epics, this stub, Notion.
- Schema / API: none.
- Seed / sample data: none.
- Tests / verification: engine rebuild succeeded; UI smoke is manual.
- Decisions & tradeoffs: reader only; backlog stays Notion/`epics.md`.
- Leftover risk: table rows render as muted plain text; requires `ENGINE_REPOSITORY_ROOT` at build time.

## Agent notes

Implemented immediately as owner P0.
