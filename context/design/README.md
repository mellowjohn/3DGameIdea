# Design Docs

Working design surfaces for humans (especially Dom + owner). Not runtime contracts — those live under `formats/` and `features/`.

## Tabs

| Tab | Purpose |
| --- | --- |
| [Dom Open Questions](dom-open-questions.md) | Prioritized P0/P1/P2 lore & geography leftovers — Act 0 Landfall mostly answered 2026-07-29; Act 1 coastal next |
| [Dom Answered Questions](dom-answered-questions.md) | Archive of Dom + owner locks (do not re-litigate) |
| [World Forge Map Canvas](world-forge-map-canvas.pen) | Pencil map-canvas layout exploration |
| [RPG Engine UI](rpg-engine-ui.pen) | Pencil exploration of full editor shell + player HUD/modals (World Forge chrome language) |
| [Dialogue UI](dialogue-ui.pen) | Pencil: line/choices/prompt + research pass (AA chips, keys, portrait, prompt strip, history/Esc, settings) |
| [Quest UI](quest-ui.pen) | Pencil: quest flow + minimap/full-map navigation · [DEC-0051](../decisions/index.md#dec-0051-no-xp-power-progression-and-quest-ux) |
| [Inventory UI](inventory-ui.pen) | Pencil: bag / equip / hotbar / quest bag / camp chest · [DEC-0050](../decisions/index.md#dec-0050-inventory-ux-item-kinds-and-positive-soft-affinity) · REVIEW draft |
| [Quest assets](quest-assets/) | Transparent quest markers (`?`/`!`, pins, minimap dots, offscreen chevron) → `assets/ui/quest/` |
| [Quest UI recording](recording_quest_ui_progression_2026-07-29.md) | 2026-07-29 Dom+John: journal kinds draft, accept UX |
| [Power progression recording](recording_archetype_quests_power_progression_2026-07-29.md) | 2026-07-29: locks → DEC-0051 |
| [LD + character concepts recording](recording_ld_character_concepts_2026-08-03.md) | 2026-08-03 Dom+John: Act 0–3 LD/scene/cast review; camp Palworld lean; kit feedback → TICKET-0254–0256 |
| [Player HUD](player-hud.pen) | Pencil: Dragon Age–inspired combat HUD + asset board (vector chrome) |
| [Act 0 Menu / Creation](act0-menu-creation-ui.pen) | Pencil v3.2: zoned canvas (**APPROVED** / **REVIEW** / **REFERENCE** / **LEGACY**) · color sticker sheet · main menu · cinematic prologue · cathedral class glass · landscape difficulty glass · appearance + medallion · Settings |
| [HUD assets](hud-assets/) | Concept PNGs for combat HUD chrome |
| [Dialogue assets](dialogue-assets/) | Concept PNGs for dialogue chrome (same UI language) |
| [Menu assets](menu-assets/) | Concept PNGs for main menu / creation chrome + transparent title logo |

Shown in the engine **Design Docs** viewport tab (section `design`) alongside features/story/art markdown. Open **Dom Open Questions** and use the **Form** radio to answer + Submit (writes the markdown). Markdown preview supports horizontal scroll for wide tables.

## Related

- In-game UI chrome: [`../art/ui-chrome-direction.md`](../art/ui-chrome-direction.md)
- Story canon: [`../story/index.md`](../story/index.md)
- Engine/product open questions: [`../interviews/open-questions.md`](../interviews/open-questions.md)
- Cartography language: [`../art/cartography-design.md`](../art/cartography-design.md)
- Official map theaters: [`../story/official-world-map.md`](../story/official-world-map.md)
