# Campaign Beat Sheet

- Status: developing story context — **Act 0 Landfall spine MVP-locked** (owner review 2026-08-05)
- Ticket: [TICKET-0020](../planning/tickets/TICKET-0020.md)
- Decisions: [DEC-0001](../decisions/index.md#dec-0001-product-and-platform-target), [DEC-0021](../decisions/index.md#dec-0021-soft-gates-with-rare-optional-instances), [DEC-0032](../decisions/index.md#dec-0032-open-world-travel-discovery-map-and-dual-soft-gates), [DEC-0033](../decisions/index.md#dec-0033-anywhere-player-camp-as-editable-instance-dao-style)
- Sources: [story-vision.md](story-vision.md), [prologue-and-opening.md](prologue-and-opening.md), Twine Act 0 [`sources/wrathful-conquest-act0.twee`](sources/wrathful-conquest-act0.twee) (IFID `18D3E14D-3321-4EC0-B2E7-197EC99657D7`)

Named beats only — not full scripts or dialogue trees. Labels: **established** (aligned with current story context), **draft** (working, needs owner review), **open** (unresolved).

## World gating model (DEC-0021)

| Mode | Use | Loading |
| --- | --- | --- |
| Seamless open world (default) | Overland Tessera, towns, roads, soft-gated regions | Streaming / soft gates — no chapter load screens |
| Rare optional instances | Dungeons, set-piece arenas, vision/dream spaces, **player camp** when isolation or density needs it | Prefer seamless handoff or short transition; avoid frequent full-world reloads |

Story “acts” are **narrative arcs**, not separate loaded chapters. Progress uses quest/story flags, region pressure, and soft gates. Instances are tools, not the campaign spine.

## Act overview

| Act | Arc | Gate style | Status |
| --- | --- | --- | --- |
| Act 0 — **Landfall** | Prologue → Calrenoth siege tutorial → Creotar vision | Soft-gated tutorial corridor in open world; optional instance for Realm of Darkness | **established** (MVP spine locked 2026-08-05); title **final** (D-P0-15); geography confirmed western peninsula tip ([official-world-map.md](official-world-map.md#draft-faction-theaters-2026-07-20)); exact coords still **D-P2-08** |
| Act 1 | Retreat aftermath → **Ledgeport** hub → dual-path intro (Thalassar ↔ Cristallo) + succession (A1-05) | Soft gates open into 4×4 km world | **draft** |
| Act 2 | Faction pull (Cristallo / Arrotrebae) + mid-war stakes | Soft gates + optional dungeon instances | **draft** / mid beats **open** (faction gaps) |
| Act 3 | Approach Luceran / Shroud crisis | Soft gates; possible set-piece instance for climax approach | **draft** |
| Act 4 | Endings by morality / allegiance | Outcome branches; may reuse world state | **open** |

### Act 0 geography (MVP-locked theater; coords open)

Status: **established** theater + handoff (owner MVP lock 2026-08-05). Exact world coords remain **D-P2-08** (level design). Do not invent settlement names; **The Thalassar** / **The Underflow** locked.

- **Calrenoth** sits on the **western peninsula tip** (confirmed) as a Tessera-built frontier landing facing Imperium pressure from the south — outside the kingdom’s main **western** core. Two approaches: **landlocked** player entrance + **moat-scale drawbridge** to another land spur.
- Prefer the Calrenoth siege **in the open world** (backdrop + approach to meet Arkand); rare instances only when needed ([DEC-0021](../decisions/index.md#dec-0021-soft-gates-with-rare-optional-instances)).
- Nearby encounter concepts: **The Thalassar** (**Muirthalia**) and **The Underflow** (**Grakk-Maren**) — same watershed cult (false **The Sea of Whispers** as Luceran lieutenant plant) — [factions.md](factions.md#calrenoth-corridor-liturgy-names).
- After the fall, survivors flee toward nearby **Ledgeport** (`ledgeport`) — neutral market free-town / trade port (Act 1 hub / focus region). Named **O’hlundian evergreens wake** deprecated as a competing Act 1 geography anchor (D-P0-05); **DA campsite post–Act 0** locked (D-P0-10). Camp *placement model* (instance vs placeable base) remains open as **D-P1-23** / TICKET-0254 — does not change A0-09 tutorial events.
- **Cristallo** theater: central island. Act 1 dual-path intro can reach Cristallo after/alongside Thalassar path (D-P1-08).

---

## Act 0 — Landfall (Fall of Calrenoth)

**Title:** **Landfall** — **final** marketing name (Dom + owner, 2026-07-20; confirmed D-P0-15, 2026-07-29) — tutorial establishing the Chaotic Imperium, Luceran the Hollow, and the Kingdom of Tessera as a force of its own.

**Premise (established for MVP):** Player is deployed into the Imperium siege of **Calrenoth** (aligns with [ashfell-blade.md](ashfell-blade.md) / King Asher’s war levy; same premise for Outrider and Runecaster). Alternate Wild God revival opening remains **open** chronology — **not** the Act 0 spine.

**Boss note (locked 2026-08-05):** Act 0 has **no named chapter boss fight**. Luceran at A0-07 is theatrical dread/collapse. First player-visible main-story boss is **Pneumyra** (A1-05). Any Act 0 loot-band advance comes from completing Landfall / tutorial milestones, not a boss kill ([DEC-0051](../decisions/index.md#dec-0051-no-xp-power-progression-and-quest-ux)).

### Beat A0-01 — Prologue throne whisper

- **Status:** established (MVP lock 2026-08-05; matches [prologue-and-opening.md](prologue-and-opening.md) + Twine)
- Frangitur whispers to Luceran on the throne; taunts control of the Nefarium Shroud; addresses adventurers: Tessera must be ripped apart.
- **Locus (D-P0-17):** Frangitur is **inside the Nefarium Shroud**; presentation is **narration** (cinematic VO), not an interactive dialogue tree.
- **VO craft (D-P0-17b):** syllables + intonation on the **already-provided** Frangitur dialogue — not blocked on a prose rewrite.
- Stage beats: throne zoom-out → blood glass → white silhouette / Shroud → flame → **character creation** (then Calrenoth cine — see opening flow).
- **Links:** [frangitur-the-great-evil.md](frangitur-the-great-evil.md), [nefarium-and-the-shroud.md](nefarium-and-the-shroud.md)

### Beat A0-02 — Character creation

- **Status:** established ([DEC-0009](../decisions/index.md#dec-0009-starting-archetype-character-creation)); UI copy shape **D-P0-17 / D-P0-17b**
- Customize protagonist; choose Ashfell Blade / Outrider / Runecaster; difficulty ([DEC-0044](../decisions/index.md#dec-0044-starting-archetype-lane-orgs-and-rename)).
- **UI:** one short **description bubble** + **lore background** per archetype (final strings → D-P0-17c).
- **Handoff:** after Confirm → **Calrenoth cinematic** (continuous-feeling pan; shot list D-P0-17d + camera locks **D-P0-17e**: ≥30s, Landfall title on aerial wide, road+front gate, discover Arkand after control) → quest **Landfall** (`mq_act0_calrenoth`).
- **Links:** [character-creation.md](character-creation.md)

### Beat A0-03 — Meet Arkand on the approach

- **Status:** established (MVP lock 2026-08-05)
- After Calrenoth cinematic + quest start: siege backdrop; **player discovers** Arkand trapped under a wheelbarrow (cine settles short of armored-hand reveal — D-P0-17e); rescue; Arkand introduces himself (Knight of Tessera, King’s Guard).
- Dialogue: **keep Twine / `dlg_act0_meet_arkand` wording** (D-P0-17d) — no rewrite until Dom supplies a newer pass. Branches (eager / rude / keep-focused) stay the working shape.
- **Links:** [companions.md](companions.md) (Arkand)

### Beat A0-04 — Approach Calrenoth under fire

- **Status:** established (MVP lock 2026-08-05)
- Fight through Imperium blockers on the road; enter fortress under catapults, fireballs, arrows.
- Guards halt party; Arkand vouches (backup for Commander Grenge).
- Overhear crisis: Imperium assembled without warning; Rinos fell with no signal.

### Beat A0-05 — Commander Grenge and the thin “battalion”

- **Status:** established (MVP lock 2026-08-05)
- Meet **Commander Grenge** (green shroud over plate); berating scout **Damius** over Rinos.
- Grenge expected a battalion from **King Asher**; player is alone or claims ambush / hubris / abandonment — dialogue branches.
- Mission: help retreat; secure rear drawbridge with **Sergeant Larrell**; Arkand accompanies.
- **Faction touch:** Chaotic Imperium siege; Asher’s weak support of the front ([factions.md](factions.md) — do not expand here).
- **Geography:** Calrenoth as Tessera’s southern frontier landing — reinforcing from the western kingdom core is hard ([official-world-map.md](official-world-map.md#draft-faction-theaters-2026-07-20)).

### Beat A0-06 — Drawbridge defense

- **Status:** established (MVP lock 2026-08-05)
- Run lower castle through fire and ash; reach Larrell; chains jammed; hold waves while drawbridge lowers.
- Signal fire; Grenge’s remaining forces evacuate.

### Beat A0-07 — Larrell choice and Luceran’s shadow

- **Status:** established (MVP lock 2026-08-05)
- Choice pressure: save Larrell vs flee as Imperium overruns the rear.
- **Cast continuity (D-P0-12 / D-P0-12b):** **Grenge / Larrell / Damius** **survive by default** and may reappear in Act 1 after evacuating **over the wall**. Optional hostage = **Sergeant Larrell** if the player does **not** save her at this beat (consequential flag + later payoff). Prefer consequential flags over silent disappearances.
- Black fog; dread; pale dark rider (Luceran the Hollow); player collapses — **theatrical**, not a boss fight the player wins.
- **Gate note:** Prefer soft-gated siege space **in the open world**; short set-piece instance only if density needs it — one contiguous Calrenoth experience with minimal loads ([DEC-0021](../decisions/index.md#dec-0021-soft-gates-with-rare-optional-instances)).

### Beat A0-08 — Realm of Darkness / Creotar

- **Status:** established (MVP lock 2026-08-05); Creotar identity locked 2026-07-20; honesty lock D-P2-14
- Vision space: darkness; light-being **Creotar** (**Creo** short form) warns that Luceran the Hollow / Nefarium Shroud must be opposed.
- Creotar self-describes as god/gifter of **knowledge and creation**; names **Claritas** (D-P2-02b) as the crystal that can oppose the Shroud (staff-borne by Cristallo’s leader; White Lotus guardians).
- **Writer truth vs in-vision framing (locked):** opposing Luceran / Shroud misuse is **real**. Lines that urge “free Luceran from the Shroud,” “Shroud is Luceran’s prison,” or “break the Shroud to free Creotar” are **manipulated / ironic** (Frangitur/Luceran framing) — dramatic irony vs Frangitur’s A0-01 “rip Tessera apart” prologue ([prologue-and-opening.md](prologue-and-opening.md)). Playable copy may keep Creotar’s sincere tone; do not treat those free-Luceran / prison-for-Luceran claims as author-endorsed fact.
- **Continuity:** Creotar = Creo; Frangitur = fallen/perverted form of the same being ([frangitur-the-great-evil.md](frangitur-the-great-evil.md)). Shroud **contains Frangitur** and **binds** Imperium chaos; breaking it **releases Frangitur** (D-P2-14).
- Twine names **Grul’thaz the Black Howl** (Shadowpaw) as prior Shroud bearer — **locked:** **The Black Howl** warband held the Shroud in the first war before Luceran usurped it.
- Asher is Luceran’s **fragile half-brother** on the throne — **established** (D-P0-13).
- Fill empty Twine crystal-location stubs with **Claritas**; replace “Tutorial Completion / next level” with A0-09 camp handoff.
- **Presentation:** struggling tesseract/mirage lean locked (D-P2-18); numeric VFX polish remains **D-P2-17**.
- **Gate note:** Strong candidate for a rare **vision instance** (or seamless overlay) — not a full world reload loop.

### Beat A0-09 — Tutorial completion → Act 1 handoff

- **Status:** established (MVP lock 2026-08-05; D-P0-10 / D-P1-19)
- Vision ends; player is placed **outside Calrenoth events** and wakes at a **Dragon Age–style campsite** (companion-relationship + camp-storage tutorial; **rest for HP**; **not** survival feeding).
- Camp is **travel-only** — no combat escape ([DEC-0033](../decisions/index.md#dec-0033-anywhere-player-camp-as-editable-instance-dao-style) until/unless D-P1-23 amends). Player **travels to camp**, then **travels to map destinations** (including **Ledgeport**) from camp.
- **Tutorial guide:** **Arkand** walks storage chest, rest, and travel in lore-friendly lines. Companions with pending story / profile beats show a **talk flag**; player chooses who to speak to (no forced first companion beyond Arkand’s tutorial).
- **No** non-companion camp NPC in v1 (keep camp light). Optional later camp-attack beat deferred.
- Replaces the named **O’hlundian evergreens wake** as the Act 0→1 geography anchor (D-P0-05); evergreens may remain as **biome flavor** on the retreat path only.
- **Calrenoth on the map:** remains as a **ruined, impacted** seamless location the player can revisit (destroyed/attacked state), not a wiped instance.

---

## Act 1 — First hub and open world

### Beat A1-01 — Survivor camp / retreat

- **Status:** draft ([DEC-0033](../decisions/index.md#dec-0033-anywhere-player-camp-as-editable-instance-dao-style)); **camp tutorial begins post–Act 0** (D-P0-10 / D-P1-19, 2026-07-29)
- After campsite tutorial (**Arkand**-guided storage / rest / travel; companion talk flags), survivor path toward **Ledgeport**; rejoin Arkand (and possibly Larrell if saved / not hostage, or Grenge remnants). Establish immediate survival goal and Creotar’s warning as personal quest seed.
- **Camp loop:** talk to companions (story + relationship beats), optional storage; **rest for HP**; leave camp to travel. No non-companion camp NPC in v1. After unlock, player may camp from (nearly) anywhere on the overland map via the same persistent camp instance (DAO-style) — **not** a combat escape hatch.
- Player then finds the road toward **Ledgeport** on foot from camp travel.
- **Geography (draft):** Act 1 focus = **Ledgeport**; named evergreen wake deprecated as competing geography.

### Beat A1-02 — First hub (Ledgeport)

- **Status:** draft ([story-vision.md](story-vision.md) opening flow); **Act 1 major hub** — name **confirmed**
- **Ledgeport** (`ledgeport`) — **neutral** coastal free town ruled more by market than feudal crown: mayor possible; merchant guild; undermarket / black market; visiting traders from Arrotrebae and Cristallo. Side quests derive from market / spy / money pressure.
- Unlock services, companion camp view concepts, tavern/carriage post (FT discovery), and the wider campaign structure.
- Soft-open surrounding regions of the 4×4 km world; Act 1 dual-path intro (Thalassar ↔ Cristallo) branches from here.
- **Ferry (draft lock 2026-07-22):** route toward Cristallo central island — cross channel to **Porto Lucente** (`porto_lucente` / `porto_lucente_dock`) in the U-bay.

### Beat A1-03 — Open-world unlock

- **Status:** draft
- Main path stops being a corridor: player may explore, take side content ([TICKET-0022](../planning/tickets/TICKET-0022.md)), and pursue crystal / Shroud leads.
- Soft gates may still lock high-chaos or late story regions.

### Beat A1-04 — Meet Vanessa (timing TBD)

- **Status:** draft / **open** exact beat
- Introduce Vanessa as pragmatic counterweight ([companions.md](companions.md)). Exact hub vs road encounter TBD.

### Beat A1-05 — Coastal Arrotrebae succession (assassination plot)

- **Status:** draft ([factions.md](factions.md#design-session-2026-07-20-p0-questions-pass--draft))
- Early Act 1 political thread: the leader of **The Thalassar** is assassinated — plot led by **Pneumyra** (Sea of Whispers lieutenant; Act 1 boss body).
- Succession: candidates run **trials** to attune to **Anál Muir**; player champions a **good Thalassar candidate**; **The Underflow** pushes an underhanded usurper path. Powers are literal leader-only (optional temporary quest boon).
- Teaches how Arrotrebae tribes work as part of Act 1 dual-path intro (light standing); keep major Cristallo vs Arrotrebae allegiance for later.
- Contact geography: **Ledgeport** — not owned by The Thalassar or Cristallo.
- **Do not invent** leader/successor person names (TBD/dynamic) or quest id until authored. Transcript “Aerotropy/Atrobia” = Arrotrebae mishearing.

---

## Act 2 — Faction war and allegiance

Mid-campaign beats stay **draft** where faction gaps from [TICKET-0021](../planning/tickets/TICKET-0021.md) block detail.

### Beat A2-01 — Cristallo contact

- **Status:** draft — theology/politics **partial lock** (2026-07-22); crystal **Claritas** (2026-07-29)
- Player engages Cristallo-aligned houses; morality and reputation begin to matter.
- **Draft spine:** Old/New Testament faith split; church/state bipartisan conflict; **White Lotus** guardians of **Claritas** (Creator crystal on the **leader’s staff**; cleanses Nefarium); Luceran-puppet overzealous sub-faction; Act 2 duty to wield relic against Shroud effects after navigating politics.
- **Draft (2026-07-29):** Act 1 path choice (Arrotrebae vs Cristallo focus) can leave the neglected theater more open to **Imperium corruption** by Act 2 — soft consequence, not a hard lock.

### Beat A2-02 — Arrotrebae / tribal contact

- **Status:** draft / **open** council rules
- Parallel pull toward tribal path; Vanessa arcs may fork here ([companions.md](companions.md)). Coastal tribe from A1-05 may already be known — deepen into umbrella council / same-god conflicts rather than re-introducing from zero.

### Beat A2-03 — Imperium pressure on the map

- **Status:** draft
- Soft-gated Imperium advances, corrupted ground, optional dungeon instances for Nefarium sites.

### Beat A2-04 — Crystal / Shroud lead advances

- **Status:** draft — **Claritas** named (D-P2-02b); court map pin still open with D-P2-08
- Pursue Creotar’s crystal **Claritas** (or competing interpretations of “destroy the Shroud”) — dramatic irony: player may think they free **Creotar** while Luceran seeks to free **Frangitur** ([prologue-and-opening.md](prologue-and-opening.md)).
- **Claritas** **cleanses Nefarium** on contact (inverse of Shroud); set in a staff borne by **Cristallo’s leader**; **White Lotus** order guards the relic / path.

---

## Act 3 — Usurper crisis

### Beat A3-01 — Approach Luceran’s seat of power

- **Status:** draft
- Soft gates tighten around throne / Shroud stronghold regions.

### Beat A3-02 — Companion loyalty stress

- **Status:** draft / **open** break points
- Arkand / Vanessa loyalty tested by good–evil choices ([companions.md](companions.md)).

### Beat A3-03 — Confront Luceran / Shroud set piece

- **Status:** draft
- Climax confrontation; optional instance for the throne/Shroud encounter if density requires it.
- **Draft lock (2026-07-22):** Luceran holds command while **Frangitur remains trapped in the Shroud**; player may have been manipulated to “free Creotar” — truth reveals **Frangitur** late.

---

## Act 4 — Endings (open)

### Beat A4-01 — Morality / faction lock-in resolution

- **Status:** **open** (thresholds undefined in [story-vision.md](story-vision.md))

### Beat A4-02 — Ending branches

- **Status:** **open** — soft sketch 2026-07-29 (D-P2-13)
- Oppose evil, exploit it, or become greater evil; Cristallo/Arrotrebae preserved, reformed, or dismantled.
- **Shroud endgame (draft):** after Frangitur falls, Chaotic Imperium has **no masters** → choose to **destroy** the chaos for good **or** forge/wield a Shroud to **control** it (control path = corruption / become evil; Luceran’s first-war burden is the cautionary model). **Claritas** is a candidate tool on the destroy path.

---

## Named cast introduced by Act 0

| Name | Role | Status |
| --- | --- | --- |
| Arkand | Companion; King’s Guard knight; camp tutorial guide | **established** (MVP lock 2026-08-05) |
| Commander Grenge | Calrenoth commander; default Act 1 reappearance | **established** cast role (survive-by-default D-P0-12) |
| Sergeant Larrell | Rear drawbridge NCO; optional **hostage** if not saved (D-P0-12b); else default Act 1 reappearance | **established** cast role |
| Damius | Scout blamed for Rinos; default Act 1 reappearance | **established** cast role |
| King Asher | Current King of Tessera; Luceran’s half-brother | **established** kinship (D-P0-13); brittle support still draft flavor |
| Creotar | Vision guide; knowledge god (Creo = short form; Frangitur = fallen form) | **established** identity + A0-08 role |
| Grul’thaz the Black Howl | Prior Shroud bearer; **The Black Howl** war-chief (Shadowpaw) | **established** first-war bearer (D-P2-04); later fate open D-P2-11 |
| Drul’gath | **The Underflow** war-chief; Act 0 corridor / A1 usurper-path face | **established** display (optional Act 0 glimpse; not a required A0-01…09 stage beat) |
| Luceran the Hollow | Dark rider / Usurper; A0-07 theatrical collapse | **established** |

## Open questions (beat sheet)

**Act 0 leftover (non-spine):** world coords (**D-P2-08**, level design); House Ashfell / Lodge / Guild **faces** (**D-P1-21b** / **D-P1-22**); camp *placement* model (**D-P1-23**). Landfall spine A0-01…A0-09 **MVP-locked** 2026-08-05 — see [`../design/dom-answered-questions.md`](../design/dom-answered-questions.md).

- Exact Calrenoth / **Ledgeport** / neighbor world coords — theaters in [official-world-map.md](official-world-map.md#draft-faction-theaters-2026-07-20); Dom **D-P2-08** (deferred to level design).
- ~~Exact placement of O’hlundian evergreens wake vs Ledgeport~~ — evergreen wake deprecated; **DA campsite post–Act 0** locked (D-P0-10, 2026-07-22).
- ~~Whether Calrenoth stays permanently ruined on the seamless map after Act 0~~ — **resolved** DEC-0032.
- ~~Creotar identity vs Creo/Frangitur~~ — **resolved** 2026-07-20; ~~“destroy the Shroud” honesty~~ — **partially true** (D-P2-14, 2026-07-29); writer vs vision framing locked in A0-08 (2026-08-05).
- ~~Crystal **name**~~ — **Claritas** (D-P2-02b); court map pin still open with D-P2-08; **White Lotus** guardians + cleanse behavior locked.
- ~~Wild God revival chronology vs Calrenoth Act 0~~ — Calrenoth / **Landfall** is the default spine; title **final** (D-P0-15).
- ~~Act 0 named chapter boss~~ — **none**; Luceran A0-07 theatrical only; first main-story boss = Pneumyra (A1-05) — locked 2026-08-05.
- Exact Vanessa introduction beat — Dom **D-P1-14** (Act 1 block).
- A1-05 quest id / standing numbers; **Sea of Whispers** reveal timing (lieutenant **Pneumyra** + epithet **She Who Sings the Undertow** locked; boss body fight locked; masks + **Anál Muir** locked) — [factions.md](factions.md#the-sea-of-whispers); Dom **D-P1-15** / **D-P1-16**.
- ~~**Ledgeport** ferry wiring to Cristallo~~ — **yes** to **Porto Lucente** (`porto_lucente`) U-bay (2026-07-22); world coords still open (D-P2-08).
- Morality thresholds and ending matrix (Act 4) — soft Shroud destroy-vs-control sketch 2026-07-29; harden under D-P2-13.
- Twine source covers **Act 0 only**; Acts 1–4 are planning beats, not yet authored in Twine. Act 0 playable copy polish remains checklist rows `story_beats_a0_*`.

Also tracked in [`context/interviews/open-questions.md`](../interviews/open-questions.md).
