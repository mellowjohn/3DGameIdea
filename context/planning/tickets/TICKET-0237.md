# TICKET-0237: Act 0 Landfall loot slice (thin inventory + finds)

- Epic: EPIC-0018
- Status: ready
- Agent: unassigned
- Priority: P2
- Notion: https://app.notion.com/p/3aad3efc56958193ad09c3df76894c09

## Goal

During Act 0 Landfall, players can actually **get and keep** a small set of items (not a full Terraria pool): starter weapon plus a handful of finds/rewards on the siege → camp path.

## Context links

- `context/features/gearing-system.md` — Act 0 Landfall loot slice
- DEC-0048 (no hard class locks; obscure rare optional, not story-required)
- `samples/open-world-rpg/assets/world-forge/act0_mvp_readiness.worldforge.json` — `coding_inventory_thin_act0`, `gameplay_act0_loot_slice`
- Soft: TICKET-0111 / TICKET-0232 (may absorb thin inventory into 0111 if still open)
- Beat sheet: `context/story/campaign-beat-sheet.md`

## Acceptance criteria

- [ ] Thin session inventory can **grant**, **hold**, and **equip** items used in Act 0 (at least weapon + one trinket/consumable)
- [ ] Hard class-lock equip rejects are **not** implemented
- [ ] ~**4–8** Act 0 item defs authored (include starter weapons); ids from display names
- [ ] At least **one** world pickup/find on Calrenoth approach (crate/pouch/interaction)
- [ ] At least **one** story grant (Arkand / Grenge / camp handoff)
- [ ] Optional: low-rate footsoldier common drop **or** documented as deferred with a stub table
- [ ] Optional: at most one obscure nook rare; not required for `mq_act0_calrenoth`
- [ ] Readiness rows `coding_inventory_thin_act0` / `gameplay_act0_loot_slice` updated to `wip`/`done` honestly
- [ ] Named suite or validate coverage for grant/equip + fail-closed unknown item id
- [ ] Feature note lists the shipped Act 0 item ids when done

## Out of scope

- Full item catalog / Ledgeport vendor economy
- Mining/crafting loop (TICKET-0235)
- Thrator / glad mount (TICKET-0236)
- Full soft-affinity multiplier math if a flat equip works for MVP (follow-on TICKET-0232)
- Inventory UI polish beyond a minimal usable surface (TICKET-0162 may remain separate)

## Dependencies

Soft-blocked by character creation spawn loadout and interaction volumes. Parallel OK with combat waves. Prefer implementing thin inventory here or as the first vertical slice of TICKET-0111 — do not fork two inventory systems.

## Verification

- Rebuild `engine` for C++ inventory path
- Suite / validate for item defs + grant/equip
- Desktop: boot Act 0 path, pick up ≥1 find, receive ≥1 grant, confirm items persist in session (save optional if TICKET-0114 already supports it)
- Update MVP readiness statuses

## What changed

_(fill before needs-approval)_

## Agent notes

_(stub — owner ask 2026-07-27: Act 0 must include obtainable items, not only starter kits)_
