# TICKET-0102: Animation clip asset format + hot reload

- Epic: EPIC-0008
- Status: needs-approval
- Agent: cursor-agent
- Priority: P0
- Notion: https://app.notion.com/p/39ad3efc569581f09a1bf5f4cdbbf5d0

## Goal

Define and load an engine-owned animation clip asset format (from glTF animation channels or a compiled intermediate), with hot reload so authored clips update without a full restart. Builds on the skeletal/skin import path from TICKET-0101.

## Context links

- `context/planning/epics.md` (EPIC-0008)
- `context/roadmap.md` (M5)
- `context/formats/animation-clip-assets.md` (contract)
- `context/formats/mesh-assets.md` (skeletal subset)
- `context/architecture/overview.md` (animation goals)
- Prior: TICKET-0101 (done)
- Follow-ons: TICKET-0103 (blend trees), 0104 (root motion), 0110 (M5 exit)

## Acceptance criteria

- [x] Documented clip asset contract (channels, sample rate / keys, joint targeting, limits).
- [x] Importer or loader produces engine-owned clip data from accepted sources without breaking static/skinned mesh import.
- [x] Hot reload path updates clips when source assets change (editor and/or CLI), with structured errors on failure.
- [x] Named tests cover success + malformed clip / missing joint cases.
- [x] Context indexes (`formats/`, `features/`, coverage) updated; rebuild `engine`; relevant suites pass.

## Out of scope

- Blend trees / state machines (0103), root motion (0104), animation events (0105), IK/retarget (0106), audio (0107).
- Final production Squire mesh art (placeholder skinned + clipped glTF is acceptable).
- GPU skinning polish beyond what is required to prove clip load/playback smoke.
- Compiled `.anim.json` intermediate (glTF source is the accepted path for this slice).

## Dependencies

- TICKET-0101 done (skeletal/skin import).
- Blocks useful 0103+ runtime work.

## Verification

- Rebuild `engine` + `engine_core` + `engine_suite_tests` (Debug MSVC).
- `assets` suite: **53/53** passed (clip import, LINEAR sample, CUBICSPLINE/bad-target reject, library hot reload keep-on-failure).
- Embedded fixtures only — no production art.

## Agent notes

- Added `AnimationClip` / `ImportedAnimationSet` + `import_gltf_animation_clips`.
- `AnimationClipLibrary` validate-then-replace hot reload (failed reload keeps prior clips).
- `sample_translation_channel` for CPU LINEAR/STEP tests.
- Docs: `context/formats/animation-clip-assets.md`; features/coverage/roadmap/mesh-assets updated.
- 2026-07-15: Implementation complete — Status → `needs-approval`.
