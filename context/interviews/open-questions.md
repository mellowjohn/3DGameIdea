# Open Questions

No question blocks milestones 1 or 2. Re-run the project interview skill before decisions that expand shipping platforms, online services, world scale, mod support, or multiplayer.

**World design (Dom):** open P0/P1/P2 items + Act 0+ **persons / relationships / quests / dialogues** create backlog in [`../design/dom-open-questions.md`](../design/dom-open-questions.md); answered locks in [`../design/dom-answered-questions.md`](../design/dom-answered-questions.md). This file keeps the full cross-cutting engine + story backlog.

## Entity component authoring

Resolved 2026-07-13 as [DEC-0016](../decisions/index.md#dec-0016-entity-attached-components-and-dual-mcp-apply-paths) and [DEC-0017](../decisions/index.md#dec-0017-prefab-and-scene-component-authoring-with-unity-like-inheritance): Add Component on prefab assets and scene entities; Unity-like prefab→instance inherit/override; MCP dual path (dedicated tools + `engine_prefab_apply` / `engine_scene_apply`). Implementation: TICKET-0147. No open blockers for this topic.

## Prefab composition (non-blocking)

These do not block starting v2 compositional prefab work under [DEC-0008](../decisions/index.md#dec-0008-compositional-prefab-meshes-from-primitives):

- Default low-poly segment counts per primitive (`cube`, `pyramid`, `cylinder`, `sphere`)
- Whether nested entity hierarchies inside a prefab should inherit part transforms before world placement
- Whether the editor should offer optional bake-to-glTF export for compositional prefabs

## Story / faction canon (blocks World Forge schema TICKET-0011; soft-blocks mid-campaign beats)

Filed from TICKET-0021 review. Full gap list: [`context/story/factions.md`](../story/factions.md#gaps-blocking-world-forge-schema-ticket-0011-and-mid-campaign-beats). Do not invent answers in schema or beat docs.

- ~~Luceran–Frangitur–Shroud: what grants Chaotic Imperium command, and how much agency Luceran retains?~~ — **draft lock 2026-07-22 / 07-29:** Frangitur trapped in Shroud; Luceran commands binding; Shroud = prison + Imperium mantle; Act 3 “free Creotar” frees Frangitur; Creotar Act 0 guidance **partially true**.
- Cristallo: ~~faith, hierarchy, politics~~ — **partial lock 2026-07-22** (Old/New Testament split, White Lotus, **Claritas** cleanses Nefarium). Still open: hierarchy titles, Claritas court map pin, puppet sub-faction name.
- Arrotrebae: ~~council structure~~ (**draft lock:** tribal-leaders council, rite-passed seats only). ~~Decision rules~~ **locked 2026-07-22:** majority consensus + champion duel. Still open: council seat roster; Luceran-lost tribe **names**. **The Thalassar** (`thalassar`) locked — not Luceran-fallen; **Underflow** Luceran-influenced **coerced ally**.
- **The Thalassar (locked 2026-07-20):** succession shape + powers locked; council seat vs others open. Liturgy mask **Muirthalia**. Minor relic **Anál Muir** (`anal_muir`). False patron **The Sea of Whispers** — [`factions.md`](../story/factions.md#the-sea-of-whispers).
- **The Underflow (locked 2026-07-20):** Act 0 inland orc warband (`underflow`); liturgy mask **Grakk-Maren**; war-chief **Drul’gath** (`drul_gath`); A1 usurper path draft; Act 0 stance **coerced ally** (D-P0-14) — [`factions.md`](../story/factions.md#the-underflow).
- ~~Calrenoth pair public liturgy names (D-P0-04)~~ — **locked:** **Muirthalia** / **Grakk-Maren**.
- ~~**Neutral coastal trade port display name**~~ — **Ledgeport** (`ledgeport`) **confirmed**. ~~Ferry wiring to Cristallo~~ **yes** to **Porto Lucente** (`porto_lucente`) U-bay (2026-07-22); world coords (D-P2-08).
- ~~Exact placement evergreen wake vs Ledgeport vs Calrenoth (D-P0-05)~~ — Calrenoth keep; evergreen wake **deprecated** as competing Act 1 geography; focus = **Ledgeport**. ~~DEC-0032 wake/camp~~ → **DA campsite post–Act 0** locked (D-P0-10, 2026-07-22).
- **Relic hierarchy (draft):** minor tribal relics ≪ Nefarium Shroud confirmed for **Anál Muir**; Cristallo Creator crystal **Claritas** **cleanses Nefarium** (White Lotus; staff-borne by Cristallo leader) — court map pin open.
- Orc warbands: ~~which warband held the Nefarium Shroud~~ — **The Black Howl** (first war); **Grul’thaz** prior bearer (2026-07-22). **The Underflow** = Act 0 minor warband + Luceran-influenced coerced ally.
- Player influence: **model** resolved as [DEC-0029](../decisions/index.md#dec-0029-continuous-faction-standing-with-hostility-transfer). **Numeric thresholds** remain open.
- Kingdom of Tessera: playable faction choice vs political arena around other factions?
- ~~Campaign structure~~ — **resolved** [DEC-0021](../decisions/index.md#dec-0021-soft-gates-with-rare-optional-instances).
- **Faction theaters / Act 0–1 geography (draft 2026-07-20):** Cristallo = central island; Tessera = west; Imperium = south; Calrenoth tip **confirmed**; Act 0 **Landfall** (**final** title); **The Thalassar** + **The Underflow**; Act 1 hub **Ledgeport** (focus region). Exact coordinates, borders, 4×4 km footprint still open — [`official-world-map.md`](../story/official-world-map.md#draft-faction-theaters-2026-07-20).
- Imperium heraldry: dark-crusade / fractured-creator art shipped (`heraldry-chaotic_imperium.png`); owner lock vs revise.
- **Act 1 dual-path intro (locked draft):** Arrotrebae/Thalassar path ↔ Cristallo path by travel; light standing in Act 1.
- **Act 1 assassination / succession (draft locks):** lieutenant boss leads plot; trials + champion path + Underflow usurper; person names **TBD**; quest id + standing rewards open — Dom rows **D-P1-12** / **D-P1-15** / **D-P1-16** in [`dom-open-questions.md`](../design/dom-open-questions.md); beat sheet [`campaign-beat-sheet.md`](../story/campaign-beat-sheet.md).
- **Act 0+ cast / hub authoring:** ~~Underflow war-chief~~ **Drul’gath** locked; ~~Grenge/Larrell/Damius fates~~ survive-by-default (D-P0-12); ~~optional hostage~~ **Larrell** if not saved (D-P0-12b); ~~Asher–Luceran kinship~~ **established** half-brothers (D-P0-13); ~~camp tutorial~~ Arkand-guided (D-P1-19); ~~Creotar honesty~~ **partially true** (D-P2-14); ~~crystal name~~ **Claritas** (D-P2-02b); ~~Underflow stance~~ coerced ally (D-P0-14); ~~House Ashfell name~~ confirmed (D-P1-21); ~~Landfall title~~ **final** (D-P0-15). Leftovers: coords **D-P2-08**, faces **D-P1-21b** / **D-P1-22** in [`dom-open-questions.md`](../design/dom-open-questions.md).

**Update 2026-07-15:** Open-world vs chapter-lock **resolved** as [DEC-0021](../decisions/index.md#dec-0021-soft-gates-with-rare-optional-instances). Beat sheet: [`campaign-beat-sheet.md`](../story/campaign-beat-sheet.md). Remaining story opens from Act 0 Twine:

- ~~Creotar vs Creo/Frangitur~~ — **resolved 2026-07-20:** Creotar = Creo (short); Frangitur = fallen form. ~~Destroy-Shroud guidance~~ — **partially true** (D-P2-14, 2026-07-29).
- ~~Crystal **name**~~ — **Claritas** (D-P2-02b); court map pin open with D-P2-08; **White Lotus** guardians + cleanse behavior locked 2026-07-22
- ~~Wake-up / O’hlundian evergreens vs first hub~~ — first hub = **Ledgeport** (confirmed). Named evergreen wake deprecated; **DA campsite post–Act 0** locked (D-P0-10, 2026-07-22)
- ~~Wild God revival chronology vs Calrenoth Act 0 spine~~ — **resolved** for default spine: Calrenoth / **Landfall** final title (Wild God remains alternate/open)
- Acts 1–4 not yet in Twine (planning beats only)
- Which consequential side quests ([side-quest-catalog.md](../story/side-quest-catalog.md)) can flip faction lock-in vs only nudge standing (blocked on morality thresholds)

## Starting archetypes / lane orgs (DEC-0044)

**Resolved 2026-07-24** as [DEC-0044](../decisions/index.md#dec-0044-starting-archetype-lane-orgs-and-rename): starters are **Ashfell Blade** / **Outrider** / **Runecaster** with home orgs House Ashfell / Outrider Lodge / Runecaster Guild; hybrid bind into major factions; Runecaster ≠ crystal-guardian order.

Still open:

- Combat **systems** and party/build **comps** per lane (readiness `archetype_systems_comps_pass`)
- ~~Confirm/rename House Ashfell~~ — **confirmed** 2026-07-29; name Lodge/Guild/house **faces** (D-P1-21b / D-P1-22)
- First home-org quest seeds (`archetype_lane_org_quests`) — journal **Archetype** tab locked ([DEC-0051](../decisions/index.md#dec-0051-no-xp-power-progression-and-quest-ux)); optional for main story, needed for full lane power; rewards per-quest gear and/or ability
- ~~What an archetype-quest **unlock** grants~~ — **locked:** per-quest gear and/or ability, act-scaled ([DEC-0051](../decisions/index.md#dec-0051-no-xp-power-progression-and-quest-ux))
- Schema `kind: archetype` widen when seeds land
- ~~Outrider + Runecaster kit turnarounds~~ — first-pass sheets in `context/art/reference/` (revise after Blockbench kit pass)
- Advanced specialization lists (post-demo)

## Character power progression (XP / levels)

**Resolved 2026-07-29** as [DEC-0051](../decisions/index.md#dec-0051-no-xp-power-progression-and-quest-ux): **no traditional XP / player level**. Power from gear, boss/act loot bands, archetype quests, and story milestones; abilities also from gear/trinket archetype affinity. Provenance: [`../design/recording_archetype_quests_power_progression_2026-07-29.md`](../design/recording_archetype_quests_power_progression_2026-07-29.md).

Still open (implementation / content, not product fork):

- Exact Act 0 boss presence (session interrupted)
- Numeric act loot-band thresholds / which chapter bosses bump which band
- Schema ship timing for `kind: archetype` enum (journal UX locked; widen when lane-org seeds land)
- HUD chip screen corner polish (top-left draft)
- Minimap marker art for tracked objectives (TICKET-0062)

## Campaign gating (TICKET-0020)

Resolved 2026-07-15 as [DEC-0021](../decisions/index.md#dec-0021-soft-gates-with-rare-optional-instances): soft gates on seamless world + rare optional instances (dungeons/set pieces/visions); minimize load screens. Beat sheet: [`campaign-beat-sheet.md`](../story/campaign-beat-sheet.md).

Still open from beat sheet / Act 0 Twine import:

- ~~Wake-up / camp location after Creotar vision~~ — **DA campsite post–Act 0**; **Arkand**-guided tutorial (D-P1-19).
- ~~Whether Calrenoth remains ruined on the seamless map after Act 0~~ — **resolved** DEC-0032: yes, ruined/impacted revisit.
- ~~Creotar identity vs Creo/Frangitur~~ — **resolved 2026-07-20:** Creotar = Creo; Frangitur = fallen form ([frangitur-the-great-evil.md](../story/frangitur-the-great-evil.md)).
- ~~Wild God revival chronology vs Calrenoth Act 0 spine~~ — **resolved** for default spine: Act 0 Calrenoth / **Landfall** final title (Wild God remains alternate/open).
- Vanessa introduction beat timing.
- Morality thresholds and ending matrix (Act 4) — soft Shroud destroy-vs-control sketch 2026-07-29.
- Twine draft orc names (Grul’thaz / Shadowpaw) — not established until owner review (also TICKET-0021).
- ~~Act 0 supporting cast survival~~ — survive-by-default; optional hostage = **Larrell** if not saved (D-P0-12b).

## UI canvas scale modes (non-blocking)

v1 letterbox/pillarbox is locked in [DEC-0025](../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp). Owner wants a later option to **scale to viewport edges** (no side bars). Open when scheduling that follow-on:

- Per-canvas vs global scale mode (`letterbox` | `fill_edges` / cover / stretch)
- Whether HUD layer and modal screens can differ (edge-flush HUD + letterboxed pause)

## World Forge scope (TICKET-0010)

Resolved 2026-07-15 as [DEC-0019](../decisions/index.md#dec-0019-world-forge-editor-home-and-story-canon-split) + [DEC-0020](../decisions/index.md#dec-0020-world-forge-narrative-tooling-umbrella): editor-home panels; story markdown canon + World Forge JSON keyed to IDs; narrative tooling umbrella (relationship graph/editor; quests/dialogue/story events product-home). Full boundary: [`context/features/world-forge-scope.md`](../features/world-forge-scope.md).

Open (do not invent in TICKET-0011–0014 without owner input):

- Project path layout for World Forge assets (e.g. under `assets/` vs project root) and naming convention.
- POI anchoring model: world-space coordinates only, references to scene entity UUIDs, or both (and which is authoritative for mini-map).
- Whether region polygons/cells must align to 128 m partition cells or may be freeform overlays.
- Story-event schema shape (triggers, conditions, outcomes) and how it binds to beat-sheet IDs from EPIC-0003.
- Relationship edge type vocabulary (minimum set for v1 graph editor).

## Open-world navigation (TICKET-0030)

**Resolved 2026-07-16** as [DEC-0032](../decisions/index.md#dec-0032-open-world-travel-discovery-map-and-dual-soft-gates): carriage-post FT + gold, discovery map fog/dust, dual soft-gate denial, horses-only near-term mounts, Act 1 evergreen wake, ruined Calrenoth revisit, Act 0 spine. See [`open-world-navigation.md`](../features/open-world-navigation.md).

Still open (implementation detail, not product fork):

- Exact gold price curve / distance scaling for carriage FT.
- Schema field names for soft-gate denial mode (`checkpoint` vs `hostile_frontier`) and carriage-post POI kind/tags.
- Horse + 3-companion mounting presentation when mounts ship.

## Player camp (DEC-0033)

**Resolved:** anywhere overland camp → persistent editable camp instance; evergreen story tutorial; **hard deny** when pitching would escape/negate an active combat situation/zone (DEC-0033). See [`open-world-navigation.md`](../features/open-world-navigation.md).

Still open:

- Outside active combat: may the player pitch in **hostile frontiers**, interiors, or story-locked beats?
- How much camp customization ships in v1 (furniture props vs full base-building)?
- Camp rest → time-of-day / ambush rules?
- Exact combat-state signal for deny (party aggro flag vs authored combat volume vs both).

## Map design language (TICKET-0031)

**Mostly resolved** via DEC-0032 (one major hub per act; snow when climate/story fits). Guidance: [`map-design-language.md`](../features/map-design-language.md).

Still open:

- Exact hub settlement names/placements per act (authoring).

**Resolved:** Tessera is the world’s primary land (Middle-earth-scale); Kingdom of Tessera is the polity within it — [DEC-0034](../decisions/index.md#dec-0034-tessera-is-the-worlds-primary-land).

## Authored Rigidbody / physics body (future)

**Resolved (body mode)** 2026-07-17 as [DEC-0038](../decisions/index.md#dec-0038-authored-rigidbody--dynamic-bodies-for-player-and-entities): authored **`rigidbody`** component; player and physics entities use **true dynamic** rigidbodies (forces / friction materials), not CharacterVirtual-as-a-mode. Epic: **EPIC-0015** (TICKET-0196–0199).

Still open for design detail during implementation:

- Default mass / friction / freeze-rotation for the player capsule (0198)
- How jump, grounding, and stairs are expressed on a dynamic body (0198)
- DEC-0030 root-motion retarget onto Rigidbody-backed entities (0199)

## Water and hydrology (DEC-0039)

**Resolved 2026-07-18** as [DEC-0039](../decisions/index.md#dec-0039-water-swim-and-hydrology-authoring): gameplay swim + scripted floating vessels; one world-wide sea level; Sculpt owns water placement/sculpting; World Forge Map owns hydrology layout and ferry routes; fully authored (no noise auto-hydrology); scripted wave motion; deep-water fatigue + damage; reflection/refraction low-poly water; mud/sand shores; bounded sea regions + map-edge fog; dry basins stay dry when sensible; suppress underwater foliage; lava/magic pools deferred. Feature doc: [`water-hydrology.md`](../features/water-hydrology.md).

Still open (implementation tuning, not product forks):

- Shallow vs deep depth thresholds (meters below surface or vs terrain bed).
- Fatigue drain and health damage rates in deep water; link to stamina HUD if needed.
- Wave simulation technique (Gerstner stack vs summed sines vs vertex-only scroll) within “scripted natural motion” constraint.
- World Forge schema field names for hydrology regions and ferry route polylines.
- Water persistence file path and cell schema (mirror terrain 33×33 / 40 m or region-based meshes).
- Whether shallow water allows walking, wading slowdown only, or forced swim.
- Ferry scripted crossing: fade vs visible hull animation duration defaults.

## Inventory / item system (DEC-0048 + DEC-0050)

**Resolved 2026-07-29** as [DEC-0050](../decisions/index.md#dec-0050-inventory-ux-item-kinds-and-positive-soft-affinity) (extends [DEC-0048](../decisions/index.md#dec-0048-terraria-shaped-gearing-with-soft-archetype-affinity)): 8-slot hotbar use model; 4 accessory/trinket slots; slot bag (not weight); stacks 99 / ammo ~1000; per-player camp storage; separate quest inventory; **positive** soft affinity (bonus, not off-lane nerf); data+Lua effects; kind tags + labels; no durability v1; save shape bag/hotbar/equipped/camp + non-slot currencies. Feature: [`../features/gearing-system.md`](../features/gearing-system.md). Recording: [`../design/recording_item_system_2026-07-29.md`](../design/recording_item_system_2026-07-29.md).

**Equip slots (locked 2026-07-29 follow-up):** `head` / `chest` / `legs` + `trinket0`…`trinket3`; stats from armor + trinkets + weapons.

**Bag capacity (locked 2026-07-29 follow-up):** base **20** slots; expandable via craft/loot bag upgrades.

Still open (implementation tune / content polish, not product forks):

- Bag upgrade **steps**, soft **max** capacity, and whether upgrade bags are consumed, equipped, or permanent unlocks.
- Numeric **lane bonus** magnitudes (positive multipliers / which stats).
- Vendor price curves and Ledgeport undermarket catalog (post–Act 0).
- Whether `soldiers_scrap_pouch` opens into scrap rolls vs is a single consumable/material entry.
- Full craft UI loop timing (TICKET-0235 materials stub first).
- Act 0 starter **armor** pieces (if any beyond cloth kits that are appearance-only).

