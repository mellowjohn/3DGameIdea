# TICKET-0101: glTF skeletal/skin data import path

- Epic: EPIC-0008
- Status: done
- Agent: cursor-agent
- Priority: P0
- Notion: https://app.notion.com/p/39ad3efc569581219ea8c8a30375ddb2

## Goal

Extend the existing fastgltf mesh import path to load skeletal/skin data needed for M5 character animation — the engineering critical path before clip formats, blend trees, and root motion.

## Context links

- `context/formats/mesh-assets.md` (triangle path live; skeletal subset documented)
- `context/resources/index.md` (fastgltf 0.9.0 MIT)
- `context/roadmap.md` (M5 remaining: animation state machines, root motion, audio)
- `context/architecture/overview.md` (animation goals)
- Follow-ons: TICKET-0102–0110
- Priority: **P0** — closed; next critical path is TICKET-0102

## Acceptance criteria

- [x] Importer reads glTF skin/joints/weights (documented subset) into engine-owned structures without breaking existing static mesh import.
- [x] Malformed skeletal data returns structured errors; static meshes without skins still import.
- [x] Project validation or named tests cover success + failure cases.
- [x] `mesh-assets.md` (or sibling format doc) documents the skeletal contract and limits.
- [x] Rebuild `engine`; relevant suites pass.

## Out of scope

- Animation clip hot reload (0102), blend trees (0103), root motion (0104), events (0105), IK/retarget (0106), audio (0107).
- Full runtime character animation playback beyond what is required to prove import.
- Production player art / final Squire rig (placeholder skinned glTF is enough for follow-ons).

## Dependencies

- Existing fastgltf triangle importer.
- Unblocks TICKET-0102+.

## Verification

- Rebuild `engine` + `engine_suite_tests` (Debug MSVC); `assets` suite 36/36 passed.
- Embedded-buffer skinned glTF fixtures in `tests/suite_tests.cpp` (success + JOINTS-without-WEIGHTS, out-of-range joint, IBM count mismatch).

## Agent notes

- Implemented `ImportedSkin` / `MeshJointInfluence` on `ImportedMesh`; import path loads skins + optional `JOINTS_0`/`WEIGHTS_0`.
- Static meshes unchanged (`has_skinning()` false).
- 2026-07-10: Implementation complete and verified (`assets` suite 36/36). Status set to `needs-approval`.
- 2026-07-15: Owner approved → `done`. No production player rig required for this ticket; art can land with clip playback work.
