# Factions of Tessera

- Status: developing story context
- Gap review: TICKET-0021 (2026-07-10) — status labels and gaps below; no new canon invented

## Canon status summary

| Faction / group | Status | What is established | What remains draft / open |
| --- | --- | --- | --- |
| Kingdom of Tessera | **draft** (arena vs faction choice open) | Dominant human occupying power; land fractured by chaotic forces; heroic medieval visual tone (*Gondor*/*Rohan* as references only) | Whether the kingdom is a playable faction choice or the political arena around others; precise geography vs setting name “Tessera” |
| Chaotic Imperium | **established** (existence + leader); **open** (Frangitur/Shroud links) | Evil coalition of living + ethereal forces; dark Roman visual language (references only); led by Luceran the Hollow | How Imperium authority ties to Frangitur, the Nefarium Shroud, and Luceran’s remaining agency |
| The Cristallo | **draft** | Religious/noble oligarchy; selective; higher social classes; refined architecture; hostile to woodland faction; can shift good/evil via player influence (proposal) | Theology, hierarchy, politics, relationship to Creo/Frangitur and the gods |
| Arrotrebae of the Rampant Wilds | **draft** | Woodland multi-tribe coalition; chieftains as local battlelords; no single overlord; adversarial with Cristallo; tonal barbarian/druid references | Collective decision rules when chieftains disagree; named tribes; player influence mechanics |
| Orc warbands | **draft** (structure); **open** (names) | Multiple warbands, not one faction; may support or oppose Imperium based on Nefarium exposure and hatred of Luceran; a great orc leader wore the Shroud before Luceran took it | Warband names/IDs; which warband held the Shroud and why; name of the slain great leader |
| Player influence (Cristallo / Arrotrebae) | **proposal** | Engine model: continuous standing + hostility transfer + lock-in fields ([DEC-0029](../decisions/index.md#dec-0029-continuous-faction-standing-with-hostility-transfer)). Both start neutral in story; morality remains a separate track. | Numeric thresholds, destruction/reform, morality binding — still open; do not invent numbers in World Forge seeds |

Labels follow [story index continuity](index.md): **established** = current story context; **draft** / **proposal** / **open** = needs owner review before canon.

## Kingdom of Tessera

The Kingdom of Tessera is the dominant occupying power in the region and is populated primarily by humans across many cities and settlements. Chaotic forces have divided the land and fractured those previously aligned with its principal good faction.

The kingdom's visual direction is heroic medieval fantasy, with *Gondor* and *Rohan* cited as tonal references rather than setting canon.

## Chaotic Imperium

The Chaotic Imperium gathers the evil forces that disrupt the player's journey. It combines living creatures with ethereal spirits and follows a dark Roman visual language. The Black Númenóreans and the varied demonic forces of *Dragon Age* are cited as references rather than setting canon.

Luceran the Hollow is the Imperium's current leader. The Imperium's connection to Frangitur, the Nefarium Shroud, and Luceran's remaining agency needs further definition.

## Neutral Factions

### The Cristallo

The Cristallo represents religious and noble power. It is selective, oligarchic, associated with higher social classes, and hostile toward the opposing woodland faction. It has a distinct and refined architectural identity.

The organization can develop toward good or evil through player influence. Its theology, relationship to the gods, and exact political structure remain open.

### Arrotrebae of the Rampant Wilds

The Arrotrebae are a woodland coalition composed of many tribes. Their chieftains represent different philosophical traditions and act as battlelords within their own domains. No chieftain rules all others; the faction operates through a democratic or council-based structure.

The faction draws tonal inspiration from fantasy barbarians and druids. Its relationship with the Cristallo is openly adversarial.

### Orc Warbands

Orcs are divided into multiple warbands rather than one unified faction. Individual warbands may support or oppose the Chaotic Imperium depending on their exposure to Nefarium and their hatred of the Imperium's current leader.

The great orc leader slain by Luceran remains unnamed.

## Player Influence

The Cristallo and Arrotrebae begin as neutral powers under the current proposal. The player's morality, relationships, and decisions can push either faction toward good or evil, support one against the other, or potentially destroy one.

## Gaps blocking World Forge schema (TICKET-0011) and mid-campaign beats

Concrete gaps — do not invent answers in schema or beat work until owner-resolved. Also filed under [Open Questions](#open-questions) and [`context/interviews/open-questions.md`](../interviews/open-questions.md).

### Luceran–Frangitur–Shroud links

- Is Imperium command granted by wearing the Shroud, by Frangitur’s will, by political myth, or a mix?
- How much agency does Luceran retain vs Shroud/Frangitur control?
- Reconcile battle-worn Shroud with throne-fused imagery ([nefarium-and-the-shroud.md](nefarium-and-the-shroud.md)).
- Competing “Shroud as prison” proposal must stay non-canon until chosen or rejected.

### Cristallo theology / politics

- Faith, hierarchy, and what it means for Creo/Frangitur to be a god “under” the Cristallo.
- Relationship to Kingdom of Tessera noble houses (Squire path) vs Cristallo as a separate power.
- Fields World Forge will eventually need: faith id, hierarchy ranks, settlement affiliation — blocked until theology/politics exist.

### Arrotrebae council rules

- How collective decisions work when chieftains disagree (vote, veto, ritual, war?).
- Named tribes / philosophical traditions for relationship-graph nodes.
- Standing and hostility rules vs Cristallo for mid-campaign beats.

### Orc warband naming

- Stable warband IDs/names for data modeling.
- Which warband possessed the Nefarium Shroud and why.
- Name of the great orc leader slain by Luceran.

### Player influence rules

- **Model (DEC-0029):** continuous per-faction standing; hostility fallout from rival/opposes edges; ranks + lock-in authoring; morality is a separate track (not in TICKET-0181).
- Morality thresholds and when archetype selection “binds” a neutral faction ([story-vision.md](story-vision.md)) — still open.
- Numeric Cristallo/Arrotrebae thresholds — still open; leave standing configs empty/draft until owner fills numbers.
- Conditions for destroying a faction vs reforming it — still open.
- Whether Kingdom of Tessera is a faction choice or the arena (affects schema root).

## Open Questions

- Define the Cristallo's faith, hierarchy, and relationship to Creo before his fall.
- Define how the Arrotrebae make collective decisions when chieftains disagree.
- Identify which orc warband possessed the Nefarium Shroud and why; name the slain great orc leader.
- Decide whether the Kingdom of Tessera is itself a faction choice or the political arena surrounding the other factions.
- Define Luceran–Frangitur–Shroud authority over the Chaotic Imperium without silently adopting the prison proposal.
- Specify player influence **numbers** (thresholds, destruction) for Cristallo and Arrotrebae; engine standing fields exist per [DEC-0029](../decisions/index.md#dec-0029-continuous-faction-standing-with-hostility-transfer).
