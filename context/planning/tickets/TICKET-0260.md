# TICKET-0260: Bow draw / aim / release graph + nocked arrow

- Epic: EPIC-0018
- Status: active
- Agent: cursor-agent
- Priority: P2
- Notion: _(mirror when online)_

## Goal

Replace one-shot `bowShoot` with hold-to-draw states (`bowDraw` → `bowAim` → `bowRelease`), play-test LMB for ranged weapons, spawn a nocked arrow on draw and free a visual projectile on release.

## Context links

- Plan / feature: `context/features/bow-draw-aim-release.md`
- Animator format: `context/formats/animator-controller-assets.md`
- Prior held flex: TICKET-0258 / `animation-studio.md`
- Sample controller: `samples/open-world-rpg/assets/animators/player.animator.json`
- Arrow mesh: `assets/models/outrider_arrow.gltf`

## Acceptance criteria

- [ ] Controller has states `bowDraw`, `bowAim`, `bowRelease` wired to `BowDraw` / `BowAim` / `BowRelease` clips + `bowDrawn` bool transitions
- [ ] Timeline events `nockArrow` (draw) and `releaseArrow` (release) authored on the controller
- [ ] Play-test: hotbar `ranged` weapon — LMB hold sets `bowDrawn`, release clears it and plays release
- [ ] Nocked arrow mesh appears while drawn/aiming; release event spawns flying mesh with lifetime
- [ ] Held shortbow `bow_draw` flex tracks draw / aim / release phases
- [ ] Docs: feature note + epics row; restore/not break melee LMB attack path

## Out of scope

- Projectile damage / combat volumes
- Ammo stack consume
- Perfect nock IK / authoring Studio UI for arrow weld
- Upper-body locomotion blend while aiming

## Dependencies

Builds on inventory hotbar tags, held shortbow skin (0258), animation events (0105).

## Verification

- Rebuild `engine`
- `engine validate --project samples/open-world-rpg` (controller loads)
- Desktop: equip shortbow → play test hold LMB (draw + nock) → release (projectile) → walk during aim

## What changed

- Summary: Outrider bow combat is now a **hold-to-draw** graph (`bowDraw` → `bowAim` → `bowRelease`) driven by `bowDrawn`, with timeline events that nock a visual arrow and free a visual projectile on release. Play-test LMB on a `ranged` hotbar weapon toggles draw; melee `attack` is unchanged.
- Files / surfaces: `player.animator.json`; clips `BowDraw`/`BowAim`/`BowRelease`; `render_app.cpp` (input, remaps, nock/projectile draw); docs feature + ticket + epics.
- Schema / API: controller param `bowDrawn` (bool); events `nockArrow` / `releaseArrow` (legacy name `bowRelease` also fires projectiles).
- Seed / sample: sliced override clips from prior BowShoot polish; shortbow flex still `bow_draw`.
- Tests / verification evidence: `engine` Debug MSBuild OK; `engine validate --project samples/open-world-rpg` OK; desktop play-test smoke left for owner.
- Decisions & tradeoffs: v1 projectile is visual-only (no damage volumes/ammo); aim body pose freezes locomotion anim while walk still moves capsule.
- Leftover risk / follow-ons: nock weld rough; string hand offset needs Studio polish; damage + ammo consume; upper-body aim blend.

## Agent notes

Implemented end-to-end vertical slice for TICKET-0260. Rebuild + validate green; editor/MCP restarted; lease released. Owner should equip shortbow in play test and hold/release LMB.
