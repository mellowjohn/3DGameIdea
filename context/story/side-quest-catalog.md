# Side-Quest Catalog

- Status: developing story context
- Ticket: [TICKET-0022](../planning/tickets/TICKET-0022.md)
- Related: [campaign-beat-sheet.md](campaign-beat-sheet.md), [factions.md](factions.md), [story-vision.md](story-vision.md)

Content planning only. **Runtime quest schema** (`quests.worldforge.json`, TICKET-0050 / [DEC-0026](../decisions/index.md#dec-0026-quest-owned-dialogue-hooks-multi-stage)) seeds from this catalog. Quest creator UI and dialogue runtime remain later tickets (0051–0053).

Labels: **draft** (working), **proposal** (design intent), **open** (unresolved). No entry is established canon until owner review.

## Design rules

| Rule | Intent |
| --- | --- |
| Lean starter | 12 IDs (`SQ-01`–`SQ-12`); expand with `SQ-13+` later |
| Substance over stubs | Every quest has **objective steps**, **named outcome flags**, **concrete rewards**, and **stated mainline/faction impact** — not vibe-only hooks |
| Optional start | Side quests never hard-gate entering the next act |
| Consequential forks | Marked quests **do** change later main-story course and/or allegiance track via stored flags |
| Tone | Dark fantasy core + goofy contrast ([story-vision.md](story-vision.md)) |
| World model | Soft gates default; rare instances OK ([DEC-0021](../decisions/index.md#dec-0021-soft-gates-with-rare-optional-instances)) |
| Faction tags | Draft where [TICKET-0021](../planning/tickets/TICKET-0021.md) gaps remain |

### Entry checklist (required fields)

Each quest must answer:

1. **When / where it starts** (region + unlock condition)
2. **What the player must do** (ordered objectives)
3. **Choice fork** (at least one meaningful branch with mutually exclusive flags)
4. **Impact** — main story beat(s) affected + faction/morality effect
5. **Rewards** — item/standing/unlock named specifically enough to implement later
6. **If ignored** — what the world does (or does not) change

### Consequential model (**proposal**)

Consequential quests write story flags consumed by Act 2+ beats and future World Forge standing. Magnitude of standing numbers remains **open** until morality/allegiance rules exist; **direction** of the fork is fixed here.

Local-color quests (`consequential: no`) do not move allegiance tracks.

---

## Catalog

### SQ-01 — Cart Again

| Field | Value |
| --- | --- |
| Status | draft |
| Consequential | no |
| Starts | After Ledgeport unlock; farmland road east of Ledgeport. Arkand in party **or** available at camp. |
| Objectives | 1) Find Guard **Pellin** pinned under an overturned supply cart. 2) Clear 2–3 Imperium scavengers. 3) Lift cart (strength check **or** lever props). 4) Escort Pellin to hub gate. |
| Fork | **Help gladly** → `sq01.arkand_pride`; **Mock him / make Arkand do it** → `sq01.arkand_embarrassed` (dialogue only). |
| Mainline impact | None. Optional Arkand banter in later camp scenes. |
| Faction / morality | None. |
| Rewards | Rusted King’s Guard buckle (junk/sell); small coin; Arkand approval line. |
| If ignored | Pellin’s cart remains as world clutter; no flag. |

### SQ-02 — Signal Fire Debt

| Field | Value |
| --- | --- |
| Status | draft |
| Consequential | **yes** |
| Starts | Hub notice board **or** refugee **Mara of Rinos** after Act 1 unlock. Region: hill watchtower overlooking the Rinos approach. |
| Objectives | 1) Climb watchtower; clear nest of Imperium scouts. 2) Inspect signal pyre and Mara’s sealed orders. 3) Choose how to light/use the fire. 4) Report back to Mara **or** Grenge remnant if present. |
| Fork | **Light true distress** → `sq02.fire_true` (Kingdom aid path). **Light false all-clear** → `sq02.fire_lie` (Imperium advances unnoticed). **Ignite as bait then ambush** → `sq02.fire_bait` (loot + cruelty). |
| Mainline impact | Act 2 road travel near Rinos: `fire_true` soft-opens a Kingdom supply cache POI; `fire_lie` spawns an Imperium checkpoint soft-gate; `fire_bait` adds hostile deserter encounter + darker Grenge dialogue if he lives. |
| Faction / morality | True → Kingdom+, morality light+. Lie → Imperium pressure+, morality−. Bait → morality−−, Cristallo cold if overheard later. |
| Rewards | Spyglass (tool); Mara’s map scrap (marks one soft-gated hill path); standing per fork. |
| If ignored | Default world uses neutral “no signal” state — Imperium checkpoint still appears later but without player blame flag. |

### SQ-03 — Green Shroud’s Ledger

| Field | Value |
| --- | --- |
| Status | draft |
| Consequential | **yes** |
| Starts | Explore Calrenoth fringe ruins after Act 1; interact with Grenge’s scorched command table. |
| Objectives | 1) Recover **Grenge’s Ledger** (unique item). 2) Read entries: Rinos failure, Asher’s refused battalion, Larrell rear orders. 3) Deliver outcome before Act 2 Cristallo contact. |
| Fork | **Return to Grenge** (if alive) / Larrell → `sq03.ledger_loyal`. **Sell to Cristallo courier** → `sq03.ledger_sold`. **Burn** → `sq03.ledger_burned`. **Give to Imperium agent** (hidden) → `sq03.ledger_treason`. |
| Mainline impact | Act 2 Kingdom politics scene: loyal → Grenge/Larrell vouch for player; sold → Cristallo gains blackmail leverage on Asher narrative; burned → no vouch, trust vacuum; treason → Imperium ambush uses ledger intel against hub. |
| Faction / morality | Loyal: Kingdom++. Sold: Cristallo+, Kingdom−. Burned: morality mixed. Treason: Imperium+, morality−−, Kingdom lockout risk. |
| Rewards | Loyal: Grenge’s green-thread cloak (light armor). Sold: heavy coin + Cristallo invitation (starts SQ-06 early). Burned: nothing material. Treason: Nefarium-tainted coin (quest item / corruption flavor). |
| If ignored | Ledger stays in ruins until Act 3; auto-`sq03.ledger_lost` — neither vouch nor blackmail. |

### SQ-04 — Larrell’s Muster

| Field | Value |
| --- | --- |
| Status | draft |
| Consequential | **yes** |
| Starts | Hub refugee camp; branches on Act 0 Larrell survival flag from beat A0-07. |
| Objectives | **If Larrell alive:** 1) Recruit 3 named survivors (miller, archer, medic). 2) Defend camp once during night raid. 3) Assign them to watch **or** send to Grenge. **If Larrell dead:** 1) Find her broken signal horn. 2) Choose memorial / cover-up / blame Grenge. 3) Speak to refugees. |
| Fork | Alive + send to Grenge → `sq04.muster_front`. Alive + keep at hub → `sq04.muster_hub`. Dead + memorial → `sq04.larrell_honored`. Dead + cover-up → `sq04.larrell_buried_lie`. Dead + blame Grenge → `sq04.larrell_blame_grenge`. |
| Mainline impact | `muster_front` → Grenge has extra soldiers in one Act 2 skirmish (ally presence). `muster_hub` → hub defense succeeds automatically once; Vanessa intro (if any) safer. Blame Grenge → Grenge refuses aid in SQ-03 loyal path / Act 2. Honored → Arkand loyalty+. |
| Faction / morality | Front/hub: Kingdom+. Blame: morality− vs Grenge, Kingdom split. Cover-up: morality−. |
| Rewards | Alive path: Larrell’s rear-guard whistle (summon one ally once per combat — design sketch). Dead path: horn trophy + standing per fork. |
| If ignored | Hub remains under-defended; first Imperium raid on hub deals refugee casualties (world state), no player credit. |

### SQ-05 — Grove Tithe

| Field | Value |
| --- | --- |
| Status | draft |
| Consequential | **yes** |
| Starts | Road into Rampant Wilds fringe; village **mill board** posts “tithe collectors in the grove.” Unlock after Act 1 open-world. |
| Objectives | 1) Meet chieftain’s envoy **Brannoc** (draft name). 2) Inspect disputed grain store. 3) Resolve: pay tithe from player coin, force village to pay, fight grove wardens, **or** broker split. 4) Return with sealed agreement **or** trophy. |
| Fork | Broker → `sq05.broker`. Side grove → `sq05.arrotrebae`. Crush grove → `sq05.crush_wild`. Squeeze village for Cristallo favor → `sq05.cristallo_lean`. |
| Mainline impact | Act 2 woodland approach: broker/arrotrebae → soft-open Arrotrebae camp without combat; crush → hostile wilds patrols + harder Vanessa woodland path; cristallo_lean → Cristallo banquet (SQ-06) treats player as useful enforcer. |
| Faction / morality | Directly biases **Arrotrebae vs Cristallo allegiance track** (proposal). Broker = mild both. |
| Rewards | Broker: wilder-honey (consumable). Grove: tribal token (standing). Crush: warden spear. Cristallo lean: sealed letter of introduction. |
| If ignored | Default: grove blockade soft-gates one forest path until Act 2 forced confrontation on main road. |

### SQ-06 — Oligarch’s Banquet

| Field | Value |
| --- | --- |
| Status | draft |
| Consequential | **yes** |
| Starts | Cristallo estate invitation after SQ-03 sold **or** SQ-05 cristallo_lean **or** hub priest after Act 1. Region: refined town estate. |
| Objectives | 1) Attend feast; speak to host **Lady Vesperine** (draft). 2) Complete one table challenge (toast / duel of manners / information trade). 3) Optional: steal ledger page from study. 4) Exit with standing result. |
| Fork | Impress → `sq06.favored`. Insult → `sq06.snubbed`. Spy success → `sq06.spy`. Spy fail → `sq06.caught`. |
| Mainline impact | `favored` → Vanessa can be introduced here as Cristallo-adjacent mage (beats A1-04). `snubbed` → Vanessa intro moves to road encounter instead; Cristallo soft-gates a city quarter. `spy` → unlocks Act 2 blackmail beat vs oligarch; `caught` → bounty + harder Cristallo doors. |
| Faction / morality | Favored/spy: Cristallo++ (spy also morality−). Snubbed: Cristallo−−, slight Arrotrebae curiosity later. |
| Rewards | Favored: silvered circlet (light). Spy: oligarch ledger page (quest item). Snubbed: nothing. Caught: fine or jail-escape combat. |
| If ignored | Vanessa intro defaults to road beat; Cristallo standing stays neutral. |

### SQ-07 — Shadowpaw Scraps

| Field | Value |
| --- | --- |
| Status | draft |
| Consequential | **yes** |
| Starts | Chaotic battlefield cell; smoke signal from orc deserters. Flavor names **Shadowpaw** / **Grul’thaz** are draft until 0021 locks warband IDs. |
| Objectives | 1) Parley with warband remnant leader **Kerra** (draft). 2) Retrieve a stolen Imperium supply crate **or** assassinate their scout. 3) Choose alliance, wipe, or sell location to Imperium. |
| Fork | Ally → `sq07.orc_ally`. Wipe → `sq07.orc_dead`. Sell out → `sq07.orc_betrayed`. |
| Mainline impact | Ally → optional orc auxiliaries in one Act 2 Imperium skirmish; opens warband camp POI. Betrayed → Imperium tip soft-opens a dungeon instance (Nefarium cache) but orc ambush later. Dead → loot now, no auxiliaries, Vanessa disapproves if present. |
| Faction / morality | Ally: orc standing+, morality mixed. Betrayed: Imperium+, morality−−. Wipe: morality−. |
| Rewards | Ally: warband totem. Wipe: heavy loot. Betrayed: Imperium pass-token (one checkpoint). |
| If ignored | Deserters leave; no auxiliaries; Nefarium cache stays hard-gated until Act 3. |

### SQ-08 — Fog That Walks

| Field | Value |
| --- | --- |
| Status | draft |
| Consequential | **yes** |
| Starts | Corrupted prairie farm; family **Holm** pays for help after first hub night. |
| Objectives | 1) Survive fog event (timed combat vs ethereal Imperium spirits). 2) Find the buried **binding nail** under the barn. 3) Choose: destroy nail, keep nail, or drive nail into Holm elder (weaponize farm). |
| Fork | Destroy → `sq08.cleanse`. Keep → `sq08.nail_kept`. Weaponize → `sq08.weaponize`. |
| Mainline impact | Cleanse → prairie travel safe; morality+. Keep → player can use nail once in Act 2 boss-lite to stun ethereal foe (item). Weaponize → farm becomes Imperium-aligned haunt; Holm family gone; Act 2 “help the farms” beat fails / darker comedy. |
| Faction / morality | Cleanse: anti-Imperium. Weaponize: Imperium-adjacent +, morality−−. Keep: temptation flag for later corruption beat. |
| Rewards | Cleanse: blessed sickle. Keep: Binding Nail (unique). Weaponize: shadow ash (crafting) + curse debuff flavor. |
| If ignored | Farm remains haunted; blocks one prairie shortcut; Holm family disappears by Act 2 (world state). |

### SQ-09 — Crystal Rumor

| Field | Value |
| --- | --- |
| Status | draft |
| Consequential | **yes** |
| Starts | Hub bazaar; merchant **Jask** hawks “Creotar shards” after Creotar vision (A0-08) known to player. |
| Objectives | 1) Buy, steal, or intimidate a sample shard. 2) Have it appraised by hub smith **or** Vanessa if present. 3) Follow the real lead to a cave shrine **or** expose the scam ring. |
| Fork | Real shard path → `sq09.crystal_true` (cave shrine instance/soft POI). Scam exposed → `sq09.scam_out`. Player joins scam → `sq09.scam_partner`. |
| Mainline impact | `crystal_true` → redirects Act 2 crystal investigation to **known shrine POI** (fills Twine empty “location” stub with a concrete beat). `scam_out` → no shrine; Creotar thread stalls until main beat. `scam_partner` → coin now; later Frangitur-tinged dream callback (irony). |
| Faction / morality | True: Creotar-quest progress. Partner: morality−, Frangitur irony flag. Out: hub law+. |
| Rewards | True: shrine key-stone + lore. Out: Jask’s purse + town favor. Partner: cut of scam gold. |
| If ignored | Crystal thread waits on main-story Creotar follow-up only; no shrine shortcut. |

### SQ-10 — Island Ferry

| Field | Value |
| --- | --- |
| Status | draft |
| Consequential | **yes** (travel + one story POI) |
| Starts | Docks after Act 1; ferryman **Old Noll** refuses to sail. |
| Objectives | 1) Investigate pier: kill/bind water-shadows (small combat). 2) Recover Noll’s missing lantern from the boathouse. 3) Optional: dive to sunken crate. 4) Sail to **Islet of Cinders** (draft POI name). |
| Fork | Clear and sail → `sq10.ferry_open`. Steal boat without helping → `sq10.ferry_stolen`. Pay Noll to risk it without clearing → `sq10.ferry_bribe` (ambush mid-crossing). |
| Mainline impact | `ferry_open` → permanent soft travel to islet; islet holds a **Shroud-related mural** used in Act 2 exposition. Stolen → islet accessible once but Noll hostile; no return ferry. Bribe → survival combat; ferry opens if you live. |
| Faction / morality | Steal: morality−. Clear: hub+. |
| Rewards | Open: ferry access + mural lore flag `sq10.mural_seen`. Sunken crate: waterproof satchel (inventory). |
| If ignored | Islet unreachable until a later main beat boats the player there once. |

### SQ-11 — Mountain Shrine

| Field | Value |
| --- | --- |
| Status | draft |
| Consequential | **yes** |
| Starts | Mountain path soft-gate after Act 1; pilgrims argue at ruined shrine to Creo. |
| Objectives | 1) Hear both pilgrims (Creo-loyal vs Frangitur-apologist). 2) Retrieve the fallen **keystone idol** from a cave troll nest (small instance OK). 3) Place idol for Creo, smash for Frangitur, or keep. |
| Fork | Place Creo → `sq11.creo`. Smash → `sq11.frangitur`. Keep → `sq11.idol_kept`. |
| Mainline impact | `creo` → Cristallo faith door opens earlier; Acolyte players get unique dialogue. `frangitur` → dark shrine event; Creotar later distrusts player. `idol_kept` → can gift to Cristallo (SQ-06) or sell to Imperium agent. |
| Faction / morality | Creo: Cristallo+/faith+. Frangitur: morality−−, Imperium whisper+. Keep: flexible. |
| Rewards | Creo: pilgrim charm. Frangitur: cracked idol ash. Keep: Keystone Idol (quest item). |
| If ignored | Mountain path stays soft-gated; no faith standing change. |

### SQ-12 — Rak Zulla’s Mushroom Errand *(easter egg)*

| Field | Value |
| --- | --- |
| Status | draft / proposal |
| Consequential | no (allegiance); optional one-line callback **open** |
| Starts | Hidden mushroom ring in weird grove (off-path, no quest marker). Interact → meet genie **Rak Zulla**. |
| Objectives | 1) Eat (or refuse) offered mushrooms — refuse ends quest with taunt. 2) Hallucination sequence: chase **carpenter otters** across three landmark props (they “build” bridges of nonsense). Optional gag beat: a **lion gecko** cameo (prop / hallucination only — not a companion or cast character). 3) Confront **two cave trolls** arguing over a tiny hammer. 4) Return hammer to otters **or** give to Rak Zulla. 5) Wake at ring; loot chest. |
| Fork | Help otters → `easter.rak_zulla.otters`. Help Rak Zulla keep hammer → `easter.rak_zulla.genie`. Refuse mushrooms → `easter.rak_zulla.refused`. |
| Mainline impact | None required. **Proposal:** at most one Arkand/Vanessa joke line if `easter.rak_zulla.*` set. |
| Faction / morality | None. |
| Rewards | Otters: “Otter Union Card” (cosmetic junk). Genie: “Slightly Used Wish Stub” (joke item). Both paths: brag flag. |
| If ignored | Ring never triggers; no world change. |

Tone: isolated comedy pocket — must not rewrite Frangitur/Shroud lore. **Lion gecko** is easter-egg flavor only (see [companions.md](companions.md)); do not promote it to a character concept.

---

## Consequential summary (quick ref)

| ID | Mainline redirect | Allegiance / standing bias |
| --- | --- | --- |
| SQ-02 | Rinos road checkpoint vs cache | Kingdom vs Imperium pressure |
| SQ-03 | Vouch / blackmail / ambush intel | Kingdom / Cristallo / Imperium |
| SQ-04 | Ally presence in Act 2 skirmish | Kingdom; Grenge trust |
| SQ-05 | Wilds approach open vs hostile | **Arrotrebae ↔ Cristallo track** |
| SQ-06 | Vanessa intro venue; city soft-gate | Cristallo |
| SQ-07 | Auxiliaries vs Nefarium dungeon tip | Orc warband / Imperium |
| SQ-08 | Prairie shortcut; corruption item | Anti- vs pro-Imperium temptation |
| SQ-09 | Crystal shrine POI vs stalled thread | Creotar quest path |
| SQ-10 | Islet mural exposition | — |
| SQ-11 | Faith door / Creotar trust | Cristallo faith vs Frangitur |

---

## Coverage checklist (story-vision location types)

| Type | Covered by |
| --- | --- |
| Farmland / prairie | SQ-01, SQ-08 |
| Hills / mountains | SQ-02, SQ-11 |
| Woods / grove | SQ-05, SQ-12 |
| Village / town / estate | SQ-04, SQ-06 |
| Market / bazaar | SQ-09 |
| Castle / fort fringe | SQ-03 |
| Battlefield / chaotic | SQ-07 |
| Docks / island | SQ-10 |

## ID conventions

- Side quests: `SQ-##`
- Outcome flags: `sq##.*` or `easter.*`
- Do not reuse IDs

## Open questions

- Numeric standing / morality thresholds (blocked on story-vision rules).
- Lock final warband / Cristallo NPC names when 0021 resolves (Brannoc, Kerra, Vesperine are placeholders).
- ~~First hub name~~ — **Ledgeport**; islet / mountain path final geographic names still open.
- Whether SQ-12 gets any serious-world callback.

Also: [`context/interviews/open-questions.md`](../interviews/open-questions.md).
