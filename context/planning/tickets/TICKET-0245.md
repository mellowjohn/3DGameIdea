# TICKET-0245: Universal Asset Bake (Editor + MCP + CLI)

- Epic: EPIC-0009
- Status: needs-approval
- Agent: cursor-agent
- Priority: P0
- Notion: https://app.notion.com/p/3aed3efc569581ff895adfbe9ef4a9e7

## Goal

Authors and agents rebake any registered Blockbench mesh (player + named props) through one hardened path — editor Assets tab, `engine asset-bake`, and MCP — with fail-closed gates for lost clips, glitchy atlases, resolution/scale drift, and joint mismatches.

## Context links

- Plan: Universal Asset Bake
- [`context/formats/mesh-assets.md`](../../formats/mesh-assets.md)
- [`context/testing/recurring-asset-failures.md`](../../testing/recurring-asset-failures.md)
- [`skills/import-player-character/SKILL.md`](../../../skills/import-player-character/SKILL.md)
- [`skills/import-blockbench-models/SKILL.md`](../../../skills/import-blockbench-models/SKILL.md)
- Existing bakers: `tools/bake_player_v2_gltf.py`, `tools/bake_tier1_props_gltf.py`

## Acceptance criteria

- [x] `tools/asset_bake.py` + `tools/asset_bake_catalog.json` list registered targets; named `--target` required (no bare bake-all)
- [x] Shared verify module fails closed with stable codes (`ASSET-BAKE-CLIP-*`, `ATLAS-*`, `HEIGHT`, `JOINT-*`, etc.)
- [x] Player bake prefers repo `tools/art/player/`, preserves all source clips, requires animator clips Idle/Walk/Run/Fall
- [x] `engine asset-bake --project … --target <id> [--source] [--json]` and `--list`
- [x] MCP `engine_asset_bake` mirrors CLI; live session queues mesh reload on success
- [x] Diagnostics **Assets** tab: target combo, Bake, verify failure display, hot-reload on success
- [x] Updated GoodPlayerModel ingested; Walk present; atlas gates pass
- [x] Docs/skills/recurring-failures updated to the new path

## Out of scope

- Auto-export from Blockbench inside the editor
- Baking unregistered freeform models
- Wiring Jump/Attack/Dodge into the animator graph
- Full rewrite of atlas-clean heuristics (gate outputs only)

## Dependencies

None. Soft: Python + Pillow on PATH for bake spawn.

## Verification

```powershell
python tools/asset_bake.py --list --json
python tools/test_asset_bake_verify.py
engine asset-bake --project samples/open-world-rpg --list --json
engine validate --project samples/open-world-rpg
```

Player ingest: 17 clips including Walk; `player.bake.json` verify all ok; generator `GoodPlayerModel-2026-07-31`.

## What changed

- Summary: Shipped universal named asset bake (CLI/MCP/Diagnostics Assets) with fail-closed clip/atlas/scale/joint gates, and ingested the updated GoodPlayerModel export (Idle/Walk/Run/Fall + extras) into `player.gltf`.
- Files / surfaces: `tools/asset_bake.py`, `asset_bake_verify.py`, `asset_bake_catalog.json`, `bake_player_v2_gltf.py`; `src/automation/asset_bake_commands.*`, `command.cpp`, `mcp_server.cpp`, `editor_session.cpp`, `render_app.cpp` Assets tab; baked `player.gltf`/`player.png`/`player.bake.json`; docs/skills.
- Schema / API: CLI `asset-bake`; MCP `engine_asset_bake`; verify codes `ASSET-BAKE-*`.
- Seed / sample: GoodPlayerModel from `Documents/Models/GoodPlayerModel.gltf` → `tools/art/player/`.
- Tests: `tools/test_asset_bake_verify.py` ok; MSBuild `engine` ok; validate ok; list shows 17 targets.
- Decisions: MCP bakes offline then queues reload (bake can exceed bridge timeout); player atlasMax raised to 4096 for 2048 atlases.
- Leftover risk: Jump/Attack/Dodge clips are in the glTF but not wired in the animator; atlas clean still slow on 2048 textures.

## Agent notes

Owner override P0 2026-07-31. Blockbench MCP path flaky for export; used owner-exported glTF.
