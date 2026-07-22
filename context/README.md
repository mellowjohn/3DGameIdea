# Project Context Library

This directory is durable memory for humans and AI agents. Source code remains authoritative for implementation; this library records intent, contracts, status, rationale, and provenance.

## Index

- `formats/` — versioned project and asset format contracts (includes [`formats/animator-controller-assets.md`](formats/animator-controller-assets.md), [`formats/world-forge-pantheon.md`](formats/world-forge-pantheon.md), [`formats/world-forge-archetypes.md`](formats/world-forge-archetypes.md), [`formats/world-forge-acts.md`](formats/world-forge-acts.md), [`formats/world-forge-factions.md`](formats/world-forge-factions.md), [`formats/world-forge-relationships.md`](formats/world-forge-relationships.md), [`formats/world-forge-map.md`](formats/world-forge-map.md), [`formats/world-forge-quests.md`](formats/world-forge-quests.md), [`formats/world-forge-mcp.md`](formats/world-forge-mcp.md), [`formats/project-git-sync.md`](formats/project-git-sync.md))

- `architecture/overview.md` — system boundaries and constraints
- `architecture/components.md` — entity component catalog (core ECS + authored types, inherit/override, authoring matrix)
- `architecture/content-vs-engine-workflows.md` — when to edit C++ engine code vs MCP-driven project content
- `decisions/index.md` — accepted and superseded decisions
- `features/index.md` — feature inventory, status, and acceptance links
- `features/authoring-git-sync.md` — multi-author project sync via git + in-editor Project Sync (EPIC-0014 / DEC-0037)
- `features/world-forge-scope.md` — World Forge vs editor/MCP boundary (EPIC-0002 / TICKET-0010)
- `story/index.md` — narrative premise, setting, characters, factions, and other story canon
- `story/official-world-map.md` — canonical Tessera overworld map art (`official-world-map.png`)
- `design/README.md` — design tabs (Dom open questions, map-canvas explorations)
- `design/dom-open-questions.md` — prioritized P0/P1/P2 world-design questions Dom still needs to answer
- `design/dom-answered-questions.md` — archive of Dom + owner locks
- `art/character-direction.md` — player-character art references and starter-kit direction
- `art/blockbench-asset-list.md` — prioritized Blockbench production backlog (Act 0 → early Act 1)

- `art/theme-palette.md` — open-world slice color swatches (terrain, foliage, reserved Nefarium accents)
- `art/visual-direction.md` — stylized look, atmosphere, terrain, and typography baselines
- `art/cartography-design.md` — fantasy cartography language (Map Canvas, overworld reference, culture type, roads)
- `resources/index.md` — dependencies, assets, tools, references, and licenses
- `interviews/open-questions.md` — unresolved owner decisions
- `planning/epics.md` — hybrid epic/ticket backlog with Priority (mirrored to Notion Wrathful Conquest)
- `planning/notion-sync.md` — Notion board properties, assignment, and sync checklist
- `planning/epic-template.md` — Notion epic page body template
- `planning/ticket-template.md` — ticket page/stub documentation template
- `planning/tickets/` — ticket briefs for agents (required for ready/active/needs-approval)
- `.cursor/rules/epic-ticket-population.mdc` — keep epics/tickets fully populated in repo + Notion (repo root)
- `skills/engine-ticket-workflow/SKILL.md` — pick up, prioritize, and close assigned tickets
- `skills/grill-me/SKILL.md` — pressure-test ambiguities with pointed questions
- `skills/teach/SKILL.md` — explain a confusing agent question so the owner can answer it
- `skills/interview-engine-decisions/SKILL.md` — resolve blocking engine decisions and record them
- `roadmap.md` — milestones and exit criteria
- `testing/strategy.md` — layered verification and regression policy
- `testing/findings.md` — material defects, causes, fixes, and remaining risks
- `testing/coverage.md` — implemented, tested, partial, and untested areas

Use stable links and concise entries. Update the relevant index in the same change as the code or asset. Mark obsolete information as superseded rather than silently deleting rationale.
