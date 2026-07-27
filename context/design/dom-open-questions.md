# Dom Open Questions (World Design)

Status: active questionnaire for Dom (world designer)  
Audience: Dom + owner review sessions  
Last refreshed: 2026-07-22 (`recording_udm8xugk` pass — P0 cleared; Act 0+ authoring backlog remains)

This is the **design tab** of story questions Dom should still answer. Engine/product opens stay in [`../interviews/open-questions.md`](../interviews/open-questions.md). Full faction essays stay in [`../story/factions.md`](../story/factions.md). Finished locks: [`dom-answered-questions.md`](dom-answered-questions.md). Beat sheet / side quests: [`../story/campaign-beat-sheet.md`](../story/campaign-beat-sheet.md), [`../story/side-quest-catalog.md`](../story/side-quest-catalog.md).

**How to use:** prefer the in-editor **Design Docs → Dom Open Questions → Form** view (answer fields + Submit writes this file). Or edit the Answer column here. Prefer real names over placeholders. Agents must not invent answers here. After a lock, move the row to the answered archive and keep only leftovers here.

**Priority ladder**

| Priority | Meaning |
| --- | --- |
| **P0** | Blocks Act 0 / early Act 1 geography, cast seeds, or World Forge hubs |
| **P1** | Needed for Act 1 coastal politics, Ledgeport hub, dual-path intro, succession |
| **P2** | Important lore / mid-campaign; can wait until Act 2+ authoring |

---

## P0 — Answer first

No open P0 rows after 2026-07-22 `recording_udm8xugk` pass. Archive: [`dom-answered-questions.md`](dom-answered-questions.md). Remaining Act 0→1 blockers live in **P1** cast naming and **D-P2-08** coords.

---

## P1 — Act 1 coastal / hub / succession

| ID | Question | Why it matters | Current draft / constraint | Answer |
| --- | --- | --- | --- | --- |
| D-P1-10b | Name **Arrotrebae tribes** for council seats vs open conflict; name the **1–2 Luceran-lost** tribes (when ready) | Relationship edges + Act 2 Arrotrebae arc | Partial 2026-07-22: **Thalassar** not Luceran-fallen; **Underflow** Luceran-influenced; full roster deferred to Act 2. Do not invent names here | |
| D-P1-12 | Name the **Thalassar assassinated leader**, the **good successor** the player champions, and the **Underflow-backed usurper** | A1-05 quest cast; relationship graph; dialogue trees | Roles locked (assassination → trials → Anál Muir attunement → champion path + usurper). Underflow war-chief **Drul’gath** locked — usurper may be Drul’gath or a separate proxy. Person names for leader/successor/usurper **TBD** until Dom names them. Greek–Irish hybrid naming for Thalassar faces | |
| D-P1-13 | Name **Ledgeport hub faces**: mayor (if any), merchant-guild head, undermarket contact, and ferry captain (replace or lock **Old Noll**) | Act 1 hub dialogue + SQ seeds; Map / relationship nodes | Ledgeport = market free-town; mayor **possible**; guild + undermarket locked as character. Catalog uses draft **Old Noll** (SQ-10) | |
| D-P1-14 | Lock **Vanessa introduction** beat: Ledgeport road, banquet (SQ-06), camp, or Cristallo-path first contact? | Companion unlock timing; dialogue authoring order | Beat A1-04 timing **open**. SQ-06 can introduce her as Cristallo-adjacent; else road default | |
| D-P1-15 | **Display name + quest id** for main Beat **A1-05** (Thalassar succession / Pneumyra plot) | `quests.worldforge.json` main entry; dialogue tree root | Spine locked; no quest title/id yet. Do not invent — Dom names | |
| D-P1-16 | When does the player learn **Sea of Whispers is false** / **Pneumyra** is Luceran’s lieutenant (vs public **She Who Sings the Undertow**)? | Dialogue reveal beats; shrine copy; boss framing | Names + boss body locked; **reveal timing** open. Anál Muir channeling Sea of Whispers directly still open | |
| D-P1-17 | Name **1–2 White Lotus first-contact NPCs** (or “roles only until Act 2”) for the Cristallo dual-path intro | Act 1 Part 2 / early Act 2 dialogue; crystal path | White Lotus = elected guardians of Creator crystal; crystal name/location still D-P2-02b | |
| D-P1-18 | Lock or rewrite **side-quest draft NPC names** for Act 1 authoring: Pellin, Mara of Rinos, Brannoc, Lady Vesperine, Kerra, Jask, Holm family | Dialogue + relationship seeds; avoid shipping placeholders | Catalog SQ-01–11; placeholders flagged draft. Prefer Greek–Irish for Arrotrebae faces; kingdom faces freer | |
| D-P1-19 | **Camp tutorial relationship beats** post–Act 0: which companion talks first, what player learns (storage / rest / travel), any non-companion camp NPC? | DEC-0033 camp handoff; dialogue trees before Ledgeport | Camp = companion relationships + storage + rest HP; travel-only; no survival feeding. Arkand present; Vanessa timing may still be open (D-P1-14) | |
| D-P1-21 | Confirm or rename draft **House Ashfell** (Ashfell Blade home org); name 1–2 house faces | Melee lane org story; relationship seeds | Owner draft 2026-07-24 ([DEC-0044](../decisions/index.md#dec-0044-starting-archetype-lane-orgs-and-rename)). Do not invent faces here | |
| D-P1-22 | Name **Outrider Lodge** and **Runecaster Guild** first contacts (or roles-only) | Ranged/magic lane org quests | Orgs locked by DEC-0044; person names Dom-owned | |

---

## P2 — Broader lore / mid-campaign (can wait)

| ID | Question | Notes | Answer |
| --- | --- | --- | --- |
| D-P2-02b | **Display name** for the Creator-origin crystal + **exact location** on Cristallo central island | White Lotus guardians + cleanse-Nefarium behavior locked 2026-07-22 | |
| D-P2-01b | Cristallo **hierarchy titles** (theological pinnacle vs secular head) and names for the **Luceran-puppet** Scarlet-Crusade sub-faction | Old/New Testament + church/state split locked 2026-07-22 | |
| D-P2-04b | Lock display name for the **Black Howl wolf patron** (transcript “Goldrin”) | Black Howl + Grul’thaz + corrupted golden→black wolf locked 2026-07-22 | |
| D-P2-05 | Is Kingdom of Tessera a **playable faction choice** or the **arena** around others? | Schema / standing roots | |
| D-P2-06 | Player influence **numbers**: Cristallo/Arrotrebae standing thresholds, lock-in, destroy vs reform | Engine model exists ([DEC-0029](../decisions/index.md#dec-0029-continuous-faction-standing-with-hostility-transfer)); numbers story-owned | |
| D-P2-07 | Imperium heraldry: lock dark-crusade / fractured-creator emblem (retire Roman-eagle read) | Art shipped 2026-07-20 (`heraldry-chaotic_imperium.png`); awaiting owner lock | |
| D-P2-08 | Exact theater **borders** + v1 **4×4 km** footprint + locked **Ledgeport** / Calrenoth / **Porto Lucente** world coords | Soft callouts only until locked; Calrenoth tip + Ledgeport-as-Act-1-focus + ferry to **Porto Lucente** U-bay already confirmed | |
| D-P2-09 | Name remaining Arrotrebae tribes / orc warbands beyond Act 0–1 | Target ~4–5 tribes over time; name from land; includes D-P1-10b Luceran-lost tribes when ready | |
| D-P2-10 | Rockzula / Rak Zulla easter-egg placement | Joke only; not campaign spine | |
| D-P2-11 | Does **Grul’thaz** stay slain (Twine) or return as a later antagonist / ghost / warband remnant face? | SQ-07 Shadowpaw scraps; Act 2–3 orc theater | Twine: Luceran slew him taking the Shroud. Survival for later beats **open** | |
| D-P2-12 | Companion **loyalty break points**, romance vs intimate-bond support, death outside hardcore | [`companions.md`](../story/companions.md); Act 3 A3-02 | Co-op: shared morality + party ≤4 (2P + 2 companions). Do not invent break thresholds | |
| D-P2-13 | Act 4 **ending matrix** sketch: oppose / exploit / become evil × Cristallo–Arrotrebae preserved / reformed / dismantled | Needs D-P2-06 numbers; story-vision still open | Soft list of ending *types* OK before numbers | |
| D-P2-14 | Is Creotar’s Act 0 “**destroy the Shroud**” guidance **honest**, **ironic** (frees Frangitur), or **partially true**? | Prologue irony; Act 3 climax framing; dialogue honesty flags | Act 3 free-“Creotar” → free Frangitur locked as Luceran’s manipulation; Creotar vision honesty still open | |
| D-P2-15 | Confirm **House Ashfell** vs rename; optional tribe obligation for non-Ashfell lanes | [`ashfell-blade.md`](../story/ashfell-blade.md); [DEC-0044](../decisions/index.md#dec-0044-starting-archetype-lane-orgs-and-rename) | Owner draft house name 2026-07-24; faces via D-P1-21 | |

---

## Story authoring priority (create backlog)

Work Dom + writers should queue **after** (or interleaved with) the naming questions above. Agents seed World Forge **only** after Dom locks names/ids. Do not invent person/quest titles here.

### Persons to add (relationship graph)

| Priority | Role / slot | Needs Dom name? | Seed after | Notes |
| --- | --- | --- | --- | --- |
| **P0** | Underflow war-chief **Drul’gath** | Display locked | seeded (`drul_gath`) | Act 0 corridor + A1 usurper path face |
| **P0** | Pneumyra person node | Display locked | seeded (`pneumyra`) | Edges to Luceran + Thalassar plot |
| **P0** | Grul’thaz person node | Display locked | seeded (`grul_thaz`) | Edges to Black Howl; Luceran opposes (D-P2-11 fate open) |
| **P1** | Thalassar leader (assassinated) | Yes (D-P1-12) | D-P1-12 | Member_of Thalassar; rite_of Anál Muir |
| **P1** | Thalassar successor (championed) | Yes (D-P1-12) | D-P1-12 | Player ally path |
| **P1** | Underflow-backed usurper | Yes (D-P1-12) | D-P1-12 | May be Underflow orc or Luceran-leaning proxy |
| **P1** | Ledgeport mayor / guild / undermarket / ferryman | Yes (D-P1-13) | D-P1-13 | Hub conversation anchors |
| **P1** | White Lotus contact(s) | Yes (D-P1-17) | D-P1-17 | Cristallo dual-path |
| **P1** | Side-quest faces (Pellin, Mara, …) | Lock or rename (D-P1-18) | D-P1-18 | SQ-01–11 |
| **P2** | Cristallo theological + secular heads | Titles (D-P2-01b) then names | D-P2-01b | Act 2 politics |
| **P2** | Luceran-puppet crusade leader | Yes (D-P2-01b) | D-P2-01b | Overzealous sub-faction face |
| **P2** | Ashfell Blade house patron / Outrider Lodge + Runecaster Guild contacts | Yes (D-P1-21 / D-P1-22) | D-P1-21 / D-P1-22 | Char-gen + lane org pressure ([DEC-0044](../decisions/index.md#dec-0044-starting-archetype-lane-orgs-and-rename)) |

### Relationship edges to author (after persons exist)

| Priority | Edge sketch | Blocks |
| --- | --- | --- |
| **P0** | ~~Pneumyra → Luceran (`serves`); Pneumyra → Thalassar (`influences`)~~ — seeded |
| **P0** | ~~Underflow war-chief → Underflow (`leads`); Luceran → Underflow (`influences`)~~ — seeded (`drul_gath_leads_underflow`, `luceran_influences_underflow`) |
| **P0** | ~~Grul’thaz → Black Howl (`leads`); Luceran → Grul’thaz (`opposes`)~~ — seeded; slew vs survive still D-P2-11 |
| **P1** | Assassinated leader → successor (`predecessor`); usurper → Underflow; champion ↔ player (quest flag, not static edge) |
| **P1** | Ledgeport faces → Kingdom / neutral market (not owned by Thalassar or Cristallo) |
| **P1** | Vanessa intro venue → Cristallo lean or road-neutral (D-P1-14) |
| **P2** | White Lotus → Creator crystal; puppet crusade → Luceran (`puppet`) |
| **P2** | Arkand / Vanessa loyalty break conditions (flags, not only static edges) |

### Quests to create / finish seeding

| Priority | Quest | Status today | Next Dom/writer step |
| --- | --- | --- | --- |
| **P0** | `mq_act0_calrenoth` | Seeded; Twine dialogue attached | Fill empty objective `dialogueId`s; camp handoff stage after vision (D-P0-10 done) |
| **P1** | **A1-05 Thalassar succession** (main) | Beat only — **no** quest id/title | Name via D-P1-15; objectives: meet tribe → assassination fallout → trials → Pneumyra boss → succession outcome |
| **P1** | Ashfell / Outrider Lodge / Runecaster Guild first quests | Beat stubs only | After D-P1-21 / D-P1-22; readiness `archetype_lane_org_quests` |
| **P1** | Act 1 Cristallo dual-path intro (main beat package) | Beat A2-01 early slice | After D-P1-17 / D-P1-20; ferry **Ledgeport** → **Porto Lucente** |
| **P1** | SQ-01 Cart Again → SQ-04 Larrell’s Muster | Catalog solid; WF partial (01–02 only) | Seed remaining; lock NPC names (D-P1-18) |
| **P1** | SQ-05 Grove Tithe / SQ-09 Crystal Rumor / SQ-10 Island Ferry | Catalog solid; not fully seeded | Align ferry with **real** Ledgeport→Cristallo route vs Islet of Cinders side trip |
| **P2** | SQ-06 / 07 / 08 / 11 | Catalog solid | After Cristallo / orc names; Grul’thaz fate (D-P2-11) |
| **P2** | SQ-12 Rak Zulla | Easter egg | Placement only (D-P2-10) |

### Dialogues to create

| Priority | Tree | Notes |
| --- | --- | --- |
| **P0** | Act 0 polish — Creotar crystal stubs | Twine empty “crystal location” passages; keep ironic until D-P2-14 |
| **P0** | Act 0 → camp handoff lines | Arkand (and Larrell if saved) wake / travel-to-camp tutorial |
| **P1** | Ledgeport hub greetings | Mayor / guild / undermarket / ferryman (D-P1-13) |
| **P1** | A1-05 succession package | Leader death scene; trial NPCs; champion; usurper; Pneumyra boss banter; Anál Muir rite |
| **P1** | Vanessa introduction | Matches D-P1-14 venue |
| **P1** | SQ-01–04 dialogue trees | Highest Act 1 side priority; Arkand banter on SQ-01 |
| **P1** | Sea of Whispers shrine dread copy | Epithet **She Who Sings the Undertow**; reveal gated by D-P1-16 |
| **P2** | Cristallo church/state + White Lotus induction | After titles / crystal name |
| **P2** | Companion loyalty stress (Act 3) | After D-P2-12 |
| **P2** | SQ-05–11 trees | After NPC name lock |

### Other prioritize soon

| Priority | Item | Why |
| --- | --- | --- |
| **P0** | Map anchors: **Porto Lucente** + dock once D-P2-08 coords lock | Ferry soft-travel endpoint named; world coords still open |
| **P0** | Calrenoth ruined revisit state notes | Seamless post–Act 0; SQ-03 ledger spawn |
| **P1** | Thalassar rite-site POI polish (`thalassar` Muirthalia site already stubbed) | A1-05 trials stage |
| **P1** | Ledgeport services list (tavern, carriage/FT, market board, undermarket door) | Hub UX + SQ notice-board starts |
| **P1** | Dual-path travel soft-gates (Thalassar coast ↔ Cristallo ferry) | Act 1 structure D-P1-08 |
| **P2** | Standing number pass (D-P2-06) | Unblocks consequential SQ magnitude |
| **P2** | Ending beat outline (D-P2-13) | Act 4 planning |

---

## Next session focus (suggested order)

1. **D-P1-12** / **D-P1-15** — Thalassar succession cast + A1-05 quest title (usurper may be distinct from **Drul’gath**)
2. **D-P1-13** / **D-P1-14** / **D-P1-19** — Ledgeport faces, Vanessa intro, camp tutorial beats
3. **D-P1-16** — Pneumyra / false-cult reveal timing
4. **D-P0-12 leftover** — which Act 0 face is the optional hostage (Grenge / Larrell / Damius)
5. **D-P2-02b** — Creator crystal name + map location (White Lotus seat)
6. **D-P1-10b** / **D-P2-09** — next Arrotrebae tribe names when Act 2 planning starts
7. **D-P2-01b** — Cristallo hierarchy titles + puppet sub-faction name
8. **D-P2-08** — world coords for Ledgeport / Calrenoth / **Porto Lucente**

When answers land: fill Answer, move finished rows to [`dom-answered-questions.md`](dom-answered-questions.md), mirror into [`../story/factions.md`](../story/factions.md), [`../story/campaign-beat-sheet.md`](../story/campaign-beat-sheet.md), [`../story/official-world-map.md`](../story/official-world-map.md), [`../story/companions.md`](../story/companions.md), and World Forge seeds (named `clan` / persons / POIs / quests only after Dom names them).
