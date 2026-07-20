# TICKET-0205: Cartography design language + art kit + culture typography

- Epic: EPIC-0002
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: (see Notion Tickets DB by Ticket ID)

## Goal

Ship a durable fantasy cartography design language, icon/heraldry kit, and SIL OFL culture typefaces for Map Canvas labels and the future player map.

## Context links

- `context/art/cartography-design.md`
- `context/art/visual-direction.md`
- `context/features/map-design-language.md`
- `assets/ui/fonts/CARTOGRAPHY_FONTS.md`
- Related: TICKET-0206, TICKET-0207, TICKET-0061

## Acceptance criteria

- [ ] `cartography-design.md` covers aesthetic, settlement/landmark icons, heraldry, culture typography, travel grades, borders, scale layers
- [ ] Icon sheets + heraldry + stroke refs under `context/art/cartography/` with runtime copies
- [ ] Culture OFL fonts packaged and listed in `resources/index.md`
- [ ] Linked from visual-direction / map-design-language / official-world-map / README

## Out of scope

Player HUD mini-map rendering; inventing place names; Scene mesh placement.

## Dependencies

Soft: TICKET-0031 map design language; TICKET-0187 Map Canvas.

## Verification

Doc review; assets present on disk; license rows in resources index.

## Notion

- https://app.notion.com/p/3a3d3efc56958113ae40f54fca4ec77f

## What changed

- Summary: Added fantasy cartography design language, procedural icon/heraldry/stroke kit, and SIL OFL culture map fonts (Forum, EB Garamond, Uncial Antiqua, Metamorphous) with Cinzel default.
- Files: `context/art/cartography-design.md`, `context/art/cartography/*`, `samples/open-world-rpg/assets/ui/cartography/*`, `assets/ui/fonts/{forum,eb-garamond,cormorant-garamond,uncial-antiqua,metamorphous}/`, `assets/ui/fonts/CARTOGRAPHY_FONTS.md`, resources/visual-direction/map-design links.
- Verification: assets on disk; licenses recorded.

## Agent notes

Implementing with TICKET-0206 / 0207 as one cartography pass.
