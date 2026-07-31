# Scene / Faction Concept Art

Draft visual targets for production. Not runtime assets.

## Act 0 — Landfall (level design)

Style target: Unturned-inspired flat-shaded low-poly ([visual-direction.md](../visual-direction.md), [theme-palette.md](../theme-palette.md)). Beats: [campaign-beat-sheet.md](../../story/campaign-beat-sheet.md). Prop kit: [blockbench-asset-list.md](../blockbench-asset-list.md) Tier 2.

| File | Beat | Level-design notes |
| --- | --- | --- |
| `act0-main-menu.png` | Menu | Siege establishing shot of burning Calrenoth; quieter left-center for main-menu panel overlay; Imperium banners + approach clutter |
| `wrathful-conquest-title-logo.png` | Menu | Game title logo (transparent PNG). Opaque pre-punch backup: `wrathful-conquest-title-logo.opaque.png`. Used on main-menu mock in [`../../design/act0-menu-creation-ui.pen`](../../design/act0-menu-creation-ui.pen). |
| `act0-a0-02-character-creation.png` | A0-02 | Courtyard backdrop for appearance pedestal mock; Blade / Outrider / Runecaster standees remain in shot; UI overlays in [`../../design/act0-menu-creation-ui.pen`](../../design/act0-menu-creation-ui.pen) |
| `act0-class-cathedral-glass-wall.png` | A0-02 class | Cathedral wall / empty stained-glass frames — class-select backdrop (extended black, thin border) |
| `act0-difficulty-glass-landscape.png` | A0-02 difficulty | Stained-glass Tessera landscape mural — difficulty-select backdrop |
| `act0-prologue-01-throne-pullback.png` | A0-01 | Prologue cinematic still 1 — Luceran throne pullback |
| `act0-prologue-02-whisper-glass.png` | A0-01 | Prologue still 2 — earlier regen pass (superseded) |
| `act0-prologue-02-whisper-luceran.png` | A0-01 | Prologue cinematic still 2 — Frangitur whisper; Luceran alone (regen, no second figure) |
| `act0-prologue-03-address-flame.png` | A0-01 | Prologue cinematic still 3 — address adventurers / flame |
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

Starting archetype **kit turnarounds** (clothing over shared base body) live under [`../reference/`](../reference/) — see [character-direction.md](../character-direction.md):

| File | Kit |
| --- | --- |
| `../reference/starting-player-ashfell-blade-turnaround.png` | Ashfell Blade |
| `../reference/starting-player-outrider-turnaround.png` | Outrider |
| `../reference/starting-player-runecaster-turnaround.png` | Runecaster |

## Starter weapons & Act 0 items

Draft prop targets for archetype defaults + thin Landfall loot ([gearing-system.md](../../features/gearing-system.md), `art_starter_weapons`). Soft affinity + inventory UX: DEC-0048 / DEC-0050. Ids from display titles.

| File | Item | Id | Lane / source |
| --- | --- | --- | --- |
| `starter-ashfell-arming-sword.png` | Ashfell Arming Sword | `ashfell_arming_sword` | Starter — Ashfell Blade |
| `starter-outrider-shortbow.png` | Outrider Shortbow | `outrider_shortbow` | Starter — Outrider |
| `starter-runecaster-rune-focus.png` | Guild Rune Focus (inscribed token) | `guild_rune_focus` | Starter — Runecaster |
| `act0-field-bandage.png` | Field Bandage | `field_bandage` | Act 0 common / drop |
| `act0-soldiers-scrap-pouch.png` | Soldier's Scrap Pouch | `soldiers_scrap_pouch` | Act 0 approach find |
| `act0-obscure-vein-pendant.png` | Vein-Iron Pendant | `vein_iron_pendant` | Act 0 obscure rare (**ships**, DEC-0050) |

Additional Act 0 defs (ids in [gearing-system.md](../../features/gearing-system.md); concept art optional): `siege_tonic`, `crude_arrow`, `arkands_favor`, `muddied_keep_ring`, `imperium_footsoldier_badge`.

## Usage

- Prefer these for **layout, kit list, lighting landmarks, and mood** when blocking Calrenoth (and for main-menu / creation UI backdrop framing).
- Mesh density should follow oak / Tier 1 prop sheets more closely than the Imperium character sheet.
- Geography locks: western peninsula tip, landlocked player entrance, moat-scale drawbridge to another land spur ([official-world-map.md](../../story/official-world-map.md)).
- Interactive boards (workspace `canvases/`): `act0-landfall-scenes.canvas.tsx` (menu → creation → beat flow), `starter-weapons-items.canvas.tsx` (weapon/item concepts).
