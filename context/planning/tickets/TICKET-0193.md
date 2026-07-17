# TICKET-0193: Command-backed project git ops (status/fetch/pull/commit/push)

- Epic: EPIC-0014
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: (mirror after Notion MCP auth — Tickets DB)

## Goal

Expose project-root git operations through a stable automation command (and MCP tool) so GUI and headless clients share one path: status, fetch, pull, commit, push — wrapping system `git`, with structured errors.

## Context links

- [DEC-0037](../../decisions/index.md#dec-0037-git-backed-authoring-sync-in-editor)
- [DEC-0003](../../decisions/index.md#dec-0003-automation-first-tools)
- [`../features/authoring-git-sync.md`](../../features/authoring-git-sync.md)
- [`../formats/project-git-sync.md`](../../formats/project-git-sync.md)
- TICKET-0192 (workflow doc)

## Acceptance criteria

- [x] Automation command `project-git` / MCP `engine_project_git` supports `action=status|fetch|pull|commit|push`.
- [x] Operates on the opened project root; refuses when not a git working tree or when `git` is missing (typed error + remedy).
- [x] `commit` requires an explicit message; stages only project content paths (blocks build/ secrets/binaries).
- [x] `pull` / `push` use existing remotes; auth via OS credential helper / SSH — no secrets stored by the engine.
- [x] JSON/metadata includes branch, dirty summary, and conflicted paths when present.
- [x] Named suite tests cover happy path with a temp git repo fixture and failure paths (not a repo, empty message, clean commit).
- [x] Context formats/automation docs updated.

## Out of scope

- Editor ImGui panel (TICKET-0194)
- Merge-conflict resolution UI
- Embedding libgit2 (v1 wraps CLI)
- Auto-commit on Save

## Dependencies

- Soft: TICKET-0192 docs
- Blocks: TICKET-0194 / soft-blocks 0195

## Verification

- Added `automation` suite coverage for project_git fixture + failure paths.
- **Rebuild blocked on this Linux cloud agent** (Windows MSVC / D3D12 / vcpkg not available). Owner should rebuild `engine` + run `engine_suite_tests --suite automation` on Windows.

## What changed

- Summary: Added `apply_project_git_operation` wrapping system git for status/fetch/pull/commit/push; wired CLI `project-git`, MCP `engine_project_git`, and editor op `project_git`.
- Files: `include/engine/automation/project_git_commands.h`, `src/automation/project_git_commands.cpp`, `command.cpp`, `mcp_server.cpp`, `editor_session.cpp`, `CMakeLists.txt`, `tests/suite_tests.cpp`, `context/formats/project-git-sync.md`.
- Schema / API: new CLI/MCP/editor action surface documented in project-git-sync.md.
- Tests: temp-repo commit/status/block-build + missing-repo / empty-message cases in automation suite (not executed here — no Windows build).
- Decisions: system git CLI; ff-only pull; project-scoped staging.
- Leftover risk: Windows rebuild + suite run required; push/fetch auth depends on local git setup.

## Agent notes

Implemented together with 0194/0195 on the same branch.
