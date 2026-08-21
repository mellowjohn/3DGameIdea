# TICKET-0280: Landfall player-experience loop + encounter contract

- Epic: EPIC-0011
- Status: proposed
- Agent: unassigned
- Priority: P0
- Notion: pending mirror to Tickets database

## Goal

Define a testable 20–30 minute Act 0 Landfall experience from boot through the post-siege camp so discovery, archetype expression, combat challenge, narrative consequence, sensory feedback, and fellowship reinforce one coherent player loop before the remaining graybox and integration work proceeds.

## Context links

- `context/story/campaign-beat-sheet.md` — locked A0-01…A0-09 spine and Larrell outcome
- `context/story/character-creation.md` — three starter archetypes and shared premise
- `context/features/gearing-system.md` — Act 0 loot slice, soft affinity, and no-XP progression
- `context/features/open-world-navigation.md` — discovery, soft gates, and camp travel
- `samples/open-world-rpg/assets/world-forge/act0_mvp_readiness.worldforge.json` — Act 0 progression gate
- DEC-0009, DEC-0021, DEC-0032, DEC-0033, DEC-0048, DEC-0050, DEC-0051
- TICKET-0237 — shipped thin inventory and Landfall finds
- TICKET-0269…0273 — Landfall approach, keep, drawbridge, vision, and camp grayboxes

## Acceptance criteria

- [ ] Create `context/design/act0-landfall-player-experience.md` with one ordered pacing table covering boot/prologue, creation, A0-03…A0-09, and camp; every row names target elapsed-time range, player verb, intended emotional beat, required content/system, recovery pressure, and handoff condition; total target is **20–30 minutes** for a first successful playthrough.
- [ ] The design identifies the repeating Landfall loop in observable terms: **notice → approach/choose → fight or interact → receive reward/consequence → observe companion/world response → continue**; the table identifies where each step is proven in play.
- [ ] A0-04 road blockers and A0-06 drawbridge defense each have an encounter card naming purpose, enemy composition/count range, wave/trigger structure, arena affordances, recovery opportunity, target duration, success condition, and death/retry checkpoint; no exact numeric balance is claimed without later playtest evidence.
- [ ] Each required combat encounter includes a contribution check for **Ashfell Blade**, **Outrider**, and **Runecaster**: one baseline attack pattern, one signature action/resource or explicitly scoped MVP substitute, and one readable encounter advantage per archetype; all three can complete the encounter without an off-lane weapon.
- [ ] The approach contains one optional, visually readable detour that is not required for `mq_act0_calrenoth`; the contract names its entry cue, return path, approximate time cost, environmental information, and reward placement (`vein_iron_pendant` unless the document records a canon-compatible replacement).
- [ ] The A0-07 Larrell choice has a same-session consequence matrix covering **save** and **flee/hostage**: flag/state, visible evacuation or capture result, transition delta, Arkand reaction, and A0-09 camp callback/presence change; both paths still reach A0-08 and camp without a dead end.
- [ ] At least one A0-03 or A0-05 dialogue attitude choice has a small, explicitly authored Arkand callback before or at camp; it must not create a new morality/faction system dependency.
- [ ] A sword/bow/rune-focus feedback matrix names required animation event, hit/impact response, audio cue, VFX cue, and camera/UI feedback plus the existing ticket/readiness owner for each; missing capabilities become follow-up rows in an existing ticket where possible rather than new parallel systems.
- [ ] A0-09 includes a non-combat decompression budget and ordered Arkand-led loop for outcome acknowledgment, optional companion talk, storage, rest, and travel; the contract remains valid under the currently accepted camp instance model and does not wait on TICKET-0254.
- [ ] Update TICKET-0269…0273 acceptance/dependencies with the approved pacing, discovery, encounter, consequence, and camp requirements; update TICKET-0237 or open one narrowly scoped follow-up only if its shipped sample cannot satisfy the approved detour/reward contract.
- [ ] Add a desktop playtest rubric with recorded prompts for: remembered discovery, understood archetype behavior, perceived combat fairness/readability, recognized Larrell consequence, Arkand connection, strongest sensory moment, and pacing drag; require timestamped notes for at least one full boot→camp owner/QA run before `project_vertical_slice_gate` can become `done`.
- [ ] Update `gameplay_landfall_experience_contract` in `act0_mvp_readiness.worldforge.json` to `done` only after the contract and downstream ticket amendments exist; run project validation after the tracker edit.

## Out of scope

- Implementing combat waves, graybox geometry, dialogue trees, cinematics, VFX/audio assets, camp runtime, or character-creation runtime in this ticket.
- Full-game balance, advanced archetypes, faction/morality thresholds, crafting, co-op, companion approval simulation, or Act 1 content.
- A named Act 0 boss; Luceran remains a theatrical collapse per the locked beat sheet.
- Reopening the A0-01…A0-09 story spine or deciding the future placeable-base direction in TICKET-0254.

## Dependencies

- Not blocked by engine implementation; this is a docs-first design gate using accepted decisions and the locked Act 0 beat sheet.
- Soft inputs: TICKET-0237 inventory/loot evidence and current starter-weapon behavior.
- Blocks the final acceptance shape for TICKET-0269, TICKET-0270, TICKET-0271, and TICKET-0273; informs combat readiness rows `combat_road_blockers` / `combat_drawbridge_hold` and `project_vertical_slice_gate`.
- TICKET-0272 may proceed in parallel because the isolated vision-space layout does not depend on combat/discovery tuning.

## Verification

- Doc review: confirm every acceptance item is represented in `context/design/act0-landfall-player-experience.md` with links to owning tickets/readiness rows.
- Cross-ticket audit: `rg -n "TICKET-0280|Landfall player-experience" context/planning/epics.md context/planning/tickets samples/open-world-rpg/assets/world-forge/act0_mvp_readiness.worldforge.json`.
- JSON parse plus `engine validate --project samples/open-world-rpg --json` after the readiness tracker status/content edit; no engine rebuild is required.
- Desktop QA is required only for the later full boot→camp playtest evidence; the design contract itself is docs-only.
- Before Status → `needs-approval`, fill **What changed** here and on the mirrored Notion page with the final contract path, amended ticket IDs, validation result, and unresolved playtest risks.

## What changed

Not implemented. This section must be completed before Status → `needs-approval`.

## Agent notes

Created from the owner’s 2026-08-10 eight-kinds-of-fun review. Keep this as one integration contract; do not split the taxonomy into separate feature tickets.
