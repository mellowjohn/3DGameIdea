# TICKET-0234: Act-tier loot + obscure rare chase schema

- Epic: EPIC-0018
- Status: proposed
- Agent: unassigned
- Priority: P3
- Notion: https://app.notion.com/p/3aad3efc569581789b60cc130a219104

## Goal

Author act power bands and obscure rare chase items that can punch above band, without making them required for main story; support common + rare boss tables for optional farm replay.

## Context links

- `context/features/gearing-system.md`
- DEC-0048
- TICKET-0232 item model

## Acceptance criteria

- [ ] Item schema fields for `actTier` (or equivalent) and rare/chase tagging
- [ ] Validation rejects story-required flags on obscure rares if such a flag exists — or docs state rares are never quest-gated as mandatory
- [ ] Sample: ≥1 common act-tier item + ≥1 obscure rare that documents above-band intent
- [ ] Boss loot table shape supports common vs rare rolls (even if drop runtime is later)
- [ ] Format doc + project validate coverage

## Out of scope

- Full world loot placement pass
- Pity timers / bad-luck protection (unless owner asks)
- Guaranteeing first-playthrough discovery of rares

## Dependencies

Blocked by: TICKET-0232 (or 0111 item schema). Soft: TICKET-0231.

## Verification

`engine validate` + suite when implemented. Expand before ready.

## What changed

_(fill before needs-approval)_

## Agent notes

_(stub)_
