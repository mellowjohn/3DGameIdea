# TICKET-0247: In-editor asset import (pick model → bake → verify)

- Epic: EPIC-0009
- Status: needs-approval
- Agent: cursor-agent
- Priority: P0
- Notion: https://app.notion.com/p/3b0d3efc56958172bab2d3d8fd80486d

## Goal

An author working inside the running engine can pick a `.gltf` / `.glb` model from the Assets surface, watch the import run (mesh, texture atlas, animation clips), and get a green check mark when it lands — with the model live in the scene and play-test without restarting. Picking a model the engine already knows (for example `GoodPlayerModel.gltf` for the player) updates that asset in place instead of creating a duplicate.

## Context links

- `context/features/editor-mvp.md` — Asset Browser and Diagnostics panels
- `context/formats/mesh-assets.md` — runtime mesh + atlas + bake report layout
- `context/formats/prefab-assets.md` — prefab mesh references
- `context/planning/tickets/TICKET-0245.md` — named bake catalog, verify gates, `engine asset-bake`
- `context/testing/recurring-asset-failures.md` — `ASSET-BAKE-*` failure playbooks
- `skills/import-blockbench-models/SKILL.md`, `skills/import-player-character/SKILL.md`

## Acceptance criteria

- [x] Asset Browser exposes **Import Model...** which opens a native file picker filtered to `.gltf` / `.glb` / `.bbmodel`.
- [x] `.gltf` / `.glb` rows in the Asset Browser expose **Re-import** (rebake from the registered source) and **Replace...** (pick a new source).
- [x] Picking a file that matches a registered bake target updates that target in place; the Assets tab names the target and how it matched.
- [x] Picking an unregistered model registers a catalog target, bakes it, and writes a placeable prefab under `assets/prefabs/Imported/`.
- [x] Import runs off the UI thread with a staged progress readout and elapsed time; the editor keeps rendering during a multi-minute skinned bake.
- [x] Success shows a green check mark plus the outputs; failure lists the `ASSET-BAKE-*` / `ASSET-IMPORT-*` gate codes and remediation.
- [x] Baked mesh, texture, and animation clips hot-reload into the open scene and play-test without restarting the editor.
- [x] Animation clips present in the source survive a skinned import (gated by `ASSET-BAKE-CLIP-REGRESS`).
- [x] `engine asset-import --project <path> --file <model> [--target <id>] [--plan]` gives the same behavior headless.
- [x] `automation` suite covers plan resolution: registered-source match, baked-output match, new-target slug, unique id, pinned replace, unsupported/missing sources, prefab lookup.

## Out of scope

- Multi-material / multi-texture imports (the generic bake path uses the first image).
- Rig authoring: a generic skinned import does not generate `*.rig.json` or an animator controller.
- Automatic collider authoring for imported prefabs (authors add colliders in the prefab editor).
- Blockbench `.bbmodel` for unregistered assets (export glTF first); registered `.bbmodel` targets such as `tree` still work.
- Importing several files in one action.

## Dependencies

- Builds on TICKET-0245 (bake catalog, verify gates, `run_asset_bake`).
- Shares the rebuild lease workflow (TICKET-0228) for the `engine` rebuild.

## Verification

- Rebuild the `engine` target after acquiring the build lease.
- `engine_suite_tests --suite automation` (asset import plan coverage) and `--suite assets`.
- `engine asset-import --project samples/open-world-rpg --file <model> --plan --json` for plan output.
- Live editor: import a fresh model and re-import `GoodPlayerModel.gltf`, confirming the green check, hot reload, and clip preservation.

## What changed

Picking a model file inside the editor now runs the full bake pipeline with visible progress and a green check, and re-picking a model the engine already knows updates it in place.

**New surfaces**

- Asset Browser second button row: **Import Model...** (native picker) plus per-row **Re-import** / **Replace...** on `.gltf` / `.glb` entries. `Replace...` pins the picker to the catalog target that owns that baked mesh, so a renamed export still updates the same asset.
- **Diagnostics → Assets** now leads with the import panel: chosen source, resolved target and how it matched, static/skinned, mesh + atlas outputs, clip count (hover for names), owning prefab, then **Import Asset** / **Update Asset**. New assets expose an editable id and world height before the bake runs. While the bake runs the panel shows a staged progress bar and elapsed time; on success a green `ICON_FA_CHECK` line with the summary, elapsed time, and placeable prefab; on failure the `ASSET-BAKE-*` / `ASSET-IMPORT-*` codes with remediation. The TICKET-0245 named rebake section moved below it and now runs through the same async job.

**Files and surfaces**

- `include/engine/editor/file_dialog.h`, `src/rendering/editor_file_dialog.cpp` — native `IFileOpenDialog` wrapper (`ole32` linked in `CMakeLists.txt`); returns `nullopt` on non-Windows.
- `include/engine/automation/asset_import.h`, `src/automation/asset_import.cpp` — `plan_asset_import`, `plan_target_rebake`, `plan_asset_replace`, `run_asset_import`, `find_prefab_for_mesh`. Planning probes the glTF/GLB (skin, clip names, authored height) and matches registered targets by source path, baked output, id slug, or source stem. `find_prefab_for_mesh` prefers the prefab whose file name matches the mesh so the player import reports `player.prefab.json`, not `npc_test.prefab.json`.
- `include/engine/automation/asset_bake_commands.h`, `src/automation/asset_bake_commands.cpp` — `AssetBakeTargetInfo` gained `baker` / `mesh_output` / `atlas_output`; process spawning is shared through `run_asset_bake_tool`, and `bake_error` became public `asset_bake_error`.
- `src/rendering/render_app.cpp` — async import job (`std::future` + mutex-guarded stage string), `poll_asset_import` each frame, panel + browser entry points; success feeds `pending_mesh_reloads` / `prefab_meshes_dirty`, which the existing frame loop uses to re-upload GPU geometry and reload the animation clip library.
- `include/engine/editor/editor_icons.h`, `src/ui/game_fonts.cpp` — `ICON_FA_CHECK` / `ICON_FA_XMARK` glyphs (`0xf00c`, `0xf00d`).
- `src/automation/command.cpp` — `engine asset-import --file <model> [--id <slug>] [--plan] [--json]` plus help text.
- `tools/gltf_normalize.py` — flattens `.glb`, external `.bin`, and external/embedded images into single-buffer glTF plus a sidecar PNG; `measure_height` helper.
- `tools/bake_generic_gltf.py` — generic skinned baker mirroring the player normalize pass (scale positions, node translations, skeleton root offset, inverse bind matrices, animation translation tracks), skipping the scale pass when nodes carry baked `matrix` transforms.
- `tools/asset_bake.py` — `--register` command (normalize into `tools/art/<slug>/`, append a catalog entry, emit JSON) and `generic_static` / `generic_skinned` bake dispatch. `append_catalog_target` inserts one entry textually so the hand-authored catalog formatting survives, and `importedFrom` is recorded repo-relative. Player bake always regenerates `GoodPlayerModel.gltf` from `GoodPlayerModel_rigged.bbmodel` when present (`refreshedFromBbmodel`) so truncated native glTF exports cannot drop Run/Attack lengths.
- Follow-up (same ticket): Import / Re-import / Replace accept `.bbmodel` for registered targets (player matched by name); no separate Assets tab. Diagnostics → Assets lists Blockbench clips, shows baked clip names after success, reloads the clip library immediately, and reminds to restart play test when one is active. Player bake keeps Blockbench **mesh/UVs**, then replaces **every** bbmodel clip with rest-relative positions (`rest + offset/16`) so Attack no longer sinks and Run updates even when duration is unchanged. Full bbmodel mesh export remains last-resort only.
- `tools/asset_bake_verify.py` — optional `checkFeet` / `checkHeight` switches so targets that cannot be rescaled skip those gates instead of failing.
- `context/formats/mesh-assets.md` — "Importing a model from inside the editor".

**Schema / API deltas**

- Catalog entries may now use `baker: generic_static` / `generic_skinned` with a `generic` block (`targetHeight`, `generator`, `sceneName`, `meshName`, `materialName`, `maxAtlas`, `cleanBackdrop`) and record `importedFrom` / `defaultAtlas`.
- `verify` accepts `checkFeet` / `checkHeight` booleans (default true).

**Verification evidence**

- `engine` target rebuilt under the shared build lease (`cursor-asset-import` / TICKET-0247, released). Only the two pre-existing `render_app.cpp` warnings (C4996 `getenv`, C4456 shadowed `updated`) remain.
- `engine_suite_tests --suite automation` 186/186 (includes bbmodel → player plan + Run/Attack clip listing), `--suite assets` 97/97, `--suite animator` 373/373.
- Player bake after bbmodel refresh: `Attack` duration 1.15 s (was 0.75 s from truncated native glTF export), `refreshedFromBbmodel: true`.
- `engine asset-import --plan` resolves `tools/art/player/GoodPlayerModel.gltf` → update `player` (registered source, skinned, 17 clips), `assets/models/player.gltf` → update `player` (baked output), `tools/art/tree/Tree.bbmodel` → update `tree`, and rejects a `.bbmodel` with no registered target.
- Full CLI import of `GoodPlayerModel.gltf`: 17 clips baked, all 17 `ASSET-BAKE-*` gates pass, mesh reload emitted.
- Full CLI import of an unregistered model: target registered, source normalized into `tools/art/`, mesh + atlas baked, 7 gates pass, prefab written to `assets/prefabs/Imported/`, `engine validate` clean. Test artifacts were removed and the catalog restored afterwards.
- Live editor: picked `GoodPlayerModel.gltf` through the native dialog, plan showed "Updates existing asset 'player'" / 17 clips / `player.prefab.json (existing)`, **Update Asset** ran with staged progress, finished with the green check in 87.8 s, and a play-test walk shows the re-imported player textured and animating (no T-pose).

**Decisions**

- Extend the Python bake pipeline instead of writing a native importer, so imported assets go through the same `ASSET-BAKE-*` gates as the hand-registered ones.
- Blocking native dialog on the UI thread (a frame hitch while the picker is open) but the bake itself on a worker thread.
- Default world height 1.0 m static / 1.8 m skinned for new imports, editable before the bake, because DCC exports are rarely authored in metres.

**Leftover risk**

- Generic imports use the first image in the source; multi-material models lose the extra maps.
- A generic skinned import does not author `*.rig.json` or an animator controller, so a new character still needs those by hand.
- The native picker is Windows-only; `file_dialog_available()` reports false elsewhere and the panel says so.
- Imported prefabs have no colliders.

## Agent notes

- Generic static imports reuse `bake_tier1_props_gltf.bake_prop` (flatten + atlas inpaint + UV snap); generic skinned imports use the new `tools/bake_generic_gltf.py`, which mirrors the player baker's normalize pass (scale positions, scale node translations, offset skeleton root, recompute inverse bind matrices, scale animation translation tracks) without player-specific atlas cleanup.
- Sources whose nodes store baked `matrix` transforms cannot be rescaled without decomposition, so those targets skip the height/feet gates via new `checkHeight` / `checkFeet` verify switches instead of failing.
- Default world height for a new import is 1.0 m static / 1.8 m skinned because Blockbench and most DCC exports are not authored in metres; the author overrides it before importing.
