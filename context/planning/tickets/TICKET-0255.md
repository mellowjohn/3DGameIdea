# TICKET-0255: Apply Dom kit feedback (Damius, Ashfell, Underflow)

- Epic: EPIC-0003
- Status: proposed
- Agent: unassigned
- Priority: P2
- Notion: https://app.notion.com/p/3b1d3efc569581089569f10a9945aedc

## Goal

Update Act 0 character concept direction and regenerate priority concept sheets so **Damius** reads as a leather-cloak scout, **Ashfell Blade** starter reads as a warrior (mail/plate cues), and **Underflow orcs** show clan / water / corruption variation — matching Dom locks from 2026-08-03.

## Context links

- [`../../design/recording_ld_character_concepts_2026-08-03.md`](../../design/recording_ld_character_concepts_2026-08-03.md)
- Dom D-P1-24 / D-P1-25 / D-P2-20 in [`../../design/dom-answered-questions.md`](../../design/dom-answered-questions.md)
- [`../../art/character-direction.md`](../../art/character-direction.md)
- [`../../art/concepts/README.md`](../../art/concepts/README.md)
- Heraldry: [`../../art/cartography-design.md`](../../art/cartography-design.md) — stamp from shipped `emblemPath`s only

## Acceptance criteria

- [ ] `act0-char-damius.png` (or successor) shows leather armor, cloak, and restrained Tessera heraldry — not a peasant walker.
- [ ] Ashfell starter concept / turnaround updated with early warrior mail/plate discrepancy; character-direction Ashfell section matches.
- [ ] Underflow orc direction notes + at least one concept variant board: banners, water/deep-sea emblem, optional tentacle/corruption cue; uses `underflow` heraldry master (no invented crest).
- [ ] `character-direction.md` + `concepts/README.md` point at the new sheets.
- [ ] No alternate body mesh — kits stay on **GoodPlayerModel** (orcs may keep bulk kit exception already documented).

## Out of scope

- Baking / importing meshes into `samples/open-world-rpg` (follow-on import tickets).
- Naming Ashfell / Lodge / Guild faces (D-P1-21b / D-P1-22).
- Cristallo swan-knight full kit production (direction already noted; Act 1+).

## Dependencies

- Soft: Imperium ladder concepts already shipped; do not regress those sheets.
- Parallel OK with TICKET-0256.

## Verification

- Visual review of updated PNGs against Dom criteria in the recording takeaways.
- Grep/scan for “peasant” Damius direction removed from active art docs.
- Doc-only ticket may reach needs-approval after concept + direction update without engine rebuild.

## What changed

*(Fill before needs-approval.)*

## Agent notes

Dom: Ashfell max fantasy = swan-knight / aura; starter must not look like a pleb vs other two kits.

2026-08-03 concept pass (partial): regenerated `act0-char-damius.png`, `act0-char-ashfell-blade.png`, `act0-char-outrider.png`, `act0-char-runecaster.png`, `act0-char-player-archetypes.png`; updated `character-direction.md` kit sections + lane-org draft marks. **Still open on this ticket:** Underflow clan / water / corruption variant board.
