# TICKET-0041: Shader authoring strategy (graphs vs code-first)

- Epic: EPIC-0005
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ad3efc569581be89b8cde79a3aeb1c

## Goal

Record the shader authoring strategy so agents and humans do not invest in a Unity-style node graph before the MCP/agent path is clear.

## Context links

- `context/planning/epics.md` (EPIC-0005)
- [DEC-0049](../../decisions/index.md#dec-0049-agent-writable-material-shader-profiles)
- `context/formats/materials.md`
- `context/architecture/content-vs-engine-workflows.md`
- Follow-ons: TICKET-0238–0241

## Acceptance criteria

- [x] Decision recorded as DEC-0049 (code-first master shaders + JSON params; no shader graph for v1).
- [x] Trade-offs documented (agent/MCP authorship vs human visual graph).
- [x] Implementation children seeded in epics.md (0238–0241).
- [x] Features index row updated away from “shader graphs” as the default path.

## Out of scope

- Implementing shader graph UI or codegen.
- Shipping runtime masters (TICKET-0238+).

## Dependencies

None. Unblocks TICKET-0238–0241.

## Verification

Doc review: DEC-0049 present; epics.md EPIC-0005 children listed; TICKET-0041 acceptance checked.

## What changed

- Summary: Owner-directed research concluded agents need expandable JSON look vocabulary, not node graphs. DEC-0049 locks code-first masters; EPIC-0005 gains TICKET-0238–0241.
- Files / surfaces touched: `context/decisions/index.md`, `context/planning/epics.md`, this stub, features index / material-shader-profiles feature note.
- Schema / API / format deltas: none in this ticket (decision only).
- Seed / sample data: none.
- Tests / verification evidence: doc-only.
- Decisions & tradeoffs: DEC-0049.
- Leftover risk / follow-ons: humans may still want a surface graph later; keep agent path as JSON params into masters.

## Agent notes

Decision taken in chat 2026-07-28 with owner: prioritize MCP/AI artistic capability via master shaders + JSON.
