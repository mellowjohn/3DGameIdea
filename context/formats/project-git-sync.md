# Project Git Sync (`engine project-git` / `engine_project_git`)

Status: active (EPIC-0014 / DEC-0037)  
Feature: [`../features/authoring-git-sync.md`](../features/authoring-git-sync.md)

## Purpose

Command-backed authoring sync wrapping system `git` for the opened project root. Shared by CLI, MCP, and the editor Project Sync panel.

## CLI

```text
engine project-git --project <dir> --action status|fetch|pull|commit|push [--message <text>] [--json]
```

Positional action is also accepted: `engine project-git --project <dir> status`.

## MCP

Tool: `engine_project_git`

| Field | Required | Notes |
| --- | --- | --- |
| `action` | yes | `status` \| `fetch` \| `pull` \| `commit` \| `push` |
| `message` | commit only | Non-empty commit message |
| `paths` | no | Optional explicit relative paths to stage (still filtered for blocked artifacts) |

Offline — does not require a live editor.

## Editor bridge

Operation: `project_git` with the same JSON params (used by Diagnostics → Project Sync).

## Behavior

- Resolves git toplevel from the project path (nested monorepo projects supported).
- `status` / `commit` are scoped to the project prefix when the project is not the repo root.
- `commit` stages project content only; skips `build/`, `.vs/`, binaries, and secret-like paths.
- `pull` uses `--ff-only`. Metadata may include `changedWorldForge`, `changedScene`, `requiresWorldForgeReload`, `conflictedPaths`.
- Auth uses OS git credential helper / SSH agent — the engine never stores remotes secrets.

## Error codes (representative)

| Code | Meaning |
| --- | --- |
| `PROJECT-GIT-GIT-MISSING` | `git` not on PATH |
| `PROJECT-GIT-NOT-A-REPO` | Project not inside a work tree |
| `PROJECT-GIT-MESSAGE-REQUIRED` | Empty commit message |
| `PROJECT-GIT-NOTHING-TO-COMMIT` | No stageable paths |
| `PROJECT-GIT-PULL-FAILED` | Pull/ff conflict or remote error |
| `PROJECT-GIT-PUSH-FAILED` / `PROJECT-GIT-FETCH-FAILED` | Network/auth/remote failure |

## Related

- Implementation: `include/engine/automation/project_git_commands.h`, `src/automation/project_git_commands.cpp`
- Tickets: TICKET-0193–0195
