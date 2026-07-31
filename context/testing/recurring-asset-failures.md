# Recurring Asset Failures — Path Forward

When the **same class** of broken sample asset shows up again (missing locomotion clips, muddy foliage atlases, wrong-named bake targets), do not rediscover from scratch. Fix the immediate asset, then update this playbook and add a `findings.md` entry that links here under **Path forward when this recurs**.

Newest classes go first. Keep entries short: symptom → cause → commands → prevent.

## How to use

1. Match the symptom (console string, visual glitch, missing clip).
2. Follow **Fix now** exactly (named bakes only — see `import-only-named-assets`).
3. After the fix lands, confirm **Prevent / verify** still holds; bump this page if the path changed.
4. Record a dated finding in `context/testing/findings.md` with a **Path forward when this recurs** bullet pointing at the section id below.

## Player — missing hand / finger faces after bake

- **Symptom:** Palms or digit cubes look hollow / missing sides in play-test; body looks fine.
- **Cause:** Blockbench hand cubes often export **majority-inward** winding; D3D `CULL_BACK` hides those faces. Player baker historically did not flip winding (Tier-1 props did).
- **Fix now:**
  1. `engine asset-bake --project samples/open-world-rpg --target player --json` (baker stamps `…-winding` generator and flips inward hand/digit tris).
  2. Confirm `player.bake.json` gate `ASSET-BAKE-WINDING-HAND` is `ok` (outward ≥ inward).
  3. After in-editor bake, mesh **and** animation clips hot-reload; if still T-posed, end play-test and F5 again.
- **Prevent / verify:** Catalog `checkWindingHands: true` → fail-closed `ASSET-BAKE-WINDING-HAND`. Do not ship a player bake that skips the winding flip.

## Player — T-pose / animations dead after Assets bake (clips present)

- **Symptom:** Bake report lists Idle/Walk/Run/Fall, but play-test stays bind/T-pose until editor restart — or mesh T-poses while **shadows** still animate.
- **Cause:** (a) Mesh hot-reload updated skin while `AnimationClipLibrary` kept the **pre-bake** clip cache; (b) prop-instanced lit pass ignored blend weights (shadow VS skinned).
- **Fix now:** Rebuild/restart with prop-pass LBS + clip reload-on-mesh-bake; end play-test + F5 after bake. Prefer Assets bake on a build that reloads clips with `pending_mesh_reloads`.
- **Prevent / verify:** Editor reloads clip sources when mesh keys reload; prop root binds Bones CBV; `ASSET-BAKE-CLIP-*` + `ASSET-BAKE-CLIP-LOCO-THIN` fail-close thin exports. If shadow moves and mesh does not, check prop VS LBS.

## Player — `ANIM-CLIP-MISSING: Clip 'Run'` (or Walk/Fall)

- **Symptom:** Play-test console spam `ANIM-CLIP-MISSING`; Idle may work, Walk/Run/Fall fail closed.
- **Cause:** `samples/open-world-rpg/assets/models/player.gltf` drifted to a bake that dropped clips while `player.animator.json` still references Idle/Walk/Run/Fall. Full clips live on `tools/art/player/GoodPlayerModel.gltf` (or `Documents/Models/`).
- **Fix now:**
  1. Confirm source has clips: `Idle`, `Walk`, `Run`, `Fall` on `GoodPlayerModel.gltf`.
  2. `engine asset-bake --project samples/open-world-rpg --target player --json` (or Diagnostics → Assets → Bake).
  3. Confirm baked `player.gltf` animation names include Walk + Run + Fall; `player.bake.json` verify all `ok`.
  4. Play-test Idle↔Walk↔Run↔Fall; mesh hot-reloads when bake succeeds in-editor / via MCP reload queue.
- **Prevent / verify:** `asset-bake` fail-closes with `ASSET-BAKE-CLIP-MISSING` / `ASSET-BAKE-CLIP-REGRESS` / `ASSET-BAKE-CLIP-EMPTY`. Do not overwrite `player.gltf` from a clip-stripped export.

## Player — yellow chrome / neon overlays / missing soft blues after bake

- **Symptom:** Baked `player.png` looks wrong (yellow wash, neon face islands, or lost eye/metal blues) after player bake.
- **Cause:** Atlas clean treated Blockbench yellow canvas as warm skin and wiped soft blues as “neon”; pink overlays could slip through as blush.
- **Fix now:**
  1. Re-export `GoodPlayerModel.gltf` (+ matching `.png`) into `tools/art/player/` (or `Documents/Models/`).
  2. Confirm baker generator is `GoodPlayerModel-2026-07-31` or newer.
  3. `engine asset-bake --target player --json` — expect Idle/Walk/Run/Fall, zero yellow-canvas pixels, soft blue retained.
  4. Editor Assets bake queues GPU reload; otherwise restart editor.
- **Prevent / verify:** Gates `ASSET-BAKE-ATLAS-CANVAS`, `ASSET-BAKE-ATLAS-NEON`, `ASSET-BAKE-ATLAS-SOFT-BLUE`, `ASSET-BAKE-ATLAS-SIZE`.

## Foliage — glitchy / muddy `bush_tall` (or `bush`) atlas

- **Symptom:** Tall (or normal) bush reads as camo streaks, neon speckles, or “broken” faces in-world.
- **Cause:** Tier-1 foliage bake either (a) full-atlas-inpainted sparse Blockbench islands into muddy few-shade fills, or (b) strict green filter wiped non-olive paint, or (c) source `Tall_Bush.png` still carries Blockbench chrome/face islands.
- **Fix now:**
  1. Named rebake only: `python tools/bake_tier1_props_gltf.py bush_tall` (never bare baker for “just this bush”).
  2. Confirm `bush_tall` PROPS uses soft foliage clean (`foliage_strict_colors: false`) + small `foliage_inpaint_radius` (not full-atlas fill). Generator should note `v6-foliage-soft-inpaint` or newer.
  3. Sanity-check atlas: open `assets/models/bush_tall.png` — expect sparse islands and ≥~12 opaque unique colors, not a solid camo rectangle with ~6 shades.
  4. If source art is wrong (faces/neon in `tools/art/tall-bush/Tall_Bush.png`), re-export/paint foliage-only atlas in Blockbench, copy into `tools/art/tall-bush/`, then rebake — do not “fix” by editing only the baked PNG long-term.
  5. Kill `engine.exe` if PNG write fails with errno 22 / sharing violation; rebake; restart MCP/editor.
- **Prevent / verify:** After foliage rebakes, `ASSET-BAKE-ATLAS-MUDDY` (opaque unique color floor) and `ASSET-BAKE-UV-TRANSPARENT` via `engine asset-bake --target bush_tall`. See `context/formats/mesh-assets.md` (bush section) and `skills/import-blockbench-models/SKILL.md`.

## Tier-1 prop — corrupted / hollow / wrong mesh after import

- **Symptom:** Prop looks hollow, checkerboarded, truncated, or wrong after an import session.
- **Cause:** Wrong bake target, collateral atlas overwrite, or prefab replaced with a bare `source` string.
- **Fix now:** Follow **Recovering a corrupted Tier-1 prop mesh** in `context/formats/mesh-assets.md` (re-export → `tools/art/<slug>/` → named `bake_tier1_props_gltf.py <name>` → reload GPU).
- **Prevent / verify:** `import-only-named-assets` rule; `git status` on `assets/models/` and revert untouched props.
