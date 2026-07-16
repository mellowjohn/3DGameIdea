# TICKET-0017: Relationship graph visual canvas

- Epic: EPIC-0002
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ed3efc56958123b1a2c85f00c83f02

## Goal

Visual node/edge diagram for `relationships.worldforge.json` inside World Forge, complementing list/detail authoring. Mutations remain command-backed.

## Context links

- TICKET-0012 format; TICKET-0015/0016 shell + inspectors
- `include/engine/ui/world_forge_editor.h` / `src/ui/world_forge_editor.cpp`

## Acceptance criteria

- [x] Draw nodes and edges from loaded relationships asset
- [x] Selection syncs with list/detail inspector
- [x] Edits still go through `apply_world_forge_operation` (Save path unchanged)
- [x] Faction endpoint proxies shown for edges that target factions
- [x] Ephemeral drag layout + Reset layout (not serialized)

## Out of scope

Quest/dialogue graphs; perfect auto-layout as blocker; persisting canvas coordinates in JSON.

## Dependencies

TICKET-0015, TICKET-0016.

## Verification

- Rebuild `engine` — passed (after stopping locked process).
- Manual: World Forge → Relationships → **Graph**; click nodes/edges; edit detail; Save.

## What changed

### Summary

Relationships pane gained a **Graph** mode: circular layout of story nodes plus faction endpoint proxies, drawn edges labeled by kind, click-to-select syncs the right-hand detail inspector (node or edge), drag rearranges positions ephemerally. Save still writes asset JSON only via `apply_world_forge_operation`.

Follow-on polish: zoom/pan/Fit with containment so nodes stay in the canvas; wider graph pane (~70%); add/remove nodes & relationships from Graph + Nodes/Edges lists (including click-to-link mode); kind concept PNG placeholders.

### Files

- `include/engine/ui/world_forge_editor.h` — `ListKind::Graph`, graph filter/zoom fields, `concept_placeholder_tex`
- `src/ui/world_forge_editor.cpp` — canvas, filters, concept cards
- `src/ui/imgui_png_texture.cpp` — PNG upload for ImGui
- `assets/world-forge/placeholders/` — 7 concept PNGs + PROVENANCE
- Docs/tickets/epics mirrored

### Leftover

No automated GUI test; per-entity custom portraits still deferred.

## Agent notes

2026-07-15: Implemented + filter/zoom + generated placeholder images wired; rebuilt `engine.exe`. Awaiting owner approval.
