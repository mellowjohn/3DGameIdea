---
name: import-blockbench-models
description: >-
  Import Blockbench glTF/GLB props into open-world-rpg (copy source, bake,
  prefab, place, verify). Use when the user asks to import a model, bring in a
  Blockbench export, bake a glTF into assets/models, wire a Scene Asset prefab,
  or fix missing faces / wrong scale / hollow meshes on imported props.
---

# Import Blockbench Models

Bring a Blockbench (or similar) glTF into `samples/open-world-rpg` as a runtime mesh + Scene Asset prefab.

**Read first:** [`context/formats/mesh-assets.md`](../../context/formats/mesh-assets.md), [`context/art/blockbench-asset-list.md`](../../context/art/blockbench-asset-list.md), [`context/architecture/content-vs-engine-workflows.md`](../../context/architecture/content-vs-engine-workflows.md).

**Not for:** skinned player/NPC kits (use `tools/bake_player_v2_gltf.py` + character/rig docs), foliage density paint, or inventing new C++ importer features unless a ticket requires it.

## Checklist

```
Import:
- [ ] Copy source under tools/art/<slug>/
- [ ] Bake → assets/models/<name>.gltf (+ atlas .png)
- [ ] Prefab with mesh.asset + deps; full JSON via MCP
- [ ] Place / refresh in sandbox; visual verify
- [ ] Update mesh-assets.md + blockbench-asset-list.md
```

## 1. Source layout

1. Prefer a project-owned copy under `tools/art/<slug>/` (glTF + atlas PNG / `.bbmodel` if available).
2. Do not leave the only copy on the desktop or `Documents/Models/` — copy into the repo before bake.
3. License/provenance: permissively licensed only; record in `context/resources/index.md` when adding third-party art.

Typical sizes (adjust with owner): crate ~1.0 m height; bush ~1.25 m; tall bush ~1.9 m; campfire ring ~1.3 m diameter (`scale_mode: max_xz`).

## 2. Bake

Default Tier-1 static props (always pass explicit names unless rebaking everything on purpose):

```bash
engine asset-bake --project samples/open-world-rpg --target bush_tall --json
# or: python tools/asset_bake.py --target bush_tall --json
# low-level: python tools/bake_tier1_props_gltf.py bush_tall
```

List registered targets: `engine asset-bake --project samples/open-world-rpg --list --json` (or Diagnostics → **Assets** tab).

If a foliage prop looks muddy/camo or a prior import corrupted a mesh, use [`recurring-asset-failures.md`](../../context/testing/recurring-asset-failures.md) before inventing a new bake heuristic.
Add a `PROPS` entry in that script (or a dedicated `tools/bake_<name>_gltf.py` following stones/stump/dead-log). Bake must:

| Step | Why |
| --- | --- |
| Flatten node transforms into one mesh | Engine static import does not fully bake node graphs |
| Feet at y=0; normalize height or XZ span | Consistent world scale |
| Emit `POSITION` + `TEXCOORD_0` + `COLOR_0` + atlas PNG | Prop pipeline samples albedo when present |
| Flip majority-inward primitives | Blockbench cylinders often export inside-out; props use `D3D12_CULL_MODE_BACK` |
| Snap transparent UVs to opaque atlas texels | Sparse atlases → black / “missing” sides if UVs miss paint |
| Keep real geometry | Do **not** replace cylinders with AABB boxes |

Bump the glTF `asset.generator` string when changing bake behavior so reloads are obvious.

Existing specialized bakers: `bake_stones_gltf.py`, `bake_stump_gltf.py`, `bake_dead_log_gltf.py`, `bake_tree_bbmodel.py`, `bake_player_v2_gltf.py`.

## 3. Prefab

Scene Asset path: `samples/open-world-rpg/assets/prefabs/Scene Assets/<name>.prefab.json`.

Minimal mesh prefab:

```json
{
  "entities": [
    {
      "name": "DisplayName",
      "parent": null,
      "mesh": { "asset": "assets/models/<name>.gltf" },
      "transform": {
        "position": [0, 0, 0],
        "rotation": [0, 0, 0, 1],
        "scale": [1, 1, 1]
      }
    }
  ],
  "schemaVersion": 2
}
```

Solid props that the player should bump into must include a `collision[]` volume (`staticWorld`, non-trigger): trunk **capsule** for trees, mid-body **sphere** for bushes, **box**/capsule for crates/stones/stumps/logs. Size volumes in prefab-local space to match any entity mesh `scale`. Add lights/triggers only when needed (campfire light + `use_campfire`). GPU-painted foliage layers stay visual-only (no per-instance collision).

### MCP prefab writes — critical

```text
❌ BAD: engine_prefab_apply with source: "tier1-import" and no json
   → overwrites the prefab file with the string "tier1-import"
✅ GOOD: pass the full prefab json object (or write the file, then refreshCatalog)
```

After disk write, refresh live catalog if the editor is running. Mesh GPU reload may need editor restart if the asset was already loaded.

## 4. Place and verify

1. Prefer live MCP: `engine_scene_apply` place with `snapToTerrain`, then `engine_editor_screenshot`.
2. Launch editor against sandbox when needed:
   `build/windows-msvc-debug/dev-next/engine.exe editor --project samples/open-world-rpg --world worlds/sandbox.world.json`
3. Confirm: scale vs nearby props, opaque faces (no hollow cylinders), atlas colors (not black/sky-through), feet on ground.
4. Optional: `engine_project_validate`.

No C++ change → no MSBuild. C++/shader change → acquire build lease, kill `engine.exe`, rebuild `engine`, restart, release lease (see `rebuild-after-code-changes` / `agent-build-coordination`).

## 5. Context updates

Same change set:

- `context/formats/mesh-assets.md` — bake command, scale, known caveats
- `context/art/blockbench-asset-list.md` — shipped row + source → runtime paths

## Failure playbook

| Symptom | Likely cause | Fix |
| --- | --- | --- |
| Hollow / missing cylinder sides | Inside-out winding + backface cull | Bake: flip when inward > outward; keep cylinders |
| Black / blue “scraps” on sides | UVs land on transparent atlas | Bake: snap UV to opaque (prefer brown for log nodes) |
| Flat / wrong size | Wrong `target_height` / `scale_mode` | Rebake with height or `max_xz` + `target_span` |
| Prefab is 12 bytes / garbage | MCP `source` without `json` | Restore from git; rewrite full JSON |
| Stale look after rebake | GPU mesh still old | Restart editor/MCP; re-place instance if needed |
| Looks good in Blockbench, bad in engine | BB display ≠ D3D cull / atlas sample | Trust engine screenshot; fix bake, don’t box-rebuild |
| Bright yellow/pink “broken” faces on bushes | Blockbench chrome/neon in sparse atlas | Bake with `foliage_atlas` + inpaint (`v5-foliage-atlas-inpaint`) |
| In-game mesh “corrupted” / punched holes | Stale bake, MASK+sparse atlas, or bad source overwrite | See **Recover corrupted prop** below |

## Recover corrupted prop

When the runtime mesh looks broken but you still have a good Blockbench export:

1. Export/save glTF + PNG under `Documents/Models/` (and keep `.bbmodel` if present).
2. Copy into `tools/art/<slug>/` (overwrite).
3. Rebake: `python tools/bake_tier1_props_gltf.py <name>` (bump `generator` in the `PROPS` entry so reloads are obvious).
4. Confirm prefab JSON still references `assets/models/<name>.gltf` and is full JSON.
5. Restart `engine.exe` editor/MCP so GPU reloads the mesh; screenshot to verify.

Documented in detail in [`context/formats/mesh-assets.md`](../../context/formats/mesh-assets.md) (Recovering a corrupted Tier-1 prop mesh).

## Quick commands

```bash
# Copy export into repo
# tools/art/<slug>/<Export>.gltf (+ texture.png)

python tools/bake_tier1_props_gltf.py

# Validate project (from engine cwd / MCP)
# engine_project_validate
```
