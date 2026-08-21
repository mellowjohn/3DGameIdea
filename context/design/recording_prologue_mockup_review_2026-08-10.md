# Prologue Mockup Review and Scene Feedback

- **Date:** 2026-08-10, 10:10–10:21 EDT
- **Speakers:** John and reviewer (Nick)
- **Duration:** 11m 05s
- **Source:** user-provided transcript, [`Prologue_Mockup_Review_and_Scene_Feedback_2026-08-10.md`](C:/Users/johnr/Downloads/Prologue_Mockup_Review_and_Scene_Feedback_2026-08-10.md)
- **Scope:** Main Menu → Prologue → character-creation/appearance courtyard review. This is usability and art-direction feedback for the current graybox, not a change to the locked Act 0 story spine.

## Recording summary

The reviewer liked the opening flow and the character-creation concept as an early pass, but could not fully judge the prologue camera direction because the present graybox is too dark and spatially flat. They asked for more readable scenery and depth. For appearance creation, they preferred a courtyard/outer-wall setting that visibly connects the new character to the wider world rather than a sealed castle interior.

## Direct feedback → implementation intent

| Review feedback | Adopted intent | Current constraint / follow-up |
| --- | --- | --- |
| The prologue camera advanced through its beats before the dialogue was read. | A camera beat must remain legible as part of the narrated moment: hold or synchronize camera progression with the active narration/Continue state, and retain Skip. | Investigate the event-timeline/UI handoff before changing timing; do not replace the locked narrated prologue with an interactive dialogue tree. |
| “It’s so dark and I can’t see anything.” | Establish a readable focal hierarchy: the Shroud/throne, narrator subject, foreground architecture, and depth cues must be visible in the default prologue shot. | This is a lighting/readability pass, not a final asset or color lock. |
| The scene “feels a little small”; the reviewer wanted “more depth.” | Preserve the cathedral-scale footprint and strengthen depth with layered foreground/midground/background silhouettes and camera compositions that show the hall’s scale. | Exact prop kit, VFX, and final shot framing remain art/level-design work. |
| Character creation should make the player “part of the environment”; the high wall blocks the world. | Treat `appearance-courtyard` as an outer-wall courtyard: lower or open the camera-facing wall/upper lip and frame visible trees/landscape beyond it. | Do not move character creation out of the castle or invent a new location; the outer-wall treatment is sufficient for the present direction. |

## Raw transcript excerpts

- The reviewer said the current scenery was “so dark and I can’t see anything,” and John agreed that lighting should be fixed.
- The reviewer said the cinematic needed “more scenery” before camera direction could be judged, and that the current scene felt small and needed “more depth.”
- On appearance creation, the reviewer suggested reducing the wall in the shot so “you see like trees and stuff,” or treating the space as the castle’s outer wall, because character creation should make the player feel part of the world.

## Alignment

- The locked sequence remains **Main Menu → narrated prologue → character creation → Calrenoth cinematic → Landfall**; see [`../story/prologue-and-opening.md`](../story/prologue-and-opening.md) and D-P0-17 in [`dom-answered-questions.md`](dom-answered-questions.md).
- No new story entity, location, archetype, or World Forge seed was introduced by this recording.
- Exact scenery, lighting values, camera timing, and final art remain implementation work. This feedback should not be read as approval of the current placeholder assets.
