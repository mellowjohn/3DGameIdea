# Prologue and Opening

- Status: developing scene and opening context

## Prologue Monologue

The opening presents Luceran seated in the throne room while Frangitur's whispers gradually become audible. Frangitur taunts Luceran for believing his righteous claim to the Shroud could control its corruption. He claims unique knowledge of Tessera's endless violence and self-destruction, arguing that the freedom he gave Tessera was betrayed and misused.

Frangitur then addresses the adventurers directly. He ridicules Luceran's attempt to withstand the Shroud and insists that saving Tessera requires tearing it apart. His final plea becomes a threat: if the adventurers will not do it, he will.

## Stage Direction

The scene currently proposes:

1. Show the Usurper seated on the throne as the camera slowly pulls back.
2. Bring Frangitur's whispers gradually into focus.
3. Cut to a blood-filled glass panel when Frangitur speaks about himself.
4. When he addresses the adventurers, replace the background with a white silhouette and show the Shroud floating over a gray figure.
5. Ignite the background in flame on the final sentence.
6. Transition directly into character creation.

## Opening flow (locked — D-P0-17 / D-P0-17b)

Player-facing boot sequence:

1. **Prologue cinematic** (A0-01) — Frangitur narration from **inside the Nefarium Shroud** (theatrical narration, not an interactive dialogue tree with Luceran). VO craft = **syllables + intonation** on the **already-provided** Frangitur lines (Twine / this doc intent).
2. **Character creation** (A0-02) — archetype pick with one short **description bubble** + **lore background** per class (strings → D-P0-17c).
3. **Calrenoth cinematic** — continuous-*feeling* establishing pan (WoW-style; **concealed blends OK**); **shot list** D-P0-17d + **camera locks** D-P0-17e — see [`../design/draft_a0_01_03_d_p0_17c_suggestions_2026-08-05.md`](../design/draft_a0_01_03_d_p0_17c_suggestions_2026-08-05.md) §3 / Art Atlas `/storyboards`. **≥30s**, skippable. Title **“Landfall”** on the aerial wide. Focus **approach road + front gate** (not moat/rear drawbridge). Settle on wheelbarrow only — **player discovers Arkand after control**. Audio: backing track + siege ambients; no new narration after Frangitur.
4. **Initial main quest** **Landfall** (`mq_act0_calrenoth`) starts the tutorial.

**Runtime (2026-08-07):** Main Menu **New Game** fades to black, loads cinematic instance `worlds/prologue-throne.world.json` (`presentation: cinematic_instance` — flat plate, no open-map draw), reveals `prologue.uicanvas.json` dialogue chrome (2D stills hidden) over a cathedral-scale graybox throne hall (~36×48 m, ~35 m pointed A-frame vault — wall-to-wall roof plates + transverse bay ribs + sealed end walls; player prefab `prologue_player_scale_ref` for proportion; columns/galleries/pews, Nefarium Shroud stand-in, blood-glass, silhouette), and starts `evt_prologue_throne` look_at pans (**slow approach to throne → zoom Luceran → full hall wide**). Continue/Skip fades back to `main-menu.world.json` → **class glass** → **difficulty** → fades into `worlds/appearance-courtyard.world.json` (expanded pedestal courtyard under appearance chrome — Tessera banners / corner towers / workbench / weapon rack as Blockbench meshes; open north onto grass hills + skyline; pedestal, mannequins, and walls still graybox; 2D backdrop hidden) → Confirm restores menu and parks at `opening.phase=awaiting_landfall`. Project hooks: `opening.prologueWorld` / `opening.appearanceWorld` in `project.engine.json`. WF spoken-line source: `dlg_act0_prologue`. Calrenoth Landfall pan still deferred.

Scale pass (2026-08-10): The live prologue cathedral graybox now reads as roughly 54 by 80 m: widened walls and galleries, seven visible vault-bay rhythms, a deeper entry placement, paired torches through the extended nave, expanded pew rows, and sparse Nefarium watcher / dark-mist dressing. The existing throne-facing cinematic markers were moved to preserve an establishing-wide read.

Provenance: [`../design/recording_a0_01_03_copy_lock_2026-08-05.md`](../design/recording_a0_01_03_copy_lock_2026-08-05.md); camera locks D-P0-17e.

## Inciting Event

Default Act 0 spine is **Calrenoth / Landfall deployment** (D-P0-08). A draft alternate opening (settlement destroyed + Wild God revival) remains **not** the Landfall spine — do not author the boot flow around it.

## Dramatic Irony (locked)

Instruction to destroy / break the Shroud is **partially true** manipulation (D-P2-14): heroes can be steered to mistake the prison for the source of evil and free **Frangitur**. Luceran wears and commands via the binding; the Shroud **contains Frangitur** and **binds** Imperium chaos. Reconciled with Act 0 Creotar vision framing in [campaign-beat-sheet.md](campaign-beat-sheet.md) A0-08.

Twine Act 0 ([`sources/wrathful-conquest-act0.twee`](sources/wrathful-conquest-act0.twee)) has **Creotar** (Creo short form) urge opposing the Shroud / Luceran while Frangitur’s prologue urges ripping Tessera apart — identity locked (Frangitur = fallen Creotar). Honesty/irony **locked** as **partially true** (D-P2-14): oppose misuse is real; free-Luceran / break-to-free-Creotar framing is manipulation — see [campaign-beat-sheet.md](campaign-beat-sheet.md) A0-08 (MVP lock 2026-08-05).

## Open Questions and Conflicts

- Arkand first-meet: **keep Twine wording** until Dom supplies a newer pass (D-P0-17d).
- Pan timing fine-tune OK within D-P0-17e (≥30s, skippable); do not reopen Arkand-reveal / geography / title / blend locks without owner.
