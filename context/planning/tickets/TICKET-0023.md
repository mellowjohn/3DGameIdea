# TICKET-0023: Continuity checklist for new story docs

- Epic: EPIC-0003
- Status: needs-approval
- Agent: cursor-agent
- Priority: P1
- Notion: https://app.notion.com/p/39ad3efc5695819bb32cee56d2f9dce1

## Goal

Publish a reusable continuity checklist that authors and agents apply when adding or revising story docs, so draft vs established labels stay consistent and top blockers surface in `context/story/`.

## Context links

- `context/story/index.md` (Continuity Status)
- Existing story docs under `context/story/`
- `context/interviews/open-questions.md`
- Related: TICKET-0020, TICKET-0021

## Acceptance criteria

- [x] Checklist lives in repo (extend `context/story/index.md` or add `context/story/continuity-checklist.md`) covering: established vs draft labeling, naming conflicts, DEC conflicts, cross-doc links, open questions routing.
- [x] Top continuity blockers currently known are listed (or linked) from story index.
- [x] Agents can apply the checklist without inventing new canon.
- [x] `epics.md` Notes point at the checklist path.

## Out of scope

- Rewriting all story docs to final canon.
- Runtime quest/dialogue implementation.

## Dependencies

- Soft: benefits from TICKET-0021 faction gap list.
- Unblocks cleaner story doc PRs and agent story work.

## Verification

- Doc-only review; apply checklist once to an existing story file as a smoke example (optional note in Agent notes).

## Agent notes

- 2026-07-10: Added `context/story/continuity-checklist.md`; listed top blockers under Continuity Status in `context/story/index.md`. Smoke-applied checklist to `factions.md` (TICKET-0021 gap review). Awaiting owner approval.
