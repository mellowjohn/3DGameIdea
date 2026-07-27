# TICKET-0232: Item equip model + soft lane multipliers

- Epic: EPIC-0018
- Status: proposed
- Agent: unassigned
- Priority: P3
- Notion: https://app.notion.com/p/3aad3efc5695819cb212eacd66e82703

## Goal

Extend inventory/item data so any archetype can equip any gear, with lane efficiency multipliers favoring Ashfell Blade / Outrider / Runecaster without hard class locks.

## Context links

- `context/features/gearing-system.md`
- DEC-0048, DEC-0044
- TICKET-0111 (stats/items/inventory foundation)
- EPIC-0017 per-player inventory on co-op saves

## Acceptance criteria

- [ ] Item/equip schema documents slots + optional `laneMultipliers` (or equivalent) for the three starting lanes
- [ ] Equip validation allows off-lane weapons; no reject-on-mismatch
- [ ] Efficiency application is deterministic and covered by a named suite or headless validate path
- [ ] Invalid/malformed multiplier data fails closed with a stable error code
- [ ] Format/feature docs updated; ids from display names per `ids-from-display-names.mdc`

## Out of scope

- Full Terraria-sized item catalog authoring
- Combat hit resolution / weapon chains (TICKET-0233)
- Crafting UI; mount unlocks
- Hard “cannot equip” gates

## Dependencies

Blocked by: TICKET-0111 (or absorb into it if still proposed — prefer extend 0111 rather than fork). Soft: TICKET-0231 done.

## Verification

Rebuild `engine` / suite as named when implemented; `engine validate` on sample items. Expand criteria before Status → ready.

## What changed

_(fill before needs-approval)_

## Agent notes

_(stub)_
