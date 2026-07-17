# TICKET-0193: Command-backed project git ops (status/fetch/pull/commit/push)

- Epic: EPIC-0014
- Status: proposed
- Agent: unassigned
- Priority: P2
- Notion: (mirror after Notion MCP auth — Tickets DB)

## Goal

Expose project-root git operations through a stable automation command (and MCP tool) so GUI and headless clients share one path: status, fetch, pull, commit, push — wrapping system `git`, with structured errors.

## Context links

- [DEC-0037](../../decisions/index.md#dec-0037-git-backed-authoring-sync-in-editor)
- [DEC-0003](../../decisions/index.md#dec-0003-automation-first-tools)
- [`../features/authoring-git-sync.md`](../../features/authoring-git-sync.md)
- TICKET-0192 (workflow doc)

## Acceptance criteria

- [ ] Automation command (name finalized in implementation; e.g. `project_git` / `engine_project_git`) supports `action=status|fetch|pull|commit|push`.
- [ ] Operates on the opened project root; refuses when not a git working tree or when `git` is missing (typed error + remedy).
- [ ] `commit` requires an explicit message; stages only project content paths (never force-add build artifacts / secrets).
- [ ] `pull` / `push` use existing remotes; auth via OS credential helper / SSH — no secrets stored by the engine.
- [ ] JSON output includes branch, dirty summary, and conflicted paths when present.
- [ ] Named suite tests cover happy path with a temp git repo fixture and failure paths (no git, not a repo, conflict stub).
- [ ] Context formats/automation docs updated.

## Out of scope

- Editor ImGui panel (TICKET-0194)
- Merge-conflict resolution UI
- Embedding libgit2 (v1 wraps CLI unless a later decision supersedes)
- Auto-commit on Save

## Dependencies

- Soft: TICKET-0192 docs for operator language
- Blocks: TICKET-0194 (UI), soft-blocks TICKET-0195 reload hooks

## Verification

Rebuild `engine`; run the new/extended suite; manual: status/fetch/pull/commit/push against a throwaway clone.

## What changed

(Fill before `needs-approval`.)

## Agent notes

Safe default: system `git` on PATH. Prefer not inventing a second VCS abstraction.
