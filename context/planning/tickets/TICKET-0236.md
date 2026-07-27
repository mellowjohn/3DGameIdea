# TICKET-0236: Thrator warlord SQ-13 + glad mount reward draft

- Epic: EPIC-0018
- Status: ready
- Agent: unassigned
- Priority: P3
- Notion: https://app.notion.com/p/3aad3efc5695819da8ffe520fbce5d3d

## Goal

Author durable story draft for **Thrator**, easter-egg orc warlord champion (Act 1 or 2): warband-tied Orgrimmar-flavored side quest culminating in a glad mount reward hook — no runtime mount required in this ticket.

## Context links

- `context/story/side-quest-catalog.md` — SQ-13
- `context/features/gearing-system.md`
- DEC-0048, DEC-0032 (FT remains; exotic mount soft-extends horses-only)
- `context/story/factions.md` — orc warbands
- Dom-owned warband naming still open (`dom-open-questions.md`)

## Acceptance criteria

- [ ] SQ-13 entry complete in side-quest catalog (starts, objectives, forks, rewards, act placement Act 1–2)
- [ ] Person/POI stubs named with draft status; warband id left open or linked to existing draft warband without inventing conflicting canon
- [ ] Reward documents **glad mount** unlock flag/hook + optional gear drops
- [ ] Story index / catalog summary updated
- [ ] Explicit: not Act 0 Landfall content; FT not replaced by mounts

## Out of scope

- Mount locomotion runtime / mesh
- Full World Forge quest JSON + dialogue trees (follow-on when quest tooling ready)
- Locking final warband display name without Dom

## Dependencies

Soft: TICKET-0231. Parallel OK with EPIC-0003 narrative. Runtime mount blocked by later tickets.

## Verification

Doc review of SQ-13 + index links. No rebuild.

## What changed

- Summary: Seeded SQ-13 Thrator draft (Act 1–2 easter egg, Orgrimmar-flavored warlord duel, glad mount reward) in the side-quest catalog; ticket remains open for Dom warband lock + deeper POI/dialogue.
- Files / surfaces touched: `context/story/side-quest-catalog.md`, `context/story/index.md`.
- Leftover risk / follow-ons: warband id Dom-owned; mount runtime later; WF quest JSON follow-on.

## Agent notes

Catalog seed landed with EPIC-0018 planning pass; expand when Dom picks warband.
