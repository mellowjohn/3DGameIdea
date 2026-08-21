# Character Creation

- Status: developing player-character context
- Decisions: [DEC-0009](../decisions/index.md#dec-0009-starting-archetype-character-creation), [DEC-0044](../decisions/index.md#dec-0044-starting-archetype-lane-orgs-and-rename)

## Overview

New games begin with character customization after the opening cutscene. The player defines a protagonist native to Tessera by choosing a **starting archetype** (base class) and customizing their character.

**Opening handoff (D-P0-17 / D-P0-17b / D-P0-17e):** prologue cinematic → this screen → **Calrenoth continuous-feeling pan** (≥30s; Landfall title on aerial wide) → quest **Landfall**.

## Class select copy (locked — D-P0-17c)

| Archetype | Bubble | Lore |
| --- | --- | --- |
| Ashfell Blade | House steel under Asher’s levy—close combat and duty beneath the Ashfell banner. | You were drafted into House Ashfell’s war line, where disciplined fighters and hardened brawlers serve Tessera through blood and obligation. Calrenoth was supposed to hold. You arrive as reinforcement—not yet a hero. |
| Outrider | A mobile Lodge skirmisher—strike from range and stay ahead of the line. | The Outrider Lodge scouts beyond Tessera’s armies, carrying warnings and harrying enemies before battle begins. Called to reinforce Calrenoth, you approach the siege with bow in hand and danger already at your heels. |
| Runecaster | A Guild combatant who prepares runes and drafts sigils under pressure. | The Runecaster Guild teaches power that must be inscribed, prepared, and triggered through practiced craft. Sent to Calrenoth as arcane support, you carry a small arsenal of runes into a fortress already beginning to fall. |

The protagonist is not fixed as a single job title. Under [DEC-0044](../decisions/index.md#dec-0044-starting-archetype-lane-orgs-and-rename), the three starting archetypes are **Ashfell Blade**, **Outrider**, and **Runecaster**.

## Shared Premise

Every starting archetype shares the same narrative premise unless a future decision says otherwise:

- An ordinary resident of Tessera drafted into the war against the Chaotic Imperium.
- Obligation that deepens through their **lane home org** (house / lodge / guild), then later **binds** into major faction politics ([DEC-0044](../decisions/index.md#dec-0044-starting-archetype-lane-orgs-and-rename) hybrid model).
- Fear and duty rather than conventional heroic confidence at the outset.
- The same morality, allegiance, companion, and leadership arcs described in [Story Vision and Campaign Structure](story-vision.md).

## Starting Archetypes

| id | Display | Lane | Home org | Sub-themes | Story |
| --- | --- | --- | --- | --- | --- |
| `ashfell_blade` | Ashfell Blade | Melee | House Ashfell | Fighter / Brawler | [ashfell-blade.md](ashfell-blade.md) |
| `outrider` | Outrider | Ranged | Outrider Lodge | Ranger / Forager / Nomad | [outrider.md](outrider.md) |
| `runecaster` | Runecaster | Magic (item/rune) | Runecaster Guild | Rune / Sigil | [runecaster.md](runecaster.md) |

**Magic boundary:** Runecasters cast by creating runes and drafting sigils (boon/relic/item-native). They are **not** the Cristallo crystal-guardian order (today’s White Lotus draft label — rename still open). That order remains a mostly **non-magic** custodian / political path.

Per-archetype kits, tutorial beats, companion dialogue variants, and full systems/comps definitions remain to be authored. The Ashfell Blade starter look reuses the former Squire kit reference in [Character Art Direction](../art/character-direction.md). Structured catalog authoring lives in World Forge → **Archetypes** ([`archetypes.worldforge.json`](../formats/world-forge-archetypes.md)).

## Lane org model (hybrid)

1. **Act 0** — shared draft / Calrenoth pressure; kit + mentor flavor differ by archetype.
2. **Early mid-game** — home org is the lane’s story spine (quests, ranks, NPCs).
3. **Later** — org standing / allegiance **binds** into Cristallo, Arrotrebae, Kingdom politics, etc.

## Advanced Archetypes

Morality thresholds, subclass milestones, and allegiance still unlock later archetypes as proposed in story context. Detailed advanced archetypes remain deferred until after the demo.

## Open Questions

- Define appearance customization fields (body, face, voice, etc.).
- Define whether pronouns are selectable and how dialogue addresses the protagonist.
- Confirm House Ashfell faces (D-P1-21b); ~~rename~~ **confirmed** 2026-07-29.
- Define Outrider Lodge and Runecaster Guild faces, ranks, and first quests.
- Define combat **systems** and party/build **comps** per lane (interview backlog).
- Define whether each archetype uses distinct opening/tutorial combat lessons or a shared tutorial with archetype-specific loadouts.
- Reconcile the Wild God revival opening with the drafted-to-Calrenoth premise across all archetypes.
- **Archetype quest unlocks** ([DEC-0051](../decisions/index.md#dec-0051-no-xp-power-progression-and-quest-ux)): home-org lines optional for main story, needed for full lane power; per-quest gear and/or ability (act-scaled); separate Faction tab for polity standing.
- **Power progression:** no traditional XP/level — gear + chapter bosses + archetype/story unlocks ([gearing-system.md](../features/gearing-system.md#power-progression-dec-0051)).
