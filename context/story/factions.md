# Factions of Tessera

- Status: developing story context
- Gap review: TICKET-0021 (2026-07-10) — status labels and gaps below; no new canon invented

## Canon status summary

| Faction / group | Status | What is established | What remains draft / open |
| --- | --- | --- | --- |
| Kingdom of Tessera | **draft** (arena vs faction choice open) | Dominant human occupying power *within* Tessera ([DEC-0034](../decisions/index.md#dec-0034-tessera-is-the-worlds-primary-land)); land fractured by chaotic forces; heroic medieval visual tone (*Gondor*/*Rohan* as references only) | Whether the kingdom is a playable faction choice or the political arena around others |
| Chaotic Imperium | **established** (existence + leader); **open** (Frangitur/Shroud links) | Evil coalition of living + ethereal forces; led by Luceran the Hollow; structure exists because a monarch heads it — without that head it would run wild ([design session 2026-07-20](#design-session-2026-07-20-faction-structure--theaters-draft)); heraldry art: dark crusade / fractured-creator (`heraldry-chaotic_imperium.png`; Roman eagle retired) | How Imperium authority ties to Frangitur, the Nefarium Shroud, and Luceran’s remaining agency; owner lock on heraldry emblem |
| The Cristallo | **draft** | Religious/noble oligarchy; selective; higher social classes; refined architecture; hostile to woodland faction; can shift good/evil via player influence (proposal); **theater draft:** central island / origin-crater seat ([official-world-map.md](official-world-map.md#draft-faction-theaters-2026-07-20)); **2026-07-22:** Old/New Testament faith split + church/state bipartisan politics; **White Lotus** relic guardians; Creator crystal **Claritas** cleanses Nefarium (staff-borne by Cristallo leader, 2026-07-29) | Hierarchy titles; Claritas court map pin; Luceran-puppet sub-faction name |
| Arrotrebae of the Rampant Wilds | **draft** | **Umbrella culture**, not one contiguous kingdom — regional tribes molded by local land, Loa/gods, and lifestyle; council of **tribal leaders** (rite-passed seats); **2026-07-22:** **majority consensus** default; dissenters may **challenge champion**; adversarial with Cristallo; Greek–Irish naming theme; **The Thalassar** (`thalassar`) as first named clan; public mask **Muirthalia**; minor leadership relic **Anál Muir** (`anal_muir`); false patron **The Sea of Whispers** | Council seat roster; Luceran-lost tribe **names** (Underflow = Luceran-influenced orc warband, not Arrotrebae) |
| Orc warbands | **draft** (structure); **open** (some names) | **Umbrella culture** of multiple warbands; may support or oppose Imperium; **The Underflow** (`underflow`) — Act 0 minor warband inland of Calrenoth; **Luceran-influenced** **coerced ally** (2026-07-29); war-chief **Drul’gath** (`drul_gath`); public mask **Grakk-Maren**; same false **Sea of Whispers** cult as Thalassar; **The Black Howl** (`black_howl`) — **first-war Shroud bearer**; war-chief **Grul’thaz** (Shadowpaw) | Other warband names/IDs; wolf patron display name (“Goldrin” draft) |
| Player influence (Cristallo / Arrotrebae) | **proposal** | Engine model: continuous standing + hostility transfer + lock-in fields ([DEC-0029](../decisions/index.md#dec-0029-continuous-faction-standing-with-hostility-transfer)). Both start neutral in story; morality remains a separate track. | Numeric thresholds, destruction/reform, morality binding — still open; do not invent numbers in World Forge seeds |

Labels follow [story index continuity](index.md): **established** = current story context; **draft** / **proposal** / **open** = needs owner review before canon.

## Kingdom of Tessera

The Kingdom of Tessera is the dominant human occupying power **within Tessera** (the primary land of the world — [DEC-0034](../decisions/index.md#dec-0034-tessera-is-the-worlds-primary-land)). It is populated primarily by humans across many cities and settlements. Chaotic forces have divided the land and fractured those previously aligned with its principal good faction.

**Theater (draft, 2026-07-20):** the kingdom’s main territory is planted in the **west**. **Calrenoth** is a Tessera-built frontier landing / fortress on the **western peninsula tip** (owner-placed; Dom + owner confirmed) facing Imperium pressure from the south — outside the western core, which explains why it is hard to reinforce. Two approaches: a **landlocked** entrance (player Act 0 approach) and a **moat-scale drawbridge** to another land spur — not a peninsula-spanning bridge ([official-world-map.md](official-world-map.md#draft-faction-theaters-2026-07-20)).

The kingdom's visual direction is heroic medieval fantasy, with *Gondor* and *Rohan* cited as tonal references rather than setting canon.

## Chaotic Imperium

The Chaotic Imperium gathers the evil forces that disrupt the player's journey. It combines living creatures with ethereal spirits. Older art notes used a dark Roman visual language as a tonal reference only. Shipped Map Canvas heraldry (`heraldry-chaotic_imperium.png`) uses a **dark crusade** / fractured-creator read (cracked oxblood field, spiked halo + jagged starburst) — Roman-loyalist eagle and patriotic crown retired. Owner may still lock or revise the emblem.

Luceran the Hollow is the Imperium's current **monarch / head**. Draft direction (2026-07-20): without a head, Frangitur’s creation would run wild and kill everything; Luceran is what currently gives the Imperium structure. Frangitur remains the deeper creator / manipulator figure behind that creation.

**Authority lock (draft, 2026-07-22; purpose reinforced 2026-07-29):** Luceran holds Imperium command because **Frangitur is trapped within the Nefarium Shroud** and Luceran controls that binding. The Shroud was made to **contain Frangitur** and **bind** Chaotic Imperium forces; breaking it **releases Frangitur** and drops the binding. Luceran manipulates the player toward Act 3 to “free Creotar” — in reality freeing **Frangitur**. The player should not learn Creotar = Frangitur until late game. Luceran remains in charge until defeated by the player.

**Lieutenant aesthetic (2026-07-22):** Imperium agents may appear as corrupted Arrotrebae/orc silhouettes — dark-spawn–like, mist-wreathed bodies (e.g. **Pneumyra** as siren-bodied Sea of Whispers lieutenant).

## Neutral Factions

### The Cristallo

The Cristallo represents religious and noble power. It is selective, oligarchic, associated with higher social classes, and hostile toward the opposing woodland faction. It has a distinct and refined architectural identity.

**Naming language (draft lock, 2026-07-22):** Cristallo places and persons lean **Latin / Italian** (Dom + owner; e.g. ferry landing **Porto Lucente**). Distinct from Arrotrebae Greek–Irish hybrids.

**Theater (draft, 2026-07-20):** the Cristallo’s seat is the **central island** in the interior sea — tied to crater / world-origination lore (Pangea-break allegory). Ferry from **Ledgeport** crosses the channel to **Porto Lucente** (`porto_lucente`) in the island’s **U-shaped channel bay** (2026-07-22); dock POI `porto_lucente_dock`.

The organization can develop toward good or evil through player influence.

**Faith and politics (draft lock, 2026-07-22):** Cristallo worship follows **Creotar** (Creo) as understood at world’s founding. A split analogous to **Old Testament vs New Testament** (not Bible structure) drives public debate: whether Creotar still exists, left, or will return; some faithful secretly know **Creotar fell to Frangitur**; Luceran-aligned infiltrators manipulate the church. After Creotar’s long absence, **church and state separated** into two political lines: (1) **theological reinstatement** — faith should govern again; (2) **secular democratic order** — society runs through the people without waiting for Creotar. The parties conflict; a **Scarlet Crusade–like** religious sub-faction becomes overzealous when **Luceran** manipulates it (Imperium agents may puppet leadership while believers think they serve Creotar).

**Creator relic (draft, 2026-07-22; name lock 2026-07-29):** founding Cristallo met Creotar and received a **Creator-origin crystal** — display name **Claritas** (`claritas`) — that seeded their society. Form: crystal set in a **staff** forged to wield it; **Cristallo’s leader bears the staff** (not a vault megachurch). An elite **White Lotus** order (Avatar: The Last Airbender reference — elected guardians, *Unknown Soldier* preservation cycle) protects the relic / path. It is the **inverse of Nefarium**: contact **cleanses** Shroud corruption. Cristallo Act 2 duty (faith path): join White Lotus, navigate politics, wield **Claritas** against **Frangitur / Nefarium Shroud** effects. Exact court **map pin** still open (D-P2-08).

Hierarchy titles (theological pinnacle vs secular head) and the Luceran-puppet sub-faction **name** remain open.

### Arrotrebae of the Rampant Wilds

The Arrotrebae are an **umbrella culture** — not one contiguous kingdom sending warbands from a single capital. They are a woodland coalition of **many regional tribes** molded by the land they inhabit (northern Loa/lifestyle vs other biomes). Chieftains act as local battlelords; no chieftain rules all others.

**Naming language (draft lock, 2026-07-20):** all Arrotrebae subgroups (clan names, liturgy mask-names, person names when authored) should read as **Greek–Irish hybrids** — Hellenic sea/land roots braided with Irish/Gaelic sound and meaning. Not pure Greek, not pure Gaelic; the blend is the cultural signature. Orc warband names stay outside this rule.

**Council (draft):** there should be a council for emergencies / shared service to the world (Gaia / mother-earth analogue — deity naming **open**). Not every tribe sits on it; some may be in conflict or even war with one another. **Decision rule (draft lock, 2026-07-22):** when rite-passed tribal leaders disagree, **majority consensus** decides policy. **Trial by combat** resolves **criminal** disputes (orc-flavored; Arrotrebae may adapt a similar rite) — not ordinary policy splits. A leader who refuses consensus may **challenge the champion** of the prevailing side in combat.

Expect roughly **four or five** named tribes over time; name them from the land they occupy, and prioritize tribes that touch **Act 0 / Act 1** before inventing the full set. Map heraldry for “Arrotrebae” is the umbrella emblem until tribe emblems exist.

The faction draws tonal inspiration from fantasy barbarians and druids. Its relationship with the Cristallo is openly adversarial. Some tribes may be matriarchal or nature-nativist depending on local history — **proposal**, not locked.

**Coastal pirate / seafarer tribe (draft, 2026-07-20):** one regional Arrotrebae tribe occupies coastal / seafarer territory near the Act 1 southwest theater and **Ledgeport**. They are an **advanced civilization**, not a cartoon “primitive tribe” — pirate culture and water-deity worship can coexist with organization and ritual law. Transcript spellings “Aerotropy” / “Atrobia” / “Itro B” are **mishearings of Arrotrebae**, not a separate umbrella ([transcript-canon-alignment](../../.cursor/rules/transcript-canon-alignment.mdc)).

### The Thalassar

**Status:** **draft** (display name locked — Dom + owner, 2026-07-20)

**The Thalassar** is the first named **clan** under the Arrotrebae umbrella. World Forge id: `thalassar` (`parentId`: `arrotrebae`). Display name is **The Thalassar** only — no geographic suffix.

They are the Arrotrebae presence **south of the Calrenoth peninsula** (Act 0 neighbors) and the **coastal / seafarer** power in the Act 1 southwest theater (**Ledgeport** vicinity, beat A1-05 succession). Tonal references: organized pirate culture, twilight/coastal liminality, deep-water deity tradition (Cthulhu-like depth as mood, not a separate faction name).

Public liturgy mask-name locked: **Muirthalia** (worshippers do not use “Sea of Whispers” in rite). Minor leadership relic locked: **Anál Muir** (`anal_muir`) — Irish *anáil* (breath) + *muir* (sea); rite-gated leader attunement; deliberately ≪ **Nefarium Shroud**. Powers locked: **literal** water-walk / enhanced breath — **leader-only** ability; leader may grant as a **temporary quest boon** (not a player skill). Succession locked (draft): post-assassination **trials** to attune to **Anál Muir**; player champions a **good Thalassar candidate**; **The Underflow** runs an underhanded usurper path. Leader/successor **person names TBD**. Tribe heraldry: `heraldry-thalassar.png` (teal wave + trident). Still open: council seat vs other Arrotrebae. Regional cult **object** is the false **The Sea of Whispers** ([The Sea of Whispers](#the-sea-of-whispers)). Act 0 neighbor: **The Underflow** (mask **Grakk-Maren**).

### The Sea of Whispers

**Status:** **draft** (false deity — **owner confirmed**, 2026-07-20)

**The Sea of Whispers** is the **false deity** worshipped in the Calrenoth peninsula watershed (**The Thalassar** and **The Underflow**). Worshippers use other titles in sincere liturgy; **The Sea of Whispers** is the **hinted true name** for narrative (Act 0 / early Act 1 environmental whispers — springs, tide caves, rite-trance — not a full sermon in Act 0). It is **not** a legitimate Loa / Gaia-aligned god.

**Origin (draft, 2026-07-20 session):** Thalassar and Underflow originally shared the same water cult. The Underflow’s rite was **perverted**. The presence that presents itself as their god is **not** a true deity — it is a **lieutenant of Luceran the Hollow**, planted by **Frangitur** (false-god / demi-god pattern; candidate early Act 1 boss). Faith around it is pagan / deity-labeled, not a named organized religion.

Story id draft: `sea_of_whispers` (pantheon / World Forge deity entity when authored).

Public masks locked: Thalassar **Muirthalia**; Underflow **Grakk-Maren**. Thalassar leadership relic **Anál Muir** locked. **Manifestation locked:** player fights the **lieutenant body** as an Act 1 boss; the lieutenant **leads the assassination plot** (A1-05).

**Lieutenant locked (owner, 2026-07-22):** **Pneumyra** (`pneumyra`) — Greek *pneuma* (breath) + `-yra`; deliberate invert of **Anál Muir** (*anáil* + *muir*, sea-breath). Siren-bodied Luceran agent; boss display name **Pneumyra**. Those who do not know her name call her **She Who Sings the Undertow** — shrine-dread epithet chiseled on tide-cave stones (second line locals refuse to speak aloud). Still open: when the false nature is **confirmed** to the player; whether **Anál Muir** attunement channels this patron directly.

### The Underflow

**Status:** **draft** (display name locked — owner confirmed, 2026-07-20)

**The Underflow** is the first named **warband** under the orc warbands umbrella. World Forge id: `underflow` (`parentId`: `orc_warbands`). Display name is **The Underflow** only.

**Meaning:** *Under-* the ridge spine inland / east of the Calrenoth corridor, the warband holds **underflow** — subsurface water, seepage, and cave runoff that feeds the same watershed as the coast. Land-derived name; ties to whispering springs and sinkhole rites without using the false god’s title.

Minor local warband, not a continent-scale horde. Same regional cult as The Thalassar — false patron **The Sea of Whispers**; public liturgy mask **Grakk-Maren** (vs Thalassar **Muirthalia**). Same-god / heresy conflict: each side treats the other’s name as blasphemy.

**Luceran influence (draft lock, 2026-07-22):** **The Underflow** is **absolutely** under Luceran influence — contrast **The Thalassar**, where Act 1 is about **preventing** that fall. Not every orc warband is Luceran-controlled; some may share the same false patron without being mutually exclusive.

**Act 0 corridor stance (draft lock, 2026-07-29, D-P0-14):** **coerced ally** of Imperium / Nefarium Shroud — Luceran keeps a **lieutenant presence** in the corridor (Sea of Whispers / **Pneumyra** plant + Luceran-influenced warband under **Drul’gath**), not a free Imperium friendship.

**War-chief (draft lock, 2026-07-22):** **Drul’gath** (`drul_gath`) — Act 0 corridor presence and A1 usurper-path face. Orc/ridge tongue (same family as **Grul’thaz**); distinct from cult mask **Grakk-Maren**. The A1-05 usurper *candidate* may still be a separate named person (D-P1-12).

Warband heraldry: `heraldry-underflow.png` (tusks + underground black-water sinkhole).

### Calrenoth corridor — liturgy names

**Status:** **locked** (owner, 2026-07-20). Public deity titles used in shrine labels, prayers, and heresy barks. **The Sea of Whispers** stays the hinted true / narrative name (environmental whisper / later reveal), not the public rite title.

| Culture | Naming rule | Public mask-name | Meaning |
| --- | --- | --- | --- |
| **The Thalassar** | Greek–Irish hybrid (Arrotrebae theme) | **Muirthalia** | *muir* (Irish sea) + *thalía/thalassa* (Greek sea/bloom) — “sea-bloom / abundance of the deep” |
| **The Underflow** | Orc / ridge tongue | **Grakk-Maren** | *Grakk:* swallow / murmur-in-water; *Maren:* mother-waters — “she who swallows at the source” |

**Pattern:** both serve the same false power (**The Sea of Whispers**); each side treats the other’s name as heresy. Archive: [`../design/dom-answered-questions.md`](../design/dom-answered-questions.md) D-P0-04.

### Orc Warbands

Orcs follow the **same umbrella pattern** as the Arrotrebae: multiple warbands with different points of land, not one unified faction. Individual warbands may support or oppose the Chaotic Imperium depending on their exposure to Nefarium and their hatred of the Imperium's current leader.

**Shared bloodline (draft lock, 2026-07-22):** even when warbands are separate (Underflow vs Black Howl, etc.), orc leadership lines are **tied by royal / clan blood** — Dom analogy: dwarven clans that remain distinct but share bloodline. Exact genealogy between **Drul’gath** and **Grul’thaz** is **not** invented here; the rule is cultural structure only (`recording_udm8xugk_2026-07-22`).

**Regional conflict (draft):** in a given theater, an Arrotrebae tribe and an orc warband may worship **the same underlying power** under different **public liturgy names** and declare heresy — producing religious war. **Calrenoth corridor pair:** **The Thalassar** (**Muirthalia**) + **The Underflow** (**Grakk-Maren**); false patron **The Sea of Whispers** ([liturgy names](#calrenoth-corridor-liturgy-names)).

**Alliance nuance (draft):** some orc warbands (more intelligence-/lore-minded) may recognize a shared pantheon with Arrotrebae tribes and form local alliances; more warfaring warbands tend toward contention over the perverted cult. Frangitur actively seeds false gods into orc warbands.

### The Black Howl

**Status:** **draft** (display name locked — Dom + owner, 2026-07-22)

**The Black Howl** is a named **warband** under the orc warbands umbrella. World Forge id: `black_howl` (`parentId`: `orc_warbands`).

**History:** during the **first war**, **The Black Howl** held the **Nefarium Shroud** before **Luceran the Hollow** usurped it from their war-chief **Grul’thaz the Black Howl** (Twine: **Shadowpaw** clan). This is separate history from **The Underflow** (Act 0 minor warband).

**Patron (draft):** the warband’s wolf god (transcript “**Goldrin**”) was once a **golden/silver wolf of ferocity**; corrupted by **Frangitur / Nefarium** into a **black mist-wolf** silhouette — fallen regional god of the Black Howl theater. Patron **display name** still open.

**Leader:** **Grul’thaz** — corrupted orc war-chief; Twine names him prior Shroud bearer. Whether he was slain by Luceran or survives as a later antagonist remains **open** for beat authoring.

The great orc leader displaced when Luceran took the Shroud is **Grul’thaz** (draft lock 2026-07-22).

## Design session 2026-07-20 (faction structure + theaters) — draft

Source: owner + Dom (world designer) recording, 2026-07-20. Status: **draft** — lock geography/IDs only after owner review; do not invent tribe/warband names here.

| Topic | Draft takeaway | Still open |
| --- | --- | --- |
| Arrotrebae / orc structure | Umbrella cultures of regional tribes / warbands, not single kingdoms | Named tribes/warbands; Act 0 pair first |
| Arrotrebae council | Shared world/Gaia service; council = **tribal leaders** (rite-passed only), not an intra-tribe board; local wars still possible | Decision rules when chieftains disagree; which tribes on/off / Luceran-lost |
| Same-god conflict | Local Arrotrebae + orc warband may fight over different public liturgy names; Underflow rite perverted by Luceran lieutenant plant | **Pair locked:** Thalassar (**Muirthalia**) + Underflow (**Grakk-Maren**); false patron **Sea of Whispers** |
| Cristallo seat | Central island / origin-crater significance | Theology / biblical allegory detail |
| Tessera / Imperium theaters | Kingdom planted **west**; Imperium influence **south**; Calrenoth as Tessera’s southern frontier landing — see [official-world-map.md](official-world-map.md#draft-faction-theaters-2026-07-20) | Exact borders, coordinates, place names |
| Imperium leadership | Luceran as monarch/head; Frangitur as creator/manipulator behind the chaos | Shroud authority; Luceran agency |
| Imperium heraldry | Retire Roman-loyalist eagle read; move toward dark crusade / fractured-creator | Art shipped (`heraldry-chaotic_imperium.png`); owner lock vs revise |

## Design session 2026-07-20 (coastal Arrotrebae + relics + Act 1) — draft

Source: owner + Dom recording *World Building: Aquatic Pirate Factions*, 2026-07-20. Status: **draft**. Transcript spellings (“Aerotropy”, “Atrobia”, “Karanoth”, “Fairy Iron / Farium / Nefarian Shroud”) align to established **Arrotrebae**, **Calrenoth**, and **Nefarium Shroud** per transcript-canon-alignment — do not seed lookalike entities.

| Topic | Draft takeaway | Still open |
| --- | --- | --- |
| Coastal Arrotrebae tribe | Regional pirate / seafarer tribe under the Arrotrebae umbrella; advanced society; water-deity worship | **The Thalassar** (`thalassar`) + **Anál Muir** locked; powers literal / leader-only + quest boon |
| Leader power | Power is **rare** and **rite-gated**: the tribal leader attunes to **Anál Muir** (`anal_muir`). Abilities: enhanced water breathing, walking on water — **literal**, leader-only; temporary quest boon allowed | Whether other Arrotrebae tribes share the same rite pattern with different relics |
| Relic hierarchy | Minor tribal relics ≪ **Nefarium Shroud** (primary contentious artifact). **Anál Muir** confirmed in that minor tier. Cristallo holds / seeks a separate Creator-origin crystal/relic that polarizes against the Shroud but is intentionally **nerfed** relative to it | Crystal/relic name and location |
| Act 1 assassination plot | Assassination introduces Arrotrebae politics early; succession = **trials** to attune to **Anál Muir**; player champions good candidate; Underflow usurper path; lieutenant boss leads the plot | Quest id; person names (TBD/dynamic); standing reward numbers |
| Neutral trade port | **Ledgeport** (`ledgeport`) — coastal **neutral** market free-town (not Arrotrebae-owned, not Cristallo-owned); Act 1 quest hub — mayor possible, market-led politics, merchant guild + undermarket; traders from Arrotrebae + Cristallo | Ferry wiring lock to Cristallo; world coords |
| Act pacing | Act 1 = **dual-path intro** (Arrotrebae / Thalassar path ↔ Cristallo path by travel; finish one then the other). Standing consequences stay light. Act 2 = clearer allegiance pull (polar opposites: expansionist tribal god-worship vs orderly Creotar/Creator-centered isolationism) | Softcore/hardcore narrative forks unchanged |
| Rockzula Easter egg | Mentioned as a later placement joke (“Roxula” in transcript) — **not** canon content | Where / whether it appears |

## Design session 2026-07-20 (Landfall / open questions pass) — draft

Source: owner + Dom recording *Open World RPG Worldbuilding*, 2026-07-20. Status: **draft**. Transcript misspellings map per [transcript-canon-alignment](../../.cursor/rules/transcript-canon-alignment.mdc): “Kalarnoff” / “Aura Nath” / “Karanoth” → **Calrenoth**; “Aerotrobe” / “Atrobi” → **Arrotrebae**; “Thalasar” / “Pellasar” → **The Thalassar**; “Lucerne” → **Luceran**; “Frangator” / “Trengator” → **Frangitur**; “Crayo” → **Creo** / **Creotar**; “Sea of Wisconsin” → **Sea of Whispers**.

| Topic | Draft takeaway | Still open |
| --- | --- | --- |
| Open world vs instances | Most story plays in the seamless open world; rare instanced scenes when needed for density/vision. Calrenoth siege prefers open-world placement ([DEC-0021](../decisions/index.md#dec-0021-soft-gates-with-rare-optional-instances)) | Which beats stay instances (Realm of Darkness still a candidate) |
| Act 0 title | **Landfall** — tutorial establishing Chaotic Imperium, Luceran the Hollow, Kingdom of Tessera | **Final** marketing name (D-P0-15, 2026-07-29) |
| Calrenoth geography | Western peninsula tip confirmed; landlocked player approach + moat drawbridge | Exact coords / Map Canvas lock (D-P2-08 — level design) |
| Sea of Whispers | Luceran lieutenant planted by Frangitur; Underflow cult perverted; **lieutenant body = Act 1 boss** (leads assassination); lieutenant **Pneumyra** (`pneumyra`); public epithet **She Who Sings the Undertow** | Reveal timing; **Anál Muir** channeling |
| Creotar / Creo / Frangitur | Creotar = Creo (short); Frangitur = fallen form; Act 0 “destroy the Shroud” = **partially true** (D-P2-14) | Harden Act 4 destroy-vs-control matrix (D-P2-13) |
| Act 1 hub | **Ledgeport** (`ledgeport`) — market free-town / trade port; Act 1 focus region (evergreen wake deprecated as competing geography) | World coords (D-P2-08); DEC-0032 wake reconcile → campsite locked |

## Player Influence

The Cristallo and Arrotrebae begin as neutral powers under the current proposal. The player's morality, relationships, and decisions can push either faction toward good or evil, support one against the other, or potentially destroy one.

## Gaps blocking World Forge schema (TICKET-0011) and mid-campaign beats

Concrete gaps — do not invent answers in schema or beat work until owner-resolved. Also filed under [Open Questions](#open-questions) and [`context/interviews/open-questions.md`](../interviews/open-questions.md).

### Luceran–Frangitur–Shroud links

- ~~Is Imperium command granted by wearing the Shroud, by Frangitur’s will, by political myth, or a mix?~~ — **draft lock 2026-07-22:** Luceran commands because **Frangitur is trapped in the Shroud**; Luceran wears/controls the binding; Act 3 “free Creotar” manipulation frees Frangitur.
- ~~How much agency does Luceran retain vs Shroud/Frangitur control?~~ — Luceran retains command until defeated; player learns Creotar = Frangitur late.
- Reconcile battle-worn Shroud with throne-fused imagery ([nefarium-and-the-shroud.md](nefarium-and-the-shroud.md)).
- ~~Competing “Shroud as prison” proposal~~ — **elevated to draft lock 2026-07-29 (D-P2-14):** Shroud **contains Frangitur** and **binds** Imperium chaos; break = release + lose binding.

### Cristallo theology / politics

- ~~Faith, hierarchy, and what it means for Creotar (Creo) / Frangitur to be a god “under” the Cristallo~~ — **partial lock 2026-07-22:** Old/New Testament split; church/state bipartisan politics; White Lotus guardians; Creator crystal cleanses Nefarium.
- Relationship to Kingdom of Tessera noble houses (**House Ashfell** confirmed 2026-07-29) vs Cristallo as a separate power.
- Still open: hierarchy **titles**; **Claritas** court map pin (name locked); Luceran-puppet sub-faction **name**.
- Fields World Forge will eventually need: faith id, hierarchy ranks, settlement affiliation — partially unblocked; Claritas court POI still soft-blocked on D-P2-08.

### Arrotrebae council rules

- ~~How the council is structured~~ — **draft lock 2026-07-20:** council of **tribal leaders** (not intra-tribe); only **rite-passed** leaders sit.
- ~~How collective decisions work when those leaders disagree~~ — **draft lock 2026-07-22:** **majority consensus**; criminal **trial by combat**; dissenters may **challenge champion**.
- Named tribes / philosophical traditions for relationship-graph nodes — name from land; prioritize Act 0 / Act 1 tribes first ([design session](#design-session-2026-07-20-faction-structure--theaters-draft)).
- **Draft:** one or two tribes **lost contact** under **Luceran** influence (later Arrotrebae storyline space) — **names open**; **Underflow** is Luceran-influenced **orc warband**, not Arrotrebae.
- **The Thalassar (`thalassar`):** succession rite + powers locked ([The Thalassar](#the-thalassar)); council seat vs others still open. Mask **Muirthalia** + relic **Anál Muir** (`anal_muir`) locked; false patron **The Sea of Whispers** — lieutenant **Pneumyra** locked; reveal timing open ([The Sea of Whispers](#the-sea-of-whispers)).
- **The Underflow (`underflow`):** war-chief **Drul’gath** (`drul_gath`) locked; emblem, Act 0 hooks ([The Underflow](#the-underflow)). Mask **Grakk-Maren** locked; usurper path in A1-05 draft-locked (usurper *candidate* name still D-P1-12).

- Standing and hostility rules vs Cristallo for mid-campaign beats.

### Orc warband naming

- **The Underflow (`underflow`):** Act 0 minor warband — locked 2026-07-20 ([The Underflow](#the-underflow)); **Luceran-influenced** (2026-07-22); war-chief **Drul’gath** (`drul_gath`) locked 2026-07-22.
- ~~Which warband possessed the Nefarium Shroud and why.~~ — **The Black Howl** (`black_howl`) during the first war ([The Black Howl](#the-black-howl)).
- ~~Name of the great orc leader slain by Luceran.~~ — **Grul’thaz the Black Howl** (Shadowpaw) — draft lock 2026-07-22; whether slain vs survives for later beats **open**.

### Player influence rules

- **Model (DEC-0029):** continuous per-faction standing; hostility fallout from rival/opposes edges; ranks + lock-in authoring; morality is a separate track (not in TICKET-0181).
- Morality thresholds and when archetype selection “binds” a neutral faction ([story-vision.md](story-vision.md)) — still open.
- Numeric Cristallo/Arrotrebae thresholds — still open; leave standing configs empty/draft until owner fills numbers.
- Conditions for destroying a faction vs reforming it — still open.
- Whether Kingdom of Tessera is a faction choice or the arena (affects schema root).

## Open Questions

Dom-facing list: open [`../design/dom-open-questions.md`](../design/dom-open-questions.md) · answered [`../design/dom-answered-questions.md`](../design/dom-answered-questions.md).

- Define the Cristallo's faith, hierarchy, and relationship to Creo before his fall — **partial lock 2026-07-22** (Old/New Testament split, White Lotus, **Claritas** cleanses Nefarium); titles + Claritas court map pin still open.
- ~~Define how the Arrotrebae make collective decisions when chieftains disagree~~ — **majority consensus + champion duel** (2026-07-22); name tribes from land.
- ~~Identify which orc warband possessed the Nefarium Shroud~~ — **The Black Howl**; **Grul’thaz** prior bearer (2026-07-22).
- Decide whether the Kingdom of Tessera is itself a faction choice or the political arena surrounding the other factions.
- ~~Define Luceran–Frangitur–Shroud authority~~ — **draft lock 2026-07-22** (Frangitur trapped; Luceran commands; Act 3 irony).
- Specify player influence **numbers** (thresholds, destruction) for Cristallo and Arrotrebae; engine standing fields exist per [DEC-0029](../decisions/index.md#dec-0029-continuous-faction-standing-with-hostility-transfer).
- Owner-lock Imperium heraldry replacement (dark crusade / fractured-creator art shipped; Roman-eagle read retired).
- Lock exact theater borders / coordinates on [official-world-map.md](official-world-map.md) after owner review of the 2026-07-20 draft.
- ~~Name Thalassar minor relic~~ — **Anál Muir** (`anal_muir`) locked. Still: A1-05 quest/standing numbers; whether attunement channels **Sea of Whispers** directly ([The Thalassar](factions.md#the-thalassar)).
- ~~Name the neutral coastal trade port~~ — **Ledgeport** (`ledgeport`) locked; ~~ferry toward Cristallo~~ **yes** to **Porto Lucente** (`porto_lucente`) U-bay dock (D-P0-09 / D-P0-09b).
- ~~Sea of Whispers manifestation~~ — fight lieutenant body (Act 1 boss); leads assassination. ~~Lieutenant display name~~ — **Pneumyra** (`pneumyra`); public epithet **She Who Sings the Undertow** (owner lock 2026-07-22).
- ~~Anál Muir powers / succession shape~~ — literal leader-only + quest boon; trials + champion path + Underflow usurper; person names TBD.

## Design session 2026-07-20 (P0 questions pass) — draft

Source: owner + Dom recording `recording_rsz9trh9_2026-07-20` (Gaming channel, ~18m). Status: **draft locks** mirrored to Dom answered archive. Transcript near-misses (“Mirthirtalia” / “Groc Marin” / “Anal Mirl” / “Atrobi” / “Ledger port”) map to **Muirthalia**, **Grakk-Maren**, **Anál Muir**, **Arrotrebae**, **Ledgeport** (owner confirmed Ledgeport).

| Topic | Draft takeaway | Still open |
| --- | --- | --- |
| Liturgy masks | Dom reconfirmed **Muirthalia** / **Grakk-Maren**; Greek–Irish for Arrotrebae masks | — |
| D-P0-05 geography | Calrenoth keep as documented; deprecate named evergreen wake as Act 1 geography competitor; focus = **Ledgeport** | World coords |
| Ledgeport | Display name **confirmed**; ferry **yes** to **Porto Lucente** U-bay | World coords (D-P2-08) |
| Sea of Whispers body | Fight lieutenant as Act 1 boss; leads assassination plot; **Pneumyra** + epithet **She Who Sings the Undertow** | — |
| Anál Muir powers | Literal; leader-only; temporary quest boon; not player ability | — |
| Succession | Trials to attune; champion good candidate; Underflow usurper | Person names (TBD) |
| Act 1 scope | Dual-path intro (Arrotrebae ↔ Cristallo by travel); light standing | Soft-gate authoring |
| Council | Tribal-leaders council; rite-gated seats; lost Luceran-influenced tribes draft | ~~Decision rule~~ **locked 2026-07-22**; tribe names |
| Sea of Whispers body | Fight lieutenant as Act 1 boss; leads assassination plot; **Pneumyra** + epithet **She Who Sings the Undertow** | — |

## Design session 2026-07-22 (Cristallo + co-op) — draft

Source: owner + Dom recording *World Building: Cristallo and Co-op*, 2026-07-22 (~38m). Status: **draft locks** mirrored to Dom answered archive. Transcript near-misses map per [transcript-canon-alignment](../../.cursor/rules/transcript-canon-alignment.mdc): “Eritrobi” / “aerotrobe” → **Arrotrebae**; “Calor” / “Colinroth” → **Calrenoth**; “Crisel/Crystal” → **Cristallo**; “Fringator” / “Kreatar” / “Krangator” → **Frangitur** / **Creotar**; “Fairy Iron / Farium” → **Nefarium**; “Numira” → **Pneumyra**; “Goldrin” → draft wolf patron name.

| Topic | Draft takeaway | Still open |
| --- | --- | --- |
| Ledgeport → Cristallo ferry | **Yes** — cross channel to **Porto Lucente** (`porto_lucente`) in U-bay | Exact world coords |
| Campsite (DEC-0032 / D-P0-10) | **Dragon Age–style** camp tutorial **post–Act 0**, before Ledgeport hub; companion relationships + storage + rest; **travel-only** (no combat escape); travel **to** camp then **from** camp to destinations | Camp instance asset / DEC-0033 evergreen reconcile in engine |
| Arrotrebae council | **Majority consensus**; criminal trial-by-combat; dissenters **challenge champion** | Council seat tribe list |
| Luceran-lost tribes | **Underflow** Luceran-influenced; **Thalassar** not (Act 1 prevents fall); full tribe roster later | Luceran-lost Arrotrebae **names** |
| Sea of Whispers lieutenant | Siren-bodied chaotic-Imperium aesthetic; **Pneumyra** + **She Who Sings the Undertow** (reconfirmed in session) | — |
| Cristallo faith/politics | Old/New Testament split over Creotar; church/state bipartisan lines; Luceran-puppet Scarlet-Crusade sub-faction | Hierarchy titles; sub-faction name |
| Creator crystal | **White Lotus** guardians; **cleanses Nefarium**; Act 2 Cristallo duty | Crystal name + location |
| Luceran–Shroud | Luceran commands because **Frangitur trapped in Shroud**; Act 3 “free Creotar” = free Frangitur irony | Prison+mantle purpose locked 2026-07-29 (D-P2-14) |
| Black Howl / Shroud history | **Black Howl** held Shroud in first war; **Grul’thaz** (Shadowpaw); corrupted golden→black wolf patron | Wolf god display name |
| Playable races | **Humans + orcs only** (scope) | — |
| Co-op | **Single-player + online 2-player co-op**; mode-locked saves; shared campaign + standing + morality; party ≤4 (solo 1+3 companions; co-op 2+2 companions) | [DEC-0042](../decisions/index.md#dec-0042-online-co-op-session-model-mode-locked-saves) |
