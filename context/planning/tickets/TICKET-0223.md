# TICKET-0223: World Forge Events pane + MCP kind

- Epic: EPIC-0002
- Status: needs-approval
- Agent: cursor-agent
- Priority: P1
- Notion: https://app.notion.com/p/3a7d3efc569581e5bd5bf1e01909775d

## Goal

Integrate event timelines into the World Forge editor umbrella: list/detail authoring and MCP `kind=events` so agents and humans edit Act 0 theatrical sequences the same way as quests/dialogues ([DEC-0045](../../decisions/index.md#dec-0045-json-event-timelines-with-c-sequencer-world-forge-home), [DEC-0020](../../decisions/index.md#dec-0020-world-forge-narrative-tooling-umbrella)).

## Context links

- [DEC-0045](../../decisions/index.md#dec-0045-json-event-timelines-with-c-sequencer-world-forge-home)
- [DEC-0020](../../decisions/index.md#dec-0020-world-forge-narrative-tooling-umbrella) / [`world-forge-scope.md`](../../features/world-forge-scope.md) — story events product home
- Runtime/schema: [TICKET-0221](TICKET-0221.md); camera steps: [TICKET-0222](TICKET-0222.md)
- MCP pattern: [`world-forge-mcp.md`](../../formats/world-forge-mcp.md); dialogues/quests panes as UX reference
- Lookup dropdowns rule: `.cursor/rules/lookup-fields-dropdowns.mdc`
- IDs from display names: `.cursor/rules/ids-from-display-names.mdc`

## Acceptance criteria

- [x] MCP `engine_world_forge_apply` supports `kind=events` for get | validate | apply on `events.worldforge.json` (same command path as other WF kinds).
- [x] World Forge left nav adds **Events**: list of sequences + detail editor for steps (kind combo, wait seconds, dialogue tree dropdown, emit name, camera fields).
- [x] Create flow: display name → slug id via `unique_slugify_id`; optional advanced id override only if already patterned elsewhere.
- [x] Cross-ref fields use dropdowns (`start_dialogue` tree ids from loaded dialogues; act tags).
- [x] Dirty/Reload/Save participates in existing World Forge session persistence.
- [x] Act lens filter applies via `acts[]` / tags (DEC-0036).
- [x] Suite or MCP offline test: apply round-trip + reject invalid step.
- [x] Docs: `world-forge-mcp.md`, `world-forge-events.md`, `editor-mvp.md` World Forge section, scope bullet 8 updated to shipped.

## Out of scope

- Multi-track cinematic scrubber / DopeSheet UI.
- In-viewport camera path gizmos (data fields only).
- Authoring full Landfall cinematic content (content tickets).
- Changing runtime step semantics (0221/0222 own that).

## Dependencies

- Blocked by TICKET-0221 (schema + asset). Soft prefer 0222 so camera step fields exist in the editor.
- Product home EPIC-0002; runtime delivery EPIC-0006.

## Verification

- Rebuild `engine` / `engine_suite_tests` — passed (C4996 getenv warning only; C4100 unused session warning in editor).
- `engine_suite_tests --suite world_forge` — **244/244** (includes events MCP get/validate/apply round-trip + reject invalid step).
- `engine validate --project samples/open-world-rpg` — exit 0.

## What changed

### Summary

World Forge authors event timeline sequences in-editor (Events pane) and via MCP `kind=events`, sharing the same validate/apply path as other narrative assets.

### Files / surfaces

- `src/automation/world_forge_commands.cpp` — `WorldForgeKind::Events`; aliases `events|event|sequences|timelines`; dialogue-ref soft-check on validate/apply
- `src/automation/mcp_server.cpp` — tool description lists `events`
- `include/engine/ui/world_forge_editor.h` + `src/ui/world_forge_editor.cpp` — Events nav, list/detail, create (slug), step editor, Act lens, Reload/Save
- `tests/suite_tests.cpp` — events validate/get/apply round-trip + invalid step reject
- Docs: `world-forge-mcp.md`, `world-forge-events.md`, `editor-mvp.md`, `world-forge-scope.md`

### Schema / API

- MCP: `engine_world_forge_apply` `kind=events` get | validate | apply → default path `assets/world-forge/events.worldforge.json`
- Editor: World Forge → **Events** pane; create display name → slug; step kinds include `look_at` fields from 0222

### Tests / verification evidence

world_forge **244/244**; project validate OK.

### Decisions & tradeoffs

Same command-backed persistence as dialogues/quests (DEC-0003 / DEC-0020). No DopeSheet — ordered step list only.

### Leftover risk / follow-ons

Desktop smoke: create sequence → Save → Reload. Full Landfall cinematic authoring is content work, not this ticket.

## Agent notes

Implemented after 0221/0222. Status → needs-approval for owner review (do not mark done).
