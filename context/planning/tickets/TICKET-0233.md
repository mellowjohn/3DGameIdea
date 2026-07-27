# TICKET-0233: Three weapon chains + universal ability use

- Epic: EPIC-0018
- Status: proposed
- Agent: unassigned
- Priority: P3
- Notion: https://app.notion.com/p/3aad3efc569581cfa5d3ffd10d217450

## Goal

Playable Souls-lite action baseline: melee / ranged / magic weapon chains, with equipped weapon abilities usable by any archetype (soft efficiency from TICKET-0232), plus optional ability caveats (resource/cooldown/situational) without BDO-complexity animation trees.

## Context links

- `context/features/gearing-system.md`, `context/features/combat-volumes.md`
- DEC-0048
- EPIC-0011 (TICKET-0127/0128) — coordinate rather than duplicate

## Acceptance criteria

- [ ] Three chain kinds documented and driven from equipped weapon (or starter kit fallback)
- [ ] Off-lane archetype can activate the equipped weapon’s ability
- [ ] At least one ability caveat pattern implemented (e.g. stamina/cooldown) and documented
- [ ] Prefer shared animations + item VFX over per-weapon unique attack trees
- [ ] Named combat/related suite covers happy path + fail-closed missing weapon/ability refs
- [ ] Desktop QA labeled if GPU/play-test required

## Out of scope

- Tab-target primary combat
- Full ability tree / talent system
- Unique animation set per weapon
- Lock-on polish beyond what EPIC-0011 already scopes (share ownership)

## Dependencies

Blocked by / parallel: EPIC-0011 melee loop; soft affinity from TICKET-0232. Soft: TICKET-0231.

## Verification

Named suite + rebuild `engine`; desktop play-test steps when ready. Expand criteria before Status → ready.

## What changed

_(fill before needs-approval)_

## Agent notes

_(stub)_
