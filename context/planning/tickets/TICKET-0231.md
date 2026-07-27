# TICKET-0231: Gearing design + DEC-0048 recorded

- Epic: EPIC-0018
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/3aad3efc569581c5bdffcef00d9e1739

## Goal

Lock the Terraria-shaped gearing + simple action combat design in durable context so later inventory/combat tickets implement without re-litigating Dom/John session choices.

## Context links

- `context/features/gearing-system.md`
- `context/decisions/index.md` — DEC-0048
- `context/planning/epics.md` — EPIC-0018
- Provenance: Dom + John transcripts / chat 2026-07-27

## Acceptance criteria

- [x] Feature note `context/features/gearing-system.md` covers soft affinity, combat baseline, act tiers / obscure rares, acquisition loops, Thrator, sequencing
- [x] DEC-0048 accepted in `context/decisions/index.md`
- [x] EPIC-0018 + child ticket rows exist in `epics.md`
- [x] `context/features/index.md` lists gearing system
- [x] Canon spelling: Ledgeport (not Ledgerport); Thrator (not Thraador)

## Out of scope

- Runtime inventory, combat, loot, craft, or mount implementation
- Full Thrator quest dialogue / world placement (TICKET-0236)
- Amending DEC-0032 FT policy beyond soft-extend note for exotic mounts

## Dependencies

Blocks: TICKET-0232–0236 design clarity. Parallel OK with Act 0 MVP tickets.

## Verification

Doc review: open DEC-0048 + gearing-system.md + EPIC-0018 table; confirm indexes link. No rebuild.

## What changed

- Summary: Recorded owner/Dom gearing decisions as DEC-0048 and a feature note; opened EPIC-0018 with six child tickets.
- Files / surfaces touched: `context/decisions/index.md`, `context/features/gearing-system.md`, `context/features/index.md`, `context/planning/epics.md`, ticket stubs, side-quest SQ-13 seed.
- Schema / API / format deltas: none (design only).
- Seed / sample data: SQ-13 Thrator draft in side-quest catalog.
- Tests / verification evidence: doc-only review.
- Decisions & tradeoffs: Soft affinity over hard locks; obscure OP rares allowed but not story-required; Thrator Act 1–2; combat Souls-lite with ability caveats.
- Leftover risk / follow-ons: Ability caveats list still open; warband id for Thrator unset; glad mount runtime deferred.

## Agent notes

Completed in planning pass 2026-07-27 from transcript + Dom chat answers.
