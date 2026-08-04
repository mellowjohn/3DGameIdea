# TICKET-0254: Reopen player camp — Palworld placeable base vs DEC-0033

- Epic: EPIC-0004
- Status: proposed
- Agent: unassigned
- Priority: P2
- Notion: https://app.notion.com/p/3b1d3efc569581cb81ebce844a03930b

## Goal

Owner + Dom formally decide whether the player camp stays **DEC-0033 instance-primary** (DAO-style editable instance entered from overland) or amends toward a **Palworld-style placeable open-world camp/base** (map location, build-up, companions idle at camp, optional craft/forage later), then record a DEC amend/supersede and update navigation + beat-sheet language.

## Context links

- [`../../design/recording_ld_character_concepts_2026-08-03.md`](../../design/recording_ld_character_concepts_2026-08-03.md) — 2026-08-03 lean
- Dom **D-P1-23** in [`../../design/dom-open-questions.md`](../../design/dom-open-questions.md)
- [DEC-0033](../../decisions/index.md#dec-0033-anywhere-player-camp-as-editable-instance-dao-style) (current accepted)
- D-P0-10 / D-P1-19 camp tutorial locks (Arkand, storage, rest, travel)
- [`../../features/open-world-navigation.md`](../../features/open-world-navigation.md)
- [`../../interviews/open-questions.md`](../../interviews/open-questions.md) — Player camp section

## Acceptance criteria

- [ ] Written decision: keep DEC-0033, amend it, or supersede with a new DEC-#### covering placeable open-world base rules.
- [ ] Explicit v1 scope: what customization ships (furniture props vs full base-building); companions-at-camp yes/no; craft/forage/mine deferred or in.
- [ ] Combat-escape deny rule retained or explicitly rewritten.
- [ ] Dom **D-P1-23** moved to answered archive with the lock text.
- [ ] `open-world-navigation.md`, beat-sheet A0-09 / camp handoff, and interview open-questions updated to match.
- [ ] No full base-building runtime shipped in this ticket — design lock only unless owner expands scope.

## Out of scope

- Implementing camp building, mining, or companion work orders.
- Redesigning inventory camp storage (DEC-0050 already locks per-player chests).
- Act 1 hub / Ledgeport layout.

## Dependencies

- Soft: D-P1-19 Arkand tutorial lines still apply until rewritten.
- Blocks any large camp-building feature tickets until this lands.

## Verification

- Doc-only: DEC + Dom answered row + navigation feature note agree; no contradictory “instance-only” language left without a supersede note.
- Owner review of DEC text before Status → needs-approval.

## What changed

*(Fill before needs-approval.)*

## Agent notes

2026-08-03 session: John + Dom both liked Palworld placeable camp with companions chilling / optional crafting. DEC-0033 remains accepted until this ticket closes.
