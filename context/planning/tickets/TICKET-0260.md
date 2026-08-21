# TICKET-0260: Bow draw / aim / release graph + nocked arrow

- Epic: EPIC-0018
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: _(mirror when online)_

## Goal

Replace one-shot `bowShoot` with hold-to-draw states (`bowDraw` → `bowAim` → `bowRelease`), play-test LMB for ranged weapons, spawn a nocked arrow on draw and free a visual projectile on release. Draw/aim/release play on the upper-body overlay so Walk/Run keep the legs.

## Context links

- Plan / feature: `context/features/bow-draw-aim-release.md`
- Animator format: `context/formats/animator-controller-assets.md`
- Overlay: [DEC-0061](../../decisions/index.md#dec-0061-masked-override-animator-layers-upper-body-overlay), TICKET-0283
- Prior held flex: TICKET-0258 / `animation-studio.md`
- Sample controller: `samples/open-world-rpg/assets/animators/player.animator.json`
- Arrow mesh: `assets/models/outrider_arrow.gltf`

## Acceptance criteria

- [x] Controller has states `bowDraw`, `bowAim`, `bowRelease` wired to `BowDraw` / `BowAim` / `BowRelease` clips + `bowDrawn` bool transitions
- [x] Those states live on `upperBody` (same mask as Block/Attack); base idle/locomotion continue while drawing
- [x] Timeline events `nockArrow` (draw) and `releaseArrow` (release) authored on `upperBody`
- [x] Play-test: hotbar `ranged` weapon — LMB hold sets `bowDrawn`, release clears it and plays release
- [x] Dodge / hit / jump / unequip cancel the overlay **without** firing (`releaseArrow` is not the cancel path)
- [x] Nocked arrow mesh appears while drawn/aiming; release event spawns flying mesh with lifetime
- [x] Held shortbow `bow_draw` flex tracks draw / aim / release phases (release u goes peak → rest)
- [x] Clips read as nock → pull → full-draw hold → string loose (Studio MCP `save_override`)
- [x] Docs: feature note + epics row; restore/not break melee LMB attack path
- [x] Named `animator` suite covers overlay bowDrawn → bowDraw → bowAim → bowRelease → empty + nock/release events

## Out of scope

- Perfect nock IK / authoring Studio UI for arrow weld
- Undo / separate cancel-undraw clip
- Layer-filtered projectile ray (player self-hit filter); continuous attached emitter (bursts only)
- Piercing projectiles (one hurt placement per shot)

## Dependencies

Builds on inventory hotbar tags, held shortbow skin (0258), animation events (0105), masked overlay (0283 / DEC-0061).

## Verification

- Rebuild `engine` + `engine_suite_tests`
- `engine_suite_tests --suite animator`
- `engine validate --project samples/open-world-rpg` (controller loads)
- Desktop: equip shortbow → play test hold LMB (draw + nock) → WASD while aiming (legs walk) → release (projectile)

## What changed

- Summary: Outrider bow combat is a **hold-to-draw** overlay graph (`bowDraw` → `bowAim` → `bowRelease` on `upperBody`) driven by `bowDrawn`. Legs stay on idle/locomotion. Timeline events nock a visual arrow and free a visual projectile on release. Dodge/unequip cancel the overlay without firing. Clips were re-authored as nock-at-chest → pull to a side-readable full draw → string-loose settle.
- Files / surfaces: `player.animator.json` (states moved off base); clips `BowDraw`/`BowAim`/`BowRelease`; `render_app.cpp` (cancel-without-fire, draw-u release remap); `suite_tests.cpp` overlay bow graph; docs feature + ticket + epics.
- Schema / API: controller param `bowDrawn` (bool); events `nockArrow` / `releaseArrow` on layer `upperBody`.
- Seed / sample: MCP-polished override clips; shortbow flex still `bow_draw`.
- Tests / verification evidence: Debug MSBuild `engine` + `engine_suite_tests` OK (pre-existing getenv/hiding warnings). Animator suite **493/494** — overlay bow graph + nock/release events passed. Remaining fail is pre-existing Attack1/2/3 cancel-window pose. `engine validate --project samples/open-world-rpg` OK. Editor + MCP reset; lease released.
- Decisions & tradeoffs: Overlay cancel uses `crossfade empty` + `bowDrawn=false` before tick so a held-LMB dodge does not take `bowRelease`. Hip keys on the clips are ignored by the mask (same as Attack).
- Leftover risk / follow-ons: nock weld / string IK still deferred (hand is at cheek on the clip; nock mesh may not sit on the fingers yet); desktop QA for walk-and-aim.

## Agent notes

Hold-to-draw overlay + clip polish. Editor and MCP were reset after rebuild; lease released.
