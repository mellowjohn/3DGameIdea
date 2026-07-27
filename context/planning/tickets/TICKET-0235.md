# TICKET-0235: Mining/materials stub (craft loop later)

- Epic: EPIC-0018
- Status: proposed
- Agent: unassigned
- Priority: P3
- Notion: https://app.notion.com/p/3aad3efc5695814da0daefe3c9c742ae

## Goal

Introduce mineable ore/crystal **materials** as inventory items and a gather stub so later crafting can hook in, without shipping a full craft UI/loop in this ticket.

## Context links

- `context/features/gearing-system.md`
- DEC-0048 (Runecaster item/rune-native magic; do not conflate with Cristallo order)
- TICKET-0232 / 0111

## Acceptance criteria

- [ ] Material item kind (ores/crystals) in schema with act-tier optional
- [ ] At least one sample gather interaction or debug grant path documented
- [ ] Validate fails closed on unknown material refs
- [ ] Docs state full crafting recipes/stations are follow-on

## Out of scope

- Full crafting stations, recipe UI, smelting graphs
- Dense world mining node art pass
- Cristallo crystal-guardian magic system

## Dependencies

Blocked by: item inventory foundation. Soft: TICKET-0231.

## Verification

Validate + suite when implemented. Expand before ready.

## What changed

_(fill before needs-approval)_

## Agent notes

_(stub)_
