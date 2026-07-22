# TICKET-0135: Animation tools panel (Diagnostics-adjacent)

- Epic: EPIC-0009
- Status: proposed
- Agent: unassigned
- Priority: P2
- Notion: (see Notion Tickets DB by Ticket ID)

## Goal

Add an editor **Animation** tools surface beside (or switchable with) the existing **Diagnostics** panel so authors can browse/manage clips, controllers, and rig metadata, and preview playback — without inventing a second viewport tab.

## Context links

- `context/planning/epics.md` (EPIC-0009 / EPIC-0008)
- [`../../features/animator.md`](../../features/animator.md)
- [`../../formats/rig-assets.md`](../../formats/rig-assets.md) (TICKET-0106)
- Owner request 2026-07-21: switch Diagnostics area to animation manage/preview

## Acceptance criteria

- [ ] Editor UI: Animation tools reachable from the Diagnostics docking area (tab/mode switch — exact chrome TBD at pickup).
- [ ] List / inspect animation clips + animator controllers in the project.
- [ ] Show linked `*.rig.json` IK hooks / bone roles (read-only minimum).
- [ ] Preview playback controls once GPU skinning / pose path exists (may stub until then).
- [ ] Context + epics updated; rebuild `engine`.

## Out of scope

- Full IK solver authoring (follow-on after TICKET-0106 metadata)
- Replacing Diagnostics entirely
- World Forge / viewport tab proliferation

## Dependencies

- Soft: TICKET-0106 rig metadata (shipped schema)
- Soft: GPU skinning / M5 exit preview path for rich playback

## Verification

Manual editor smoke + rebuild `engine`. Set Status to needs-approval after verification — never done.

## Agent notes

Elevated from P3 stub on owner request (2026-07-21). Expand acceptance further when picked up.
