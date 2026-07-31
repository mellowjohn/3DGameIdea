---
name: import-player-character
description: >-
  Bake and wire the skinned Player_V2 character kit (glTF, atlas, rig,
  animator, character asset, spawn prefab) — not static props. Use when
  re-exporting Player_V2 from Blockbench, fixing Idle/Run/Fall skinning,
  updating player.png, NPC test stand-ins, or character.visualPrefab bindings.
---

# Import Player Character

Bring a Blockbench **skinned** Player_V2 export into `open-world-rpg` as the playable visual.

**Read first:** [`context/formats/mesh-assets.md`](../../context/formats/mesh-assets.md) (player section), [`context/formats/character-assets.md`](../../context/formats/character-assets.md), [`context/art/character-direction.md`](../../context/art/character-direction.md), [`context/art/blockbench-asset-list.md`](../../context/art/blockbench-asset-list.md).

**Modeling before bake:** reshape / build meshes from concept orthos with [`blockbench-mesh-authoring`](../blockbench-mesh-authoring/SKILL.md) (loop cuts, primary→tertiary, palms-down T-pose, paint-vs-modeled face parts).

**Not for:** static Scene Asset props — use [`import-blockbench-models`](../import-blockbench-models/SKILL.md).

## Checklist

```
Player kit:
- [ ] Copy source glTF + atlas under tools/art/player/ (GoodPlayerModel.gltf)
- [ ] engine asset-bake --project samples/open-world-rpg --target player [--json]
     (or Diagnostics → Assets → Bake player; or python tools/asset_bake.py --target player)
- [ ] Confirm player.gltf skins/joints/clips + player.png; report in player.bake.json
- [ ] Verify gates pass (Idle/Walk/Run/Fall required; ASSET-BAKE-CLIP-* / ATLAS-* codes)
- [ ] Rig / animator / character asset still point at the bake
- [ ] Play-test Idle↔Walk↔Run↔Fall; NPC stand-in has separate animator instance
- [ ] Update mesh-assets.md + blockbench-asset-list.md generator notes
```

## 1. Source layout

Prefer project-owned copies:

| Asset | Canonical repo path | Common export drop |
| --- | --- | --- |
| Rigged glTF | `tools/art/player/GoodPlayerModel.gltf` | `Documents/Models/GoodPlayerModel.gltf` |
| Atlas PNG | `tools/art/player/GoodPlayerModel.png` | `Documents/Models/GoodPlayerModel.png` |
| Optional bbmodel | `tools/art/player/GoodPlayerModel_rigged.bbmodel` | `Documents/GoodPlayerModel.bbmodel` |
| Legacy V2 glTF | `tools/art/player/Player_V2_rigged.gltf` | — |

Bake script search order is documented in `tools/bake_player_v2_gltf.py` (`SRC_CANDIDATES` / `SRC_PNG_CANDIDATES`). Copy into `tools/art/player/` before relying on Desktop paths.

Target: feet at y=0, height ≈ **2.75 m**, skins + `JOINTS_0`/`WEIGHTS_0` preserved, clips **Idle / Walk / Run / Fall** (plus authored extras).

If play-test logs `ANIM-CLIP-MISSING` for Walk/Run/Fall, the sample bake lost clips — `asset-bake` fail-closes with `ASSET-BAKE-CLIP-*`; see [`recurring-asset-failures.md`](../../context/testing/recurring-asset-failures.md), then rebake.

## 2. Bake

**Preferred (TICKET-0245):**

```bash
engine asset-bake --project samples/open-world-rpg --target player --json
# or editor: Diagnostics → Assets → target player → Bake
# or: python tools/asset_bake.py --project samples/open-world-rpg --target player --json
```

Still valid low-level: `python tools/bake_player_v2_gltf.py` (repo-canonical `tools/art/player/GoodPlayerModel.gltf` first).

Writes:

- `samples/open-world-rpg/assets/models/player.gltf`
- `samples/open-world-rpg/assets/models/player.png`
- `samples/open-world-rpg/assets/models/player.bake.json` (verify report)

Bake clears Blockbench marker colors, pads UV islands, keeps Blockbench UV V for D3D, preserves **all** source clips, and fail-closes on missing animator clips (`Idle`/`Walk`/`Run`/`Fall`), atlas canvas/neon, height, joints. Bump / note `asset.generator` when behavior changes (`GoodPlayerModel-2026-07-31`+).

Optional: Blockbench File→Export glTF into `tools/art/player/` or `Documents/Models/` then `--source` / catalog default.

## 3. Runtime wiring (usually already present)

| Piece | Path |
| --- | --- |
| Mesh | `assets/models/player.gltf` |
| Rig metadata | `assets/characters/player.rig.json` (joint names must match) |
| Animator | `assets/animators/player.animator.json` |
| Character | `assets/characters/player.character.json` → `visualPrefab` |
| Play session | `play.session.json` character + camera |
| Player spawn prefab | compositional prefab with `characterAsset` / visual match |
| NPC stand-in | `assets/prefabs/NPC/npc_test.prefab.json` (same mesh; **no** `characterAsset`; own animator instance) |

Hand bones: `Left`/`Right` + `Thumb`/`Index`/`Middle`/`Ring`/`Pinky` + `1`/`2` under each hand — must stay aligned with `player.rig.json`.

Art direction: kits layer over locked base body refs in `context/art/reference/` — do not silently redesign proportions away from `player-base-body-front.png` without owner ask.

## 4. Verify

1. `engine validate --project samples/open-world-rpg` (or `engine_project_validate`).
2. Live editor play-test: walk / jump — Idle↔Run↔Fall; no permanent bind/T-pose.
3. Place `npc_test` — must **not** mirror the player’s locomotion pose (separate animator instance). See `context/testing/findings.md` if regressions return.
4. After C++ skinning/animator changes, use [`live-editor-mcp`](../live-editor-mcp/SKILL.md) rebuild lease loop.

## Done bar

Bake outputs present; clips/joints intact; play-test locomotion OK; docs list updated generator string/date.
