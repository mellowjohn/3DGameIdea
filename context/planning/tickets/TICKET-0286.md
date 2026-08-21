# TICKET-0286: Appearance sockets (hair / skin / eyes)

- Epic: EPIC-0019
- Status: active
- Agent: cursor-agent
- Priority: P2
- Notion: https://www.notion.so/3c3d3efc5695816180fbd43ab5e0832a

## Goal

Ship character appearance sockets so a modular hair mesh sits on the GoodPlayerModel hair-cap, with skin and eye options stored on `.character.json` and previewed in Animation Studio / creation — separate from inventory armor slots.

## Context links

- `context/art/character-direction.md` (hair-cap; eyes painted on atlas)
- `context/formats/character-assets.md`
- `context/formats/mesh-assets.md` (`test_hair_spikes`)
- `context/features/animation-studio.md`
- World Forge `coding_character_asset_slots`
- Prior: TICKET-0250 (inventory armor shells)

## Acceptance criteria

- [x] `.character.json` optional `appearance` (`hairMesh`, `hairTint`, `skinTint`, `eyeTint`)
- [x] Test hair bake `test_hair_spikes` from `kit_test_hair_spikes` with `matchPlayerBake`
- [x] Runtime draws hair as a skinned overlay (not `head` armor) sharing player animation
- [x] Animation Studio Appearance sockets + MCP `set_appearance`
- [x] Named assets/character suite covers appearance parse + option tables
- [x] Creation Lua writes `appearance.*` blackboard keys
- [x] Context docs updated
- [ ] Body-atlas GPU multiply for skin/eye (follow-on)

## Out of scope

- Act 0 starter outfit sets (Ashfell / Outrider / Runecaster)
- Final hero hair styles (this is a slot test mesh)
- Inventory equip of hair
- Body-atlas GPU multiply for skin/eye (schema + Studio ids land first)

## Dependencies

Soft after TICKET-0250 armor bind-space. Does not block outfit kits.

## Verification

`engine` rebuild under lease. Assets bake `test_hair_spikes`. Character/assets suite. Animation Studio screenshot with hair on / shaved.

## What changed

Hair slot test is live. Default option is **Short Brown** (`short`): cap-only `kit_test_hair_spikes` (63 verts) bakes to `assets/models/test_hair_spikes.gltf` (`matchPlayerBake`) and draws as a skinned overlay (`hair:<mesh>`). Animation Studio **Appearance sockets** plus MCP `set_appearance` cycle hair/skin/eyes option ids. Creation Lua writes `appearance.hair|skin|eyes` blackboard keys during A0-02. Skin/eye tints are stored on `.character.json` but not yet multiplied on the body atlas.

- Files: `character_asset.*`, `render_app.cpp` hair palette/draw, `mcp_server.cpp` `set_appearance`, `player.character.json`, `ui_handlers.lua`, catalog `test_hair_spikes`, docs.
- Tests: `engine_suite_tests --suite assets` 103/103. Engine Debug rebuild (existing getenv / hiding warnings).
- Screenshot: Animation Studio Idle with short cap (`out/short-hair-on-player-3q-*.png`).

## Agent notes

Hair mesh authored in live Blockbench on GoodPlayerModelCopy (`kit_test_hair_spikes`). Helmet hidden for authoring. Skin/eye shader multiply remains before needs-approval.
