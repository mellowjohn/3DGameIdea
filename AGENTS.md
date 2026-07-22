# Agent Operating Guide

This repository builds a from-scratch 3D game engine designed for effective collaboration with human developers and AI coding tools.

## Start Here

1. Read `context/README.md` and context relevant to the task.
2. Read `context/decisions/index.md`; accepted decisions override assumptions.
3. Use the project skill at `skills/interview-engine-decisions/SKILL.md` when a missing choice could materially alter architecture, scope, compatibility, or acceptance criteria.
4. Use `skills/grill-me/SKILL.md` when the user wants aggressive ambiguity pressure-testing ("grill me"); use `skills/teach/SKILL.md` when they need a question explained before answering; use `skills/evaluate/SKILL.md` when reviewing `needs-approval` tickets or PRs for approve/defer/rework; use `skills/desktop-qa/SKILL.md` when the owner runs a Windows session to clear the approval backlog.
5. For assigned backlog work (`TICKET-####`, Ready/Active board, agent queue), use `skills/engine-ticket-workflow/SKILL.md` and the Priority ladder in `context/planning/epics.md`.
6. When creating or updating epics/tickets, follow `.cursor/rules/epic-ticket-population.mdc` (full `epics.md` + Notion bodies; `needs-approval` before owner `done`). When **authoring** or promoting tickets to `ready`, use `skills/write-engine-ticket/SKILL.md` for testable acceptance criteria and verification.
7. Inspect existing code and tests before proposing new abstractions.

## Content vs Engine Workflow

- Read `context/architecture/content-vs-engine-workflows.md` before substantive feature work.
- Use C++ (`src/`, `include/`) for new runtime capabilities: movement, physics, rendering, input, asset schemas, and editor behavior.
- Use MCP tools for scene, prefab, material, and Lua handler edits that existing commands already support.
- Call `engine_scene_plan` when the correct target is unclear; do not write open world JSON while the live editor owns the scene.
- Rebuild `engine` after C++ changes; validate project data after content batches.

## Engineering Rules

- Keep authoritative project data in documented, versioned, text-friendly formats where practical.
- Design engine capabilities for direct code use and automation through stable commands or APIs.
- Prefer deterministic commands, explicit inputs, machine-readable output, and actionable errors.
- Separate runtime, tools/editor, project content, tests, and generated artifacts.
- Do not commit generated build output or undocumented binary assets.
- Use permissively licensed open-source resources by default. Before adding any dependency, font, model, texture, animation, shader, audio file, dataset, or tool-distributed runtime, record its provenance and exact license and verify commercial-use, modification, and redistribution rights. Do not use assets marked personal-use, editorial-use, noncommercial, no-derivatives, or with unknown provenance.
- Treat Windows, Direct3D, GPU drivers, and other required platform SDK/runtime components as explicit platform exceptions: record their redistribution terms and never imply that they are project-owned or open source.
- Add tests with behavior changes. Record benchmark conditions with performance claims.
- Test boundary values, malformed input, cancellation, repeated operations, and failure recovery before happy-path polish. Every confirmed defect receives a regression test when technically reproducible.
- Keep changes narrow; do not mix unrelated refactors into feature work.

## Context Maintenance

Update the context library in the same change for accepted decisions; feature status or criteria; dependencies, assets, licenses, or tools; public interfaces and formats; and known limitations or deferred questions.

Link to source rather than duplicating implementation details. Never record secrets, credentials, generated output, or transient debugging notes.

Record material defects and surprising behavior in `context/testing/findings.md`, including reproduction, impact, cause, resolution, verification, and remaining risk. Do not record routine compile mistakes that have no reusable lesson.

## Definition of Done

A change is done when it builds, relevant tests pass, user-visible behavior is documented, and affected context indexes are current. If verification is blocked, state exactly what was not run and why.

## Delegation Boundaries

Use subagents for independent, bounded work such as test expansion, documentation/coverage audits, asset-format validation, benchmark analysis, and isolated future subsystems. Keep tightly coupled lifecycle work—rendering, streaming ownership, collision-world mutation, and cross-system transactions—with one owner until interfaces stabilize. Require subagent changes to pass the same named suites and context-maintenance rules.
