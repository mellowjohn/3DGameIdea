# TICKET-0031: Map design language (biomes, landmarks, density)

- Epic: EPIC-0004
- Status: needs-approval
- Agent: cursor-agent
- Priority: P1
- Notion: https://app.notion.com/p/39ad3efc569581de8e13d43e87f1835e

## Goal

Define a map design language for biomes, landmarks, and content density that matches dark-fantasy visual direction and supports readable open-world navigation.

## Context links

- [`../features/map-design-language.md`](../../features/map-design-language.md) (**deliverable**)
- `context/art/visual-direction.md`
- `context/story/story-vision.md`
- `context/formats/world-forge-map.md`
- Pair: TICKET-0030; feeds TICKET-0032

## Acceptance criteria

- [x] Biome/region types for Tessera tone without photoreal requirement.
- [x] Landmark rules (lights, silhouettes, value) tied to visual direction.
- [x] Density guidance bands across 4×4 km.
- [x] Notes mapping to World Forge region/POI IDs (no new schema).
- [x] `epics.md` Notes link the doc.

## Out of scope

- Final art production, heightmap sculpting, or World Forge UI.
- Streaming/LOD budgets (TICKET-0032).

## Dependencies

- Soft pair with TICKET-0030.
- Blocks clearer TICKET-0032 acceptance.

## Verification

- Doc-only review; cross-check against visual-direction constraints.

## What changed

- Summary: Defined biome bands (farmland, woods, highland, rock/mountain, settled, fortress/ruin, chaotic), landmark readability rules, and qualitative density bands (hub core → wilderness weave → chaotic pockets) with rough 4×4 km targets. Mapped concepts onto existing World Forge region/POI/link fields.
- Files / surfaces: created `context/features/map-design-language.md`; updated `features/index.md`, `epics.md`, open-questions, this stub.
- Schema / API: none; uses existing `map.worldforge.json` kinds/tags.
- Seed / sample data: none.
- Tests / verification: doc review vs visual-direction + ticket acceptance.
- Decisions & tradeoffs: Kept bands qualitative; deferred numeric spawn budgets to TICKET-0032.
- Leftover risk: Hub count and snow-in-v1 remain open preferences.

## Agent notes

Delivered with TICKET-0030 in the same session.
