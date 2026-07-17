# TICKET-0192: Document authoring sync workflow (git + World Forge)

- Epic: EPIC-0014
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: (mirror after Notion MCP auth — Tickets DB)

## Goal

Publish a clear human/agent checklist for sharing World Forge and other project authoring via git (save → commit → push / pull → reload), so multi-author collaboration works before in-editor sync UI lands.

## Context links

- [DEC-0037](../../decisions/index.md#dec-0037-git-backed-authoring-sync-in-editor)
- [`../features/authoring-git-sync.md`](../../features/authoring-git-sync.md)
- [`../architecture/content-vs-engine-workflows.md`](../../architecture/content-vs-engine-workflows.md)
- World Forge Save/Reload: [`../features/editor-mvp.md`](../../features/editor-mvp.md)

## Acceptance criteria

- [x] `context/features/authoring-git-sync.md` documents the multi-author loop with concrete paths (`assets/world-forge/*.worldforge.json`).
- [x] Checklist covers: pull before edit, Save in World Forge, commit message guidance, push, pull on the other machine, Reload World Forge.
- [x] Explicitly states this is **not** player cloud save and **not** live co-editing.
- [x] Conflict guidance: stop, resolve with git, do not hand-merge blindly in the editor.
- [x] `context/features/index.md` and `context/README.md` link the feature.
- [x] Follow-on tickets 0193–0195 remain accurate against the doc.

## Out of scope

- Implementing git commands or editor UI (0193–0195)
- Changing World Forge schemas
- Notion-only docs without repo context

## Dependencies

- Soft: World Forge Save/Reload already exist
- Blocks: none for docs; clarifies intent for 0193–0195

## Verification

Doc review — no engine rebuild. Links resolve; checklist matches DEC-0037; epic child rows present.

## What changed

- Summary: Accepted DEC-0037 (git-backed authoring sync with planned in-editor Project Sync). Added EPIC-0014 and a usable today-checklist in `authoring-git-sync.md` so collaborators can push/pull World Forge JSON before the editor UI lands.
- Files / surfaces touched: `context/decisions/index.md` (DEC-0037); `context/features/authoring-git-sync.md` (new); `context/features/index.md`; `context/README.md`; `context/architecture/content-vs-engine-workflows.md`; `context/planning/epics.md` (EPIC-0014); ticket stubs 0192–0195.
- Schema / API / format deltas: none (docs/decision only).
- Seed / sample data: none.
- Tests / verification evidence: doc-only review; no C++ rebuild.
- Decisions & tradeoffs: Git + system CLI over custom cloud; credential helper auth; no auto-commit; no merge UI in v1.
- Leftover risk / follow-ons: Notion Epics/Tickets mirror blocked until Notion MCP is authenticated in desktop Cursor. Implementation remains TICKET-0193–0195.

## Agent notes

Owner confirmed polish-then-in-editor-git-sync on 2026-07-17.
