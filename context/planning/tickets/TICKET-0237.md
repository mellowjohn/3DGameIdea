# TICKET-0237: Act 0 Landfall loot slice (thin inventory + finds)

- Epic: EPIC-0018
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/3aad3efc56958193ad09c3df76894c09

## Goal

During Act 0 Landfall, players can actually **get and keep** a small set of items (not a full Terraria pool): starter weapon plus a handful of finds/rewards on the siege → camp path.

## Context links

- `context/features/gearing-system.md` — Act 0 Landfall loot slice
- DEC-0048 (no hard class locks; obscure rare optional, not story-required)
- `samples/open-world-rpg/assets/world-forge/act0_mvp_readiness.worldforge.json` — `coding_inventory_thin_act0`, `gameplay_act0_loot_slice`
- Soft: TICKET-0111 / TICKET-0232 (may absorb thin inventory into 0111 if still open); **DEC-0050** inventory UX
- Beat sheet: `context/story/campaign-beat-sheet.md`

## Acceptance criteria

- [x] Thin session inventory can **grant**, **hold**, and place items on **hotbar / equip strip** per [DEC-0050](../../decisions/index.md#dec-0050-inventory-ux-item-kinds-and-positive-soft-affinity) (at least weapon + one trinket/consumable)
- [x] Hard class-lock equip rejects are **not** implemented; soft affinity if present is **positive bonus only**
- [x] Act 0 item defs authored per [`gearing-system.md`](../../features/gearing-system.md) concrete id table (~8–12 incl. starters + `vein_iron_pendant`); ids from display names
- [x] At least **one** world pickup/find on Calrenoth approach (crate/pouch/interaction)
- [x] At least **one** story grant (`arkands_favor` or equivalent Arkand / Grenge / camp handoff) — via MCP/Lua `inventory_grant` (path ready; desktop story hook optional)
- [x] Low-rate footsoldier commons include bandage/tonic/ammo **or** documented stub table
- [x] `vein_iron_pendant` obscure nook ships; not required for `mq_act0_calrenoth`
- [x] Readiness rows `coding_inventory_thin_act0` / `gameplay_act0_loot_slice` updated to `wip`/`done` honestly
- [x] Named suite or validate coverage for grant/equip + fail-closed unknown item id
- [x] Feature note lists the shipped Act 0 item ids when done

## Out of scope

- Full item catalog / Ledgeport vendor economy
- Mining/crafting loop (TICKET-0235)
- Thrator / glad mount (TICKET-0236)
- Full soft-affinity multiplier math if a flat equip works for MVP (follow-on TICKET-0232)
- Inventory UI polish beyond a minimal usable surface (TICKET-0162 may remain separate)
- Hand-mesh weapon attach/detach (follow-on after hotbar select)

## Dependencies

Soft-blocked by character creation spawn loadout and interaction volumes. Parallel OK with combat waves. Prefer implementing thin inventory here or as the first vertical slice of TICKET-0111 — do not fork two inventory systems.

## Verification

- Rebuild `engine` for C++ inventory path — **passed** (MSBuild Debug)
- Suite / validate for item defs + grant/equip — **`hud` 117/117**, **`inventory` 20/20**
- Desktop: boot Act 0 path, pick up ≥1 find, receive ≥1 grant, confirm items persist in session (save optional if TICKET-0114 already supports it) — play-test starter + loot grant wired; owner desktop QA recommended
- Update MVP readiness statuses — set to **wip**

## What changed

### Summary

Shipped thin Act 0 inventory: item catalog load, session `InventoryRuntime` (bag 20 / hotbar 8 / equip strip), Lua + MCP APIs, pen-faithful inventory modal with dynamic icons (`imageBind` / `hud_set_image`), play-test starter loadout, and loot containers that grant into inventory.

### Files / surfaces

- `include/engine/assets/item_catalog_asset.h`, `src/assets/item_catalog_asset.cpp`
- `include/engine/inventory/inventory_runtime.h`, `src/inventory/inventory_runtime.cpp`
- `include/engine/assets/hud_asset.h`, `hud_runtime`, Lua `hud_set_image` / `ui_canvas_set_image` / `ui_canvas_set_visible`
- MCP `engine_inventory_call` + editor `inventory_call`
- `samples/.../assets/ui/inventory.uicanvas.json` (generated), `player.uicanvas.json` hotbar 8 + imageBind
- `samples/.../assets/scripts/ui_handlers.lua`, `loot_container_interaction.lua`
- `tools/generate_inventory_canvas.py`
- Suites: `hud`, `inventory`
- Context: `gearing-system.md`, `ui-canvas.md`, `ui-canvas-assets.md`, feature index, readiness rows

### Schema / API

- Widget optional `imageBind`; Lua `engine.hud_set_image(bind, path)`, `engine.ui_canvas_set_image(canvas, bind, path)`
- Lua `engine.inventory_grant|remove|set_hotbar|set_equip|select*|equip_selected|unequip_selected|move|status|def`
- MCP `engine_inventory_call` kinds: status, grant, set_hotbar, set_equip, select_hotbar, select, equip_selected, unequip_selected

### Samples

- Play-test: hotbar0 = `ashfell_arming_sword`, bag = 2× `field_bandage`
- Loot bag / supply chest → `inventory_grant`
- **I** opens inventory and refreshes binds; keys **1–8** select hotbar when no modal

### Verification evidence

- `engine_suite_tests --suite hud` → 117 passed
- `engine_suite_tests --suite inventory` → 20 passed
- MSBuild `engine` Debug succeeded

### Decisions

- Follow DEC-0050 slot counts; pen layout for chrome; Craft tab stub only
- No hand attach in this ticket

### Leftover risk

- Session inventory not yet serialized into RPG save (`inventory: {}` stub remains)
- Archetype-specific starter weapons: play-test uses `set_starter_archetype` / default `ashfell_blade` (character-creation UI still deferred)
- Desktop visual QA of pen modal vs mock not automated
- Sort button cycles label only (does not reorder bag yet)

## Agent notes

Implemented as stepped plan: toolkit image binds → InventoryRuntime → pen modal → starters/loot/HUD.
