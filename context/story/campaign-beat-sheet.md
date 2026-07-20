# Campaign Beat Sheet

- Status: developing story context
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
| Act 0 — **Landfall** | Prologue → Calrenoth siege tutorial → Creotar vision | Soft-gated tutorial corridor in open world; optional instance for Realm of Darkness | **draft** (Twine-backed); geography confirmed western peninsula tip ([official-world-map.md](official-world-map.md#draft-faction-theaters-2026-07-20)) |
| Act 1 | Retreat aftermath → **Ledgeport** hub → dual-path intro (Thalassar ↔ Cristallo) + succession (A1-05) | Soft gates open into 4×4 km world | **draft** |
| Act 2 | Faction pull (Cristallo / Arrotrebae) + mid-war stakes | Soft gates + optional dungeon instances | **draft** / mid beats **open** (faction gaps) |
| Act 3 | Approach Luceran / Shroud crisis | Soft gates; possible set-piece instance for climax approach | **draft** |
| Act 4 | Endings by morality / allegiance | Outcome branches; may reuse world state | **open** |

### Draft Act 0 geography (2026-07-20)

Status: **draft** — Dom + owner sessions. Do not invent settlement names; **The Thalassar** / **The Underflow** locked.

- **Calrenoth** sits on the **western peninsula tip** (confirmed) as a Tessera-built frontier landing facing Imperium pressure from the south — outside the kingdom’s main **western** core. Two approaches: **landlocked** player entrance + **moat-scale drawbridge** to another land spur.
- Prefer the Calrenoth siege **in the open world** (backdrop + approach to meet Arkand); rare instances only when needed ([DEC-0021](../decisions/index.md#dec-0021-soft-gates-with-rare-optional-instances)).
- Nearby encounter concepts: **The Thalassar** (**Muirthalia**) and **The Underflow** (**Grakk-Maren**) — same watershed cult (false **The Sea of Whispers** as Luceran lieutenant plant) — [factions.md](factions.md#calrenoth-corridor-liturgy-names).
- After the fall, survivors flee toward nearby **Ledgeport** (`ledgeport`) — neutral market free-town / trade port (Act 1 hub / focus region). Named **O’hlundian evergreens wake** deprecated as a competing Act 1 geography anchor (D-P0-05); DEC-0032 wake/camp reconcile still open (Dom D-P0-10).
- **Cristallo** theater: central island. Act 1 dual-path intro can reach Cristallo after/alongside Thalassar path (D-P1-08).

---

## Act 0 — Landfall (Fall of Calrenoth)

**Title:** **Landfall** (Dom + owner, 2026-07-20) — tutorial establishing the Chaotic Imperium, Luceran the Hollow, and the Kingdom of Tessera as a force of its own.

**Premise (draft):** Player is deployed into the Imperium siege of **Calrenoth** (aligns with [the-squire.md](the-squire.md) draft / King Asher’s war levy). Alternate Wild God revival opening remains **open** chronology — not used as Act 0 spine here.

### Beat A0-01 — Prologue throne whisper

- **Status:** draft (matches [prologue-and-opening.md](prologue-and-opening.md) + Twine)
- Frangitur whispers to Luceran on the throne; taunts control of the Nefarium Shroud; addresses adventurers: Tessera must be ripped apart.
- Stage beats: throne zoom-out → blood glass → white silhouette / Shroud → flame → character creation.
- **Links:** [frangitur-the-great-evil.md](frangitur-the-great-evil.md), [nefarium-and-the-shroud.md](nefarium-and-the-shroud.md)

### Beat A0-02 — Character creation

- **Status:** established premise ([DEC-0009](../decisions/index.md#dec-0009-starting-archetype-character-creation))
- Customize protagonist; choose Squire / Archer / Acolyte; difficulty.
- **Links:** [character-creation.md](character-creation.md)

### Beat A0-03 — Meet Arkand on the approach

- **Status:** draft (Twine)
- Siege backdrop; player finds Arkand trapped under a wheelbarrow; rescue; Arkand introduces himself (Knight of Tessera, King’s Guard).
- Dialogue branches color Arkand’s first impression (eager / rude / focused on the keep).
- **Links:** [companions.md](companions.md) (Arkand)

### Beat A0-04 — Approach Calrenoth under fire

- **Status:** draft (Twine)
- Fight through Imperium blockers on the road; enter fortress under catapults, fireballs, arrows.
- Guards halt party; Arkand vouches (backup for Commander Grenge).
- Overhear crisis: Imperium assembled without warning; Rinos fell with no signal.

### Beat A0-05 — Commander Grenge and the thin “battalion”

- **Status:** draft (Twine)
- Meet **Commander Grenge** (green shroud over plate); berating scout **Damius** over Rinos.
- Grenge expected a battalion from **King Asher**; player is alone or claims ambush / hubris / abandonment — dialogue branches.
- Mission: help retreat; secure rear drawbridge with **Sergeant Larrell**; Arkand accompanies.
- **Faction touch:** Chaotic Imperium siege; Asher’s weak support of the front ([factions.md](factions.md) — do not expand here).
- **Geography (draft):** Calrenoth as Tessera’s southern frontier landing — reinforcing from the western kingdom core is hard ([official-world-map.md](official-world-map.md#draft-faction-theaters-2026-07-20)).

### Beat A0-06 — Drawbridge defense

- **Status:** draft (Twine)
- Run lower castle through fire and ash; reach Larrell; chains jammed; hold waves while drawbridge lowers.
- Signal fire; Grenge’s remaining forces evacuate.

### Beat A0-07 — Larrell choice and Luceran’s shadow

- **Status:** draft (Twine)
- Choice pressure: save Larrell vs flee as Imperium overruns the rear.
- Black fog; dread; pale dark rider (Luceran the Hollow); player collapses.
- **Gate note:** Prefer soft-gated siege space **in the open world**; short set-piece instance only if density needs it — one contiguous Calrenoth experience with minimal loads ([DEC-0021](../decisions/index.md#dec-0021-soft-gates-with-rare-optional-instances)).

### Beat A0-08 — Realm of Darkness / Creotar

- **Status:** draft (Twine); Creotar identity **resolved** (2026-07-20)
- Vision space: darkness; light-being **Creotar** (**Creo** short form) warns that Luceran the Hollow / Nefarium Shroud must be opposed.
- Creotar self-describes as god/gifter of **knowledge and creation**; claims a **crystal** (stolen from him after world’s creation) can tear/destroy the Shroud; urges freeing Luceran from the Shroud to free Tessera.
- Alternate branch stresses Shroud as a **prison** for Luceran and that Imperium chaos followed Shroud falling into wrong hands — conflicts with Frangitur’s “rip Tessera apart” prologue (dramatic irony; see [prologue-and-opening.md](prologue-and-opening.md)).
- Twine names **Grul’thaz the Black Howl** (Shadowpaw) as prior Shroud bearer — **draft** pending faction naming review.
- Asher is Luceran’s **fragile half-brother** on the throne (Twine) — **draft**.
- Twine gaps: crystal-location dialogue passages are empty stubs; “Tutorial Completion” only says continue to next level.
- **Continuity:** Creotar = Creo; Frangitur = fallen/perverted form of the same being ([frangitur-the-great-evil.md](frangitur-the-great-evil.md)). Whether the vision’s “destroy the Shroud” guidance is honest or ironic remains **open**.
- **Gate note:** Strong candidate for a rare **vision instance** (or seamless overlay) — not a full world reload loop.

### Beat A0-09 — Tutorial completion → Act 1 handoff

- **Status:** draft — geography under reconcile (Dom D-P0-10)
- Vision ends; player navigates on foot toward **Ledgeport** (Act 1 hub) — do not auto-drop into the hub. DEC-0032 still describes an **O’hlundian evergreens** wake / camp tutorial; D-P0-05 deprecated the named evergreen wake as a competing Act 1 place — keep as biome/camp flavor on the retreat path until D-P0-10 locks.
- **Calrenoth on the map:** remains as a **ruined, impacted** seamless location the player can revisit (destroyed/attacked state), not a wiped instance.

---

## Act 1 — First hub and open world

### Beat A1-01 — Survivor camp / retreat

- **Status:** draft ([DEC-0033](../decisions/index.md#dec-0033-anywhere-player-camp-as-editable-instance-dao-style)); wake place under reconcile (Dom D-P0-10)
- Survivor path toward **Ledgeport**; rejoin Arkand (and possibly Larrell / Grenge remnants depending on A0-07). Establish immediate survival goal and Creotar’s warning as personal quest seed.
- **Camp tutorial:** teach pitch/setup, talk to camp NPCs/companions, leave camp. After this, player may camp from (nearly) anywhere on the overland map via the same persistent camp instance (DAO-style).
- Player then finds the road toward **Ledgeport** on foot.
- **Geography (draft, 2026-07-20 P0 pass):** Act 1 focus = **Ledgeport**; named evergreen wake deprecated as competing geography.

### Beat A1-02 — First hub (Ledgeport)

- **Status:** draft ([story-vision.md](story-vision.md) opening flow); **Act 1 major hub** — name **confirmed**
- **Ledgeport** (`ledgeport`) — **neutral** coastal free town ruled more by market than feudal crown: mayor possible; merchant guild; undermarket / black market; visiting traders from Arrotrebae and Cristallo. Side quests derive from market / spy / money pressure.
- Unlock services, companion camp view concepts, tavern/carriage post (FT discovery), and the wider campaign structure.
- Soft-open surrounding regions of the 4×4 km world; Act 1 dual-path intro (Thalassar ↔ Cristallo) branches from here.
- **Still open:** world coords vs Calrenoth; ferry dock wiring to Cristallo (Dom D-P0-09).

### Beat A1-03 — Open-world unlock

- **Status:** draft
- Main path stops being a corridor: player may explore, take side content ([TICKET-0022](../planning/tickets/TICKET-0022.md)), and pursue crystal / Shroud leads.
- Soft gates may still lock high-chaos or late story regions.

### Beat A1-04 — Meet Vanessa (timing TBD)

- **Status:** draft / **open** exact beat
- Introduce Vanessa as pragmatic counterweight ([companions.md](companions.md)). Exact hub vs road encounter TBD.

### Beat A1-05 — Coastal Arrotrebae succession (assassination plot)

- **Status:** draft ([factions.md](factions.md#design-session-2026-07-20-p0-questions-pass--draft))
- Early Act 1 political thread: the leader of **The Thalassar** is assassinated — plot led by the **Sea of Whispers** lieutenant (Act 1 boss body).
- Succession: candidates run **trials** to attune to **Anál Muir**; player champions a **good Thalassar candidate**; **The Underflow** pushes an underhanded usurper path. Powers are literal leader-only (optional temporary quest boon).
- Teaches how Arrotrebae tribes work as part of Act 1 dual-path intro (light standing); keep major Cristallo vs Arrotrebae allegiance for later.
- Contact geography: **Ledgeport** — not owned by The Thalassar or Cristallo.
- **Do not invent** leader/successor person names (TBD/dynamic) or quest id until authored. Transcript “Aerotropy/Atrobia” = Arrotrebae mishearing.

---

## Act 2 — Faction war and allegiance

Mid-campaign beats stay **draft** where faction gaps from [TICKET-0021](../planning/tickets/TICKET-0021.md) block detail.

### Beat A2-01 — Cristallo contact

- **Status:** draft / **open** theology-politics
- Player engages Cristallo-aligned houses; morality and reputation begin to matter.

### Beat A2-02 — Arrotrebae / tribal contact

- **Status:** draft / **open** council rules
- Parallel pull toward tribal path; Vanessa arcs may fork here ([companions.md](companions.md)). Coastal tribe from A1-05 may already be known — deepen into umbrella council / same-god conflicts rather than re-introducing from zero.

### Beat A2-03 — Imperium pressure on the map

- **Status:** draft
- Soft-gated Imperium advances, corrupted ground, optional dungeon instances for Nefarium sites.

### Beat A2-04 — Crystal / Shroud lead advances

- **Status:** draft / **open** location
- Pursue Creotar’s crystal (or competing interpretations of “destroy the Shroud”) — may be dramatic irony ([prologue-and-opening.md](prologue-and-opening.md)).

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

---

## Act 4 — Endings (open)

### Beat A4-01 — Morality / faction lock-in resolution

- **Status:** **open** (thresholds undefined in [story-vision.md](story-vision.md))

### Beat A4-02 — Ending branches

- **Status:** **open**
- Oppose evil, exploit it, or become greater evil; Cristallo/Arrotrebae preserved, reformed, or dismantled.

---

## Named cast introduced by Act 0 (draft)

| Name | Role | Status |
| --- | --- | --- |
| Arkand | Companion; King’s Guard knight | draft (established companion concept) |
| Commander Grenge | Calrenoth commander | draft |
| Sergeant Larrell | Rear drawbridge NCO | draft |
| Damius | Scout blamed for Rinos | draft |
| King Asher | Absent / brittle support | draft (see squire doc) |
| Creotar | Vision guide; knowledge god (Creo = short form; Frangitur = fallen form) | draft; identity locked 2026-07-20 |
| Grul’thaz the Black Howl | Prior Shroud bearer (Shadowpaw) | draft; faction naming **open** |
| Luceran the Hollow | Dark rider / Usurper | established premise |

## Open questions (beat sheet)

- Exact Calrenoth / **Ledgeport** / neighbor world coords — theaters in [official-world-map.md](official-world-map.md#draft-faction-theaters-2026-07-20).
- ~~Exact placement of O’hlundian evergreens wake vs Ledgeport~~ — evergreen wake deprecated as competing Act 1 geography; DEC-0032 wake/camp reconcile → Dom D-P0-10.
- ~~Whether Calrenoth stays permanently ruined on the seamless map after Act 0~~ — **resolved** DEC-0032.
- ~~Creotar identity vs Creo/Frangitur~~ — **resolved** 2026-07-20; whether “destroy the Shroud” is honest guidance or Frangitur manipulation still **open**.
- Crystal location (Twine stubs empty) and who currently holds it.
- ~~Wild God revival chronology vs Calrenoth Act 0~~ — Calrenoth / **Landfall** is the default spine.
- Exact Vanessa introduction beat.
- A1-05 quest id / standing numbers; **Sea of Whispers** reveal timing; lieutenant **display name** (boss body fight locked; masks + **Anál Muir** locked) — [factions.md](factions.md#the-sea-of-whispers).
- **Ledgeport** ferry wiring to Cristallo (name **confirmed**).
- Morality thresholds and ending matrix (Act 4).
- Twine source covers **Act 0 only**; Acts 1–4 are planning beats, not yet authored in Twine.

Also tracked in [`context/interviews/open-questions.md`](../interviews/open-questions.md).
