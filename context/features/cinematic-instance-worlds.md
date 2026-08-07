# Cinematic instance worlds

Status: **active** (thin slice for A0-01 prologue + A0-02 appearance courtyard)

## Goal

Rare, self-contained `.world.json` presentations for theatrical beats that should **not** draw the open-world terrain stream — flat plate + placed graybox (or final art), driven under front-end / menu-preview rules.

This is the thin path toward [DEC-0021](../decisions/index.md#dec-0021-soft-gates-with-rare-optional-instances) optional instances. Full enter/exit return-point streaming (camp, dungeons) remains future work.

## World `presentation`

Optional footer field on `.world.json` (parsed by `Scene`):

| Value | Play / opening | World draw |
| --- | --- | --- |
| `open_world` (default) | Normal play-test | Terrain + foliage + water + placements |
| `menu` | Menu preview (no player) | Full backdrop (e.g. Calrenoth graybox) |
| `cinematic_instance` | Same no-player front-end rules | Void clear + flat ground plate + placements only (`sandbox_stage`) |

`samples/open-world-rpg/worlds/main-menu.world.json` uses `presentation: menu`. Filename `main-menu.world.json` remains a fallback for older documents.

## Opening boot (A0-01 / A0-02)

`project.engine.json`:

```json
"opening": {
  "prologueWorld": "worlds/prologue-throne.world.json",
  "menuWorld": "worlds/main-menu.world.json",
  "appearanceWorld": "worlds/appearance-courtyard.world.json"
}
```

New Game fades to black, loads `prologueWorld`, reveals `prologue.uicanvas.json` chrome (stills hidden), starts `evt_prologue_throne`, then on Continue/Skip restores `menuWorld` and character creation (class → difficulty). **Difficulty Next** fades into `appearanceWorld` under `character_creation_appearance` chrome (2D backdrop/underlay hidden so the 3D courtyard reads). Back restores `menuWorld` + difficulty; Confirm restores `menuWorld` and parks `opening.phase=awaiting_landfall`.

**Prologue camera (DebugCamera look_at):** `evt_prologue_throne` — (1) slow aisle approach toward Luceran/throne, (2) zoom into Luceran, (3) pull out to full cathedral view. Shared look axis + hold beats between shots for clean smoothstep blends. Markers: `prologue_cam_establishing` (pan start), `prologue_cam_close` (zoom end), `prologue_cam_address` (wide end), `prologue_cam_look_target`. **Pitch is look-direction radians** — negative = elevated looking down. Positive pitch is a low angle looking up; large positive pitch + long distance can put the eye under the floor.

**Appearance camera:** frontal elevated establishing pose from `appearance_cam_establishing` → `appearance_cam_look_target` (recruit faces camera; left/center under the glass panel). Runtime uses a tighter ~40° FOV while appearance chrome is up. Prefab torch particles draw on cinematic `sandbox_stage` (not suppressed with water/sky). Graybox matches concept `context/art/concepts/act0-a0-02-character-creation.png` and production sheets `act0-ld-a0-02-creation-perspective.png` / `act0-sceneset-a0-02-character-creation.png` / `act0-appearance-courtyard-kit-concept.png` (battlement courtyard, class mannequins, kit clutter, left-wall torch particles, skyline towers).

**Prologue art refs:** LD `act0-ld-a0-01-prologue-perspective.png`, sceneset `act0-sceneset-a0-01-prologue.png`, modular `act0-prologue-cathedral-kit-concept.png` (+ Tier 6 throne/Shroud). Legacy carousel stills remain under `act0-prologue-0*.png`.

`EventTimelineRuntime` ticks whenever a sequence is **active**, including New Game opening / `menu_preview` with no play-test session (not only during Play). `apply_opening_cinematic_camera` blends active `look_at` directives and **holds** the last eye between wait beats / after unlock while prologue chrome is up (does not re-snap establishing each frame). Appearance uses marker snap/hold (no timeline yet).

**Verified (2026-08-07):** Graybox throne under dialogue on flat plate; Skip restores main-menu + class selection. Camera markers / `evt_prologue_throne` retuned for cathedral hall scale (pan → zoom → wide). Timeline tick outside play-test fixed same day so pans actually run. Appearance courtyard graybox + difficulty→appearance world swap wired same day.

**Appearance polish (2026-08-07):** `appearance_player_preview` planted on pedestal at Y≈1.22 (player mesh body offset −0.09; pedestal lip top ≈0.80). Look-target camera marker scaled near-zero so the cyan gizmo does not read in Game view. Appearance canvas rebuilt with opaque gothic panel well (`creation-glass-appearance-panel-v2.png`), Ashfell medallion, readable cycle rows, soft courtyard dim; 2D backdrop/underlay stay hidden. Menu/Game view skins the recruit through `AnimatorRuntime` idle (no play-test required) so the preview is not stuck in bind pose.

## Related

- Story: [`../story/prologue-and-opening.md`](../story/prologue-and-opening.md)
- Format: [`../formats/world-placement.md`](../formats/world-placement.md)
- Animation Studio flat plate (editor-only, not a world): [`animation-studio.md`](animation-studio.md)
