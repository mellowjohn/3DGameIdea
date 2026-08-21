# TICKET-0283: Masked upper-body overlay layers (block while moving)

- Epic: EPIC-0008
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/3c2d3efc569581ddb4aded8b4e6f3ec0

## Goal

Walk and run while holding Block or swinging the Ashfell light string: Block / Attack / Attack2 / Attack3 drive spine/arms/head, and the existing locomotion blend tree keeps playing on the legs.

## Context links

- Format: [`../../formats/animator-controller-assets.md`](../../formats/animator-controller-assets.md)
- Feature: [`../../features/animator.md`](../../features/animator.md)
- Decision: [DEC-0061](../../decisions/index.md#dec-0061-masked-override-animator-layers-upper-body-overlay), [DEC-0022](../../decisions/index.md#dec-0022-c-animator-backend-with-lua-drive-api)
- Sample: `samples/open-world-rpg/assets/animators/player.animator.json`
- Prior: TICKET-0103 (override layers), TICKET-0268 (3-hit string), TICKET-0282 (locomotion 2D tree)

## Acceptance criteria

- [x] `*.animator.json` layers accept optional `mask.joints[]` + `includeChildren`, optional `weight`, and `motion.type = "none"`; invalid mask joints fail closed (`ANIM-CTRL-MASK-JOINT`).
- [x] Skinning composes later override layers only onto masked joints; unmasked joints keep the base layer (walk/run legs while Block is up).
- [x] Prefab `defaultState` override applies only to the first layer.
- [x] Sample player controller: Block and Attack/Attack2/Attack3 live on `upperBody` overlay; base idle/locomotion continue while guarding or swinging.
- [x] Play-test combo / `hitFrame` / weapon sweep read the overlay layer (not base-only `current_state`).
- [x] Named `animator` suite covers none/mask parse, overlay state machine, overlay melee combo + `hitFrame`, and masked pose compose.
- [x] Docs + DEC-0061; rebuild `engine`.

## Out of scope

- Additive (delta-from-reference) layers
- Dedicated WalkBlock / RunBlock / moving-attack clips
- Per-joint mask weights / editor mask painter

## Dependencies

- Soft: TICKET-0103 done; Block hold clip and Attack 1/2/3 already authored.
- Parallel OK with Animation Studio polish tickets.

## Verification

- Rebuild `engine` + `engine_suite_tests`. Animator suite **480/481** (overlay cases including Attack 1/2/3 combo + hitFrame passed). Remaining fail is pre-existing Attack1/2/3 cancel-window pose, not overlay. Editor + MCP reset after rebuild; build lease released.

## What changed

### Summary

Block is no longer a full-body base state. The animator can overlay a masked layer on top of locomotion, and the sample player controller puts the looping Block hold **and** the Ashfell Attack/Attack2/Attack3 string on `upperBody` (spine/arms/head) so Walk/Run keep the legs. Combo buffering, `hitFrame` probes, and the sword sweep follow that overlay layer.

### Files / surfaces

- Schema/runtime: `animator_controller_asset`, `AnimatorRuntime`, `cpu_skinning` layer compose
- Play-test: Q-block and LMB light string no longer steal the base state; overlay drops during dodge/hit/death/interact; melee wish slowdown removed
- Sample: `player.animator.json` `upperBody` layer
- Docs: DEC-0061, format + animator / gearing / Animation Studio notes, TICKET-0283

### Schema / API / format deltas

- Layer `mask.joints` / `includeChildren`, `weight`
- `motion.type = "none"`
- `ANIM-CTRL-MASK-JOINT`, `ANIM-CTRL-LAYER-WEIGHT`
- `defaultState` override applies to the first layer only
- `crossfade` with empty layer name finds the layer that owns the state (Studio Block/Attack preview)

### Seed / sample data

- `upperBody` overlay: `empty` (none) ↔ `block` (Block clip) and `attack` / `attack2` / `attack3` (light string), mask from Spine/Chest/Neck/Head/shoulders/upper arms with children
- Timeline `hitFrame` events retargeted to `upperBody`

### Tests / verification evidence

- `engine_suite_tests --suite animator`: **480/481** (overlay combo + hitFrame passed). Remaining fail is pre-existing Attack1/2/3 cancel-window pose, not overlay.
- Debug MSBuild `engine` + `engine_suite_tests`

### Decisions & tradeoffs

- Masked **override**, not additive (DEC-0061)
- Overlay ignored for root motion
- Full-body dodge/bow/hit still win: C++ clears `block` and crossfades overlay to `empty`
- Attack hip keys stay on the clip but the mask does not include Hip, so weight-shift lives on locomotion

### Leftover risk / follow-ons

- Desktop QA: F5, hold Q, WASD — legs should walk under the guard; LMB ×3 while walking — swings on the torso, combo still chains
- BowDraw / BowAim / BowRelease reuse the same `upperBody` mask (TICKET-0260 follow-on)
- MagicCast now uses the same overlay (charge backswing → forward thrust on spine/arms; legs stay on locomotion)
- 2026-08-20: leftover base `magicCast` → `fall` transition failed attach (`ANIM-CTRL-TRANSITION-FROM-MISSING`) and left the player in T-pose — removed; sample controller load is now in the animator suite
- Additive layers still deferred

## Agent notes

Owner chose overlay over extra move-and-block clips (2026-08-20), then asked for the same treatment on Attack 1/2/3.
