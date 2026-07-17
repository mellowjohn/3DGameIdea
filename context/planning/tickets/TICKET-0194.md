# TICKET-0194: Editor Project Sync panel

- Epic: EPIC-0014
- Status: proposed
- Agent: unassigned
- Priority: P2
- Notion: (mirror after Notion MCP auth — Tickets DB)

## Goal

Add an in-engine **Project Sync** surface so authors can see git status and run fetch / pull / commit / push without leaving the editor, backed entirely by TICKET-0193 commands.

## Context links

- [DEC-0037](../../decisions/index.md#dec-0037-git-backed-authoring-sync-in-editor)
- [`../features/authoring-git-sync.md`](../../features/authoring-git-sync.md)
- [`../features/editor-mvp.md`](../../features/editor-mvp.md)
- TICKET-0193 (command path)

## Acceptance criteria

- [ ] Editor exposes a Project Sync panel or Diagnostics/toolbar section showing branch, ahead/behind (when available), and dirty file summary.
- [ ] Buttons: Fetch, Pull, Commit (message field), Push — each calls the shared automation path (no duplicate git logic in ImGui).
- [ ] Status/error text uses structured command errors (auth failure, conflicts, missing git).
- [ ] Conflicted paths are listed; no silent overwrite.
- [ ] Docs in `authoring-git-sync.md` + `editor-mvp.md` describe where to find the panel.
- [ ] Basic UI smoke or automation coverage as practical.

## Out of scope

- Implementing the git command layer (0193)
- Full merge editor / three-way diff UI
- GitHub PR creation from the engine
- Storing credentials in project settings

## Dependencies

- Blocked by: TICKET-0193
- Soft: TICKET-0195 for post-pull reload prompt integration

## Verification

Rebuild `engine`; manual: sync loop between two clones of a sample project; confirm World Forge files appear after pull + reload (0195).

## What changed

(Fill before `needs-approval`.)

## Agent notes

Place UI near existing project/diagnostics chrome so World Forge authors discover it without hunting.
