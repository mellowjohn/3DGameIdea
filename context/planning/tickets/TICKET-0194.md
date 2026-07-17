# TICKET-0194: Editor Project Sync panel

- Epic: EPIC-0014
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: (mirror after Notion MCP auth — Tickets DB)

## Goal

Add an in-engine **Project Sync** surface so authors can see git status and run fetch / pull / commit / push without leaving the editor, backed entirely by TICKET-0193 commands.

## Context links

- [DEC-0037](../../decisions/index.md#dec-0037-git-backed-authoring-sync-in-editor)
- [`../features/authoring-git-sync.md`](../../features/authoring-git-sync.md)
- [`../features/editor-mvp.md`](../../features/editor-mvp.md)
- TICKET-0193

## Acceptance criteria

- [x] Diagnostics panel shows Project Sync with branch, summary, dirty/conflict detail.
- [x] Buttons: Status, Fetch, Pull, Commit (message field), Push — each calls `apply_project_git_operation`.
- [x] Status/error text uses structured command summaries/diagnostics.
- [x] Conflicted / changed paths listed in the detail pane.
- [x] Docs updated (`authoring-git-sync.md`, `editor-mvp.md`).

## Out of scope

- Implementing the git command layer (0193)
- Full merge editor / three-way diff UI
- GitHub PR creation from the engine
- Storing credentials in project settings

## Dependencies

- Blocked by: TICKET-0193 (landed same change)
- Soft: TICKET-0195 reload offer integrated in the same panel

## Verification

Code review + Windows editor smoke (blocked here). No separate UI suite.

## What changed

- Summary: Diagnostics → **Project Sync (git)** panel for status/fetch/pull/commit/push with commit message field and detail list.
- Files: `src/rendering/render_app.cpp` (EditorState fields + `draw_project_sync_panel`).
- Leftover risk: UI not interactively verified on Windows in this environment.

## Agent notes

Reload-after-pull UX lives in the same panel (0195).
