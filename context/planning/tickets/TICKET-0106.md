# TICKET-0106: IK hooks + retargeting metadata

- Epic: EPIC-0008
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ad3efc569581eea890ea2e6f75f0a1

## Goal

Ship authorable rig metadata (`*.rig.json`) for IK effector hooks and retarget bone-role maps, with C++ load/validate and a sample player rig — no IK solver yet ([DEC-0041](../../decisions/index.md#dec-0041-rig-metadata-before-ik-solver)).

## Context links

- [`../../formats/rig-assets.md`](../../formats/rig-assets.md)
- [`../../formats/mesh-assets.md`](../../formats/mesh-assets.md)
- [`../../formats/character-assets.md`](../../formats/character-assets.md)
- [`../../features/animator.md`](../../features/animator.md)
- Related: TICKET-0101 (skin), TICKET-0105 (events), TICKET-0135 (Animation tools UI), future full IK

## Acceptance criteria

- [x] `*.rig.json` schema v1 documented (`ikHooks`, `boneRoles`).
- [x] `RigAsset` C++ load / save / validate + optional `validate_against_joint_names`.
- [x] Optional `rig` field on `.character.json`.
- [x] Sample `assets/characters/player.rig.json` + player character references it.
- [x] `assets` suite covers round-trip, duplicate rejection, joint mismatch, sample load.
- [x] DEC-0041 recorded; mesh/character/animator context updated.
- [x] Rebuild `engine` / `engine_suite_tests`; Status → needs-approval (not done).

## Out of scope

- Runtime IK solve (owner: eventually yes — separate follow-on)
- GPU skinning / pose playback
- Editor Animation manage panel (TICKET-0135)
- Auto-inferring bone roles from glTF

## Dependencies

Soft: skinned mesh joint names for strict `validate_against_joint_names` (player mesh may still be static).

## Verification

- `engine_suite_tests --suite assets` → **72/72**
- Rebuilt `engine` + `engine_suite_tests` (MSVC Debug)

## What changed

- Summary: New `*.rig.json` / `RigAsset` format stores IK hooks and humanoid bone-role maps; characters can optionally reference a rig. Sample player rig + character binding shipped. No solver.
- Files / surfaces: `rig_asset.h/.cpp`, `character_asset` optional `rig`, sample JSON, format/feature/decision docs, assets suite tests, CMake.
- Schema / API / format deltas: `RigAsset`, errors `RIG-*`; character `rig` field; DEC-0041.
- Seed / sample data: `samples/open-world-rpg/assets/characters/player.rig.json` (humanoid placeholder joint names for future skinned player).
- Tests / verification evidence: assets suite (see build output).
- Decisions & tradeoffs: metadata before solver (DEC-0041); Animation UI deferred to 0135.
- Leftover risk / follow-ons: joint names are aspirational until player skinning; full IK + Animation Diagnostics-adjacent panel.

## Agent notes

Owner chose metadata-only now, full IK later; requested Diagnostics-adjacent Animation manage tab → elevated TICKET-0135 brief (P2).
