# TICKET-0282: Directional walk-strafe (blendTree2D)

- Epic: EPIC-0008
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/3c2d3efc569581a39893dddd55cdce89

## Goal

Play-test A/D at walk speed reads as true left/right strafe cycles (facing the camera look), driven by a C++ `blendTree2D` on the player locomotion state rather than reusing forward Walk.

## Context links

- Format: [`../../formats/animator-controller-assets.md`](../../formats/animator-controller-assets.md)
- Feature: [`../../features/animator.md`](../../features/animator.md)
- Craft: [`../../art/animation-craft.md`](../../art/animation-craft.md)
- Studio MCP: [`../../features/animation-studio.md`](../../features/animation-studio.md)
- Decision: [DEC-0022](../../decisions/index.md#dec-0022-c-animator-backend-with-lua-drive-api)
- Prior: TICKET-0103 (1D blend trees)

## Acceptance criteria

- [x] `*.animator.json` supports `motion.type = "blendTree2D"` with float `parameterX` / `parameterY` and children with `position: [x, y]` + clip refs; invalid trees fail closed (`ANIM-CTRL-BLEND-*`).
- [x] `AnimatorRuntime` samples freeform 2D trees via triangulation (barycentric inside triangles; closest-edge lerp outside hull); up to 3 weighted clips share state time like 1D.
- [x] Named `animator` suite covers 2D parse, axis/diagonal sample weights, and fail-closed missing-param / too-few-children cases.
- [x] Play-test sets facing-relative `moveX` / `moveZ` (and existing `speed`) from horizontal velocity; player controller locomotion uses a 2D tree with Walk / WalkStrafe / Run / RunStrafe points.
- [x] Studio MCP authors WalkStrafe + RunStrafe overrides (loopable; run lean stronger); Game from behind: A/D at walk and full-speed lateral.
- [x] Docs updated: animator format + feature note + craft note; rebuild `engine`.

## Out of scope

- Dedicated WalkBack / RunBack clips
- Nested blend trees, additive layers, IK
- Changing camera-look facing model
- Root-motion strafes (`applyRootMotion` stays false on player)

## Dependencies

- Soft: TICKET-0103 done.
- Parallel OK with Animation Studio polish tickets.

## Verification

- Rebuild `engine` (Debug MSVC) after Anim Studio edit-buffer fix; lease released.
- `engine validate --project samples/open-world-rpg`: ok.
- Combat-sandbox play-test: MCP `move` wishX ±1 at full lateral; host X travels while Z stays put; screenshots `out/run-strafe-*-*.png` + prior walk `out/strafe-*-verify-*.png`.

## What changed

### Summary

Shipped freeform `blendTree2D`, facing-relative `moveX`/`moveZ`, WalkStrafe + **RunStrafe** lean clips, and fixed Animation Studio stomping MCP `edit_clip` buffers back to Idle every frame.

### Files / surfaces

- `include/engine/assets/animator_controller_asset.h`, `src/assets/animator_controller_asset.cpp` — 2D schema + Delaunay at parse
- `src/animation/animator_runtime.cpp` — barycentric / edge-clamp sample
- `src/rendering/render_app.cpp` — facing-relative move params; sticky edit buffer (no Idle stomp)
- `samples/open-world-rpg/assets/animators/player.animator.json` — locomotion `blendTree2D` including RunStrafe ±1
- `samples/open-world-rpg/assets/models/player.WalkStrafe{Left,Right}.anim.json` — hip X ±0.08
- `samples/open-world-rpg/assets/models/player.RunStrafe{Left,Right}.anim.json` — hip X ±0.12 + Spine roll lean
- Docs: animator format/feature + animation-craft + animation-studio edit-buffer note + findings
- Tests: `tests/suite_tests.cpp` blendTree2D cases

### Schema / API deltas

- `motion.type = "blendTree2D"` with `parameterX`, `parameterY`, children `position: [x,y]`
- Fail-closed: `ANIM-CTRL-BLEND-CHILDREN` (<3), `ANIM-CTRL-BLEND-PARAM*`, `ANIM-CTRL-BLEND-TRIANGULATE`, `ANIM-CTRL-BLEND-POSITION`

### Seed / sample data

- WalkStrafe from Walk (hip ±0.08); RunStrafe from Run (hip ±0.12 + ~10° Spine lean into travel).
- Prefer Studio on **player prefab**; after the stomp fix, `edit_clip` → `upsert_keys` → `save_override` stays on the named clip even while runtime plays Idle.

### Tests / verification evidence

- animator suite 446/447 (one unrelated attack-chain fail) from earlier 0282 pass
- Rebuild `engine`; editor+MCP reset; validate ok
- Play-test combat-sandbox full-speed lateral wishes + screenshots

### Decisions & tradeoffs

- WalkBack still deferred; S reuses Walk
- Run strafe uses stronger lean than walk (readable at speed)
- Authoring: Dom prefers `assets/prefabs/player/player.prefab.json`

### Leftover risk / follow-ons

- Foot plants / arm opposition on strafe cycles still open
- Lean amounts may need Dom eye-pass in Game from behind

## Agent notes

Owner prefers player prefab as Studio subject for animation polish. Yes to run strafe + lean (this pass).
