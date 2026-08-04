# TICKET-0258: Skinned held weapons + bow draw sync

- Epic: EPIC-0019
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: _(mirror when online)_

## Goal

Preserve the Outrider shortbow armature + `bow_draw` through bake, GPU-skin the held weapon under the existing handAttach weld, and sample `bow_draw` from player BowShoot / Animation Studio scrub so limbs flex through pullback → aim → release.

## Context links

- Plan: skinned bow draw for BowShoot
- `context/formats/mesh-assets.md` — shortbow skinned bake note
- `context/features/animation-studio.md` — BowShoot + held draw
- `tools/bake_tier1_props_gltf.py` — `preserve_skin` opt-in
- `tools/bake_generic_gltf.py` — skinned normalize path
- Prior: TICKET-0250/0251 held weld; player `BowShoot` override + `shoot` trigger

## Acceptance criteria

- [x] Named bakе `outrider_shortbow` only keeps skins, JOINTS/WEIGHTS, and clip `bow_draw` (no other Tier-1 props changed)
- [x] Held weapon with `has_skinning()` uploads a bone slot and draws with `skin_entity_id` under the weld root
- [x] While player/studio state/clip is BowShoot/`bowShoot`, weapon sample time tracks remapped scrub progress; otherwise rest (`t=0`)
- [x] Item optional `handAttach.drawClip` (default/convention `bow_draw` for shortbow)
- [x] Suites: inventory `drawClip`; animator held-skin sample + import smoke for shortbow matrices
- [x] Docs updated (mesh-assets, blockbench list, animation-studio)

## Out of scope

- Two-handed / string IK
- Editing bow joints with Studio bone gizmo
- Arrow nock mesh attach
- Auto-route LMB `attack` → `shoot` when bow equipped

## Dependencies

Builds on Animation Studio held mesh + GPU skinning + player BowShoot clip.

## Verification

- Rebake: `python tools/bake_tier1_props_gltf.py outrider_shortbow` → joints=11, `bow_draw`; `asset_bake --target outrider_shortbow` all gates ok (SKIN-MISSING, CLIP-*, COLLATERAL)
- Rebuild `engine` under lease (TICKET-0258); editor/MCP process reset + lease released
- Suites: `animator` 399/399; `inventory` 55/55
- Manual: Animation tab → held shortbow → `bowShoot` scrub (limbs flex with draw progressive)

## What changed

- Summary: Shortbow bake no longer collapses to static mesh. Held shortbow GPU-skins `bow_draw` synchronized to BowShoot phases; grip still authored via handAttach.
- Files / surfaces: bake (`preserve_skin`), catalog kind skinned, item `drawClip`, `cpu_skinning` sagittal opt-out for weapons, `render_app` held bone-slot upload + draw, suite/docs/ticket.
- Schema / API: `ItemHandAttach.draw_clip` ← JSON `drawClip`; `sample_skinned_local_poses(..., apply_sagittal_handedness=true)`.
- Seed / sample: `outrider_shortbow.gltf` skinned + clip; shortbow item `drawClip: bow_draw`.
- Tests / verification evidence: animator 399/399; inventory 55/55; asset-bake verify ok.
- Decisions & tradeoffs: reuse generic skinned baker from Tier-1 opt-in; piecewise time remap on 1.7s BowShoot reference; no new major item schema beyond `drawClip`.
- Leftover risk / follow-ons: retune arm keys vs limb flex after live studio scrub; projectile/nock; combat shoot routing.

## Agent notes

Implemented from approved plan; plan file not edited.
