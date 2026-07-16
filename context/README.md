# Project Context Library

This directory is durable memory for humans and AI agents. Source code remains authoritative for implementation; this library records intent, contracts, status, rationale, and provenance.

## Index

- `formats/` — versioned project and asset format contracts (includes [`formats/world-forge-factions.md`](formats/world-forge-factions.md), [`formats/world-forge-relationships.md`](formats/world-forge-relationships.md), [`formats/world-forge-map.md`](formats/world-forge-map.md), [`formats/world-forge-quests.md`](formats/world-forge-quests.md), [`formats/world-forge-mcp.md`](formats/world-forge-mcp.md))

- `architecture/overview.md` — system boundaries and constraints
- `architecture/content-vs-engine-workflows.md` — when to edit C++ engine code vs MCP-driven project content
- `decisions/index.md` — accepted and superseded decisions
- `features/index.md` — feature inventory, status, and acceptance links
- `features/world-forge-scope.md` — World Forge vs editor/MCP boundary (EPIC-0002 / TICKET-0010)
- `story/index.md` — narrative premise, setting, characters, factions, and other story canon
- `art/character-direction.md` — player-character art references and starter-kit direction
- `resources/index.md` — dependencies, assets, tools, references, and licenses
- `interviews/open-questions.md` — unresolved owner decisions
- `planning/epics.md` — hybrid epic/ticket backlog with Priority (mirrored to Notion Wrathful Conquest)
- `planning/notion-sync.md` — Notion board properties, assignment, and sync checklist
- `planning/epic-template.md` — Notion epic page body template
- `planning/ticket-template.md` — ticket page/stub documentation template
- `planning/tickets/` — ticket briefs for agents (required for ready/active/needs-approval)
- `.cursor/rules/epic-ticket-population.mdc` — keep epics/tickets fully populated in repo + Notion (repo root)
- `skills/engine-ticket-workflow/SKILL.md` — pick up, prioritize, and close assigned tickets
- `roadmap.md` — milestones and exit criteria
- `testing/strategy.md` — layered verification and regression policy
- `testing/findings.md` — material defects, causes, fixes, and remaining risks
- `testing/coverage.md` — implemented, tested, partial, and untested areas

Use stable links and concise entries. Update the relevant index in the same change as the code or asset. Mark obsolete information as superseded rather than silently deleting rationale.
