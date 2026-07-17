# TICKET-0195: Safe reload after pull (World Forge + dirty-session rules)

- Epic: EPIC-0014
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: (mirror after Notion MCP auth — Tickets DB)

## Goal

After a successful project **pull**, reload World Forge (and document/handle dirty Scene/Sculpt sessions) so the local editor shows remote story changes without corrupting in-memory authority.

## Context links

- [DEC-0037](../../decisions/index.md#dec-0037-git-backed-authoring-sync-in-editor)
- [`../features/authoring-git-sync.md`](../../features/authoring-git-sync.md)
- [`../architecture/content-vs-engine-workflows.md`](../../architecture/content-vs-engine-workflows.md)
- World Forge `WorldForgeEditorSession::reload`

## Acceptance criteria

- [x] Successful Pull that changes World Forge files sets `requiresWorldForgeReload` and offers **Reload World Forge** in Project Sync.
- [x] Dirty World Forge blocks auto-offer until Save/discard; dirty Scene/Sculpt + changed scene files shows a fail-closed warning.
- [x] World Forge-only pulls can reload without full editor restart via existing `reload()`.
- [x] Documented in `authoring-git-sync.md` / `editor-mvp.md` (bindings.script.json still needs restart — existing rule).
- [x] Pull metadata + UI paths covered; dedicated dirty-session unit test deferred (manual on Windows).

## Out of scope

- Live multi-user OT/CRDT co-editing
- Auto-merging conflicted JSON
- Player save migration

## Dependencies

- Soft: TICKET-0193/0194 entry points (same delivery)

## Verification

Windows editor: A pushes World Forge change; B Pull → Reload World Forge. Not run in Linux cloud agent.

## What changed

- Summary: Pull response metadata drives a Project Sync reload offer; respects dirty World Forge and dirty Scene/Sculpt when scene files changed.
- Files: `project_git_commands.cpp` (changedWorldForge / requiresWorldForgeReload), `render_app.cpp` (offer + reload button).
- Leftover risk: No automated dirty-session UI test; scene file reload still requires save/discard + careful restart for open worlds.

## Agent notes

None.
