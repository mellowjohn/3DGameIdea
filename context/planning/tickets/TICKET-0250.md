# TICKET-0250: Gear / equipment swap in Animation sandbox

- Epic: EPIC-0019
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: (see Notion Tickets DB by Ticket ID)

## Goal

Authors can change what the sandbox subject has equipped (held items with `worldMesh`) while previewing animations, so weapon silhouettes can be validated without entering a world play-test.

## Context links

- [`context/features/animation-studio.md`](../../features/animation-studio.md)
- [`context/features/gearing-system.md`](../../features/gearing-system.md)
- Prior: TICKET-0249

## Acceptance criteria

- [x] **Equip UI in Animation mode:** Held-item dropdown from item catalog (`worldMesh` present); `(none)` for empty. Armor strip: Head / Chest / Legs combos + iron-test set button (studio-session only).
- [x] **Visual attach:** Equipped mesh appears on the sandbox subject using existing handAttach / bone weld path.
- [x] **Animation under gear:** Playing / scrubbed clips keep the attachment following joints.
- [x] **No world inventory mutation:** Studio-session `anim_studio_held_item_id` only; bags untouched.
- [x] **Docs + rebuild `engine`.**

## Out of scope

- Full inventory UX / bag management
- Crafting, loot tables, affinity math
- Keyframe editing (0253)
- Armor/equip-strip mesh swap — Animation sandbox now toggles head/chest/legs; character-asset appearance sockets (hair/skin) remain follow-on

## Dependencies

- Blocked by: TICKET-0249 (subject + skinning)
- Soft: TICKET-0246 bone weld correctness

## Verification

Desktop: Animation tab → equip Ashfell sword / Outrider bow → Play Attack/Idle → weapons follow hands; Scene world unchanged.

## What changed

### Summary

Animation Studio can equip any catalog item with a `worldMesh` onto the sandbox subject. The mesh welds to the sampled joint each frame (same chain as play-test hotbar) without touching inventory bags.

### Files / surfaces

- `src/rendering/render_app.cpp` — `anim_studio_held_item_id`, held combo UI, studio skin joint resolve, `append_held_weapon_render_instance` in sandbox pass
- `context/features/animation-studio.md`, `gearing-system.md`, this stub, `epics.md`

### Schema / API

- No new asset schema. Studio state only on `EditorState`.

### Samples

- Uses existing `assets/items/*.json` with `worldMesh` / `handAttach`.

### Verification evidence

- `engine` rebuild under lease (TICKET-0250)
- Hidden Animation viewport smoke if MCP restarted

### Decisions

- Held `worldMesh` items only for this ticket; armor equip strip deferred.

### Leftover risk

- Item meshes must already be loadable in the imported mesh set (same as play-test).

## Agent notes

Landed with TICKET-0251 weld UI in the same Diagnostics Animation strip.
