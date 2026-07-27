# Scene / Faction Concept Art

Draft visual targets for production. Not runtime assets.

## Act 0 — Landfall (level design)

Style target: Unturned-inspired flat-shaded low-poly ([visual-direction.md](../visual-direction.md), [theme-palette.md](../theme-palette.md)). Beats: [campaign-beat-sheet.md](../../story/campaign-beat-sheet.md). Prop kit: [blockbench-asset-list.md](../blockbench-asset-list.md) Tier 2.

| File | Beat | Level-design notes |
| --- | --- | --- |
| `act0-a0-03-approach-arkand.png` | A0-03 | Landlocked forest approach; Calrenoth silhouette + siege fire; wheelbarrow / road clutter staging for Arkand rescue |
| `act0-a0-04-gate-under-fire.png` | A0-04 | Combat corridor to gate; barricades, catapults, Imperium banners; warm torch landmarks on gate |
| `act0-a0-05-grenge-courtyard.png` | A0-05 | Enclosed briefing courtyard; command table, crates, torches; green-shroud Grenge read |
| `act0-a0-06-drawbridge-defense.png` | A0-06 | Rear hold point; moat-scale drawbridge + chain spool kit; signal pyre; land spur beyond (not a sea bridge) |
| `act0-a0-07-luceran-shadow.png` | A0-07 | Black void fog overrun; pale rider Luceran; collapse beat — soft-gated siege climax mood |
| `act0-a0-08-creotar-vision.png` | A0-08 | Rare vision instance (Realm of Darkness); Creotar light-being vs player; not seamless overland |
| `act0-a0-09-survivor-camp.png` | A0-09 | Post-Act 0 DAO-style camp tutorial; warm fire landmark; travel-only safe hub |

## Faction / character sheets

| File | Notes |
| --- | --- |
| `chaotic-imperium-soldiers-concept.png` | Imperium footsoldier motifs (starburst, spiked halo, void fog) — denser than final low-poly bake; use for faction identity, not mesh density |

## Starter weapons & Act 0 items

Draft prop targets for archetype defaults + thin Landfall loot ([gearing-system.md](../../features/gearing-system.md), `art_starter_weapons`). Soft affinity only (DEC-0048). Proposed ids from display titles.

| File | Item | Lane / source |
| --- | --- | --- |
| `starter-ashfell-arming-sword.png` | Ashfell Arming Sword | Starter — Ashfell Blade |
| `starter-outrider-shortbow.png` | Outrider Shortbow | Starter — Outrider |
| `starter-runecaster-rune-focus.png` | Guild Rune Focus (inscribed token) | Starter — Runecaster |
| `act0-field-bandage.png` | Field Bandage | Act 0 common / drop |
| `act0-soldiers-scrap-pouch.png` | Soldier's Scrap Pouch | Act 0 approach find |
| `act0-obscure-vein-pendant.png` | Vein-Iron Pendant | Act 0 optional obscure rare |

## Usage

- Prefer these for **layout, kit list, lighting landmarks, and mood** when blocking Calrenoth.
- Mesh density should follow oak / Tier 1 prop sheets more closely than the Imperium character sheet.
- Geography locks: western peninsula tip, landlocked player entrance, moat-scale drawbridge to another land spur ([official-world-map.md](../../story/official-world-map.md)).
- Interactive boards (workspace `canvases/`): `act0-landfall-scenes.canvas.tsx` (beat flow), `starter-weapons-items.canvas.tsx` (weapon/item concepts).
