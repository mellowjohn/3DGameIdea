# TICKET-0110: M5 exit: animation tests + CLI/editor previews

- Epic: EPIC-0008
- Status: ready
- Agent: unassigned
- Priority: P0
- Notion: (see Notion Tickets DB by Ticket ID)

## Goal

Close **M5** with automated animation/collision/interaction evidence and CLI/editor preview hooks so the milestone is verifiable without relying on owner mobile QA.

## Context links

- `context/planning/epics.md` (EPIC-0008)
- [`context/roadmap.md`](../../roadmap.md) — M5 exit criterion
- [`context/features/animator.md`](../../features/animator.md)
- TICKET-0101–0104 (done), 0105–0106 (needs-approval desktop QA)

## Acceptance criteria

- [ ] Named suite coverage for: clip load/hot-reload, blend/state transitions, root-motion sync, animation events firing to Lua (or stub host when no script bound).
- [ ] CLI or headless preview path documents sample player controller + clip playback (e.g. `engine` subcommand or suite artifact) with deterministic output.
- [ ] Editor preview hook documented: where to inspect animator state during play test (Diagnostics or minimal panel — full Animation tools = TICKET-0135).
- [ ] `context/roadmap.md` M5 section updated to reflect exit evidence; any new CLI documented in features/index.
- [ ] Rebuild `engine`; relevant suites green (`animator`, `character`, `scripting` as applicable).

## Out of scope

- miniaudio (TICKET-0107)
- Full IK solve (0106 metadata only)
- Animation tools panel UI (0135)
- M6 quest/dialogue runtime

## Dependencies

- Soft: 0105/0106 in needs-approval — tests should pass against shipped code; do not block on owner approval.

## Verification

Rebuild `engine`; run animation-related suites + new/extended tests. Set Status → `needs-approval` with suite counts and CLI sample output in **What changed**.

## Agent notes

Elevated to **P0 / ready** 2026-07-22: owner deferred desktop QA on needs-approval backlog; this ticket closes M5 with headless evidence first.
