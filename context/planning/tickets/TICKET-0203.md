# TICKET-0203: Swim mode + deep-water fatigue and damage

- Epic: EPIC-0016
- Status: proposed
- Agent: unassigned
- Priority: P2
- Notion: (mirror when pulled)

## Goal

Add character controller swim locomotion with shallow vs deep bands: deep water drains fatigue and applies health damage over time when the player must sustain swimming ([DEC-0039](../../decisions/index.md#DEC-0039-water-swim-and-hydrology-authoring)).

## Context links

- [`character-controller.md`](../../features/character-controller.md)
- [`water-hydrology.md`](../../features/water-hydrology.md)
- [`open-world-navigation.md`](../../features/open-world-navigation.md)
- [`navigation-grid.md`](../../features/navigation-grid.md)
- TICKET-0202 (water queries)

## Acceptance criteria

- [ ] Swim mode activates when capsule is in authored water
- [ ] Shallow band: wade or reduced swim cost (threshold documented in feature doc)
- [ ] Deep band: fatigue drain while swimming; exhaustion ΓåÆ damage over time
- [ ] Navigation grid marks underwater/deep samples unwalkable
- [ ] Foliage suppressed underwater (hook from TICKET-0202 queries)
- [ ] `character` suite + manual play-test in sample water

## Out of scope

Underwater combat polish; drowning cinematics; boat passenger state (TICKET-0200).

## Dependencies

Blocked by TICKET-0202. Soft: stamina HUD (EPIC-0007) for player feedback.

## Verification

Rebuild `engine`; play test swim + deep damage; `character` and `terrain`/nav suites.

## Agent notes

Exact depth thresholds and drain rates are tuning ΓÇö document chosen defaults in feature doc.

