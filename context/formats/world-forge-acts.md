# World Forge Act lens (`acts`)

Status: active — TICKET-0189 · Epic EPIC-0002 · [DEC-0036](../decisions/index.md#dec-0036-world-forge-act-lens)

Campaign acts organize World Forge authoring without splitting assets into per-act files. Acts remain narrative arcs on the seamless world ([DEC-0021](../decisions/index.md#dec-0021-soft-gates-with-rare-optional-instances)).

## Canonical act ids

| Id | Label | Beat sheet |
| --- | --- | --- |
| `act0` | Act 0 — Landfall | Fall of Calrenoth |
| `act1` | Act 1 | **Ledgeport** hub / open world |
| `act2` | Act 2 | Faction war |
| `act3` | Act 3 | Usurper crisis |
| `act4` | Act 4 | Endings |

## Field

Optional `acts: string[]` on:

- Quests (`quests.worldforge.json`)
- Dialogue trees (`dialogues.worldforge.json`)
- Map regions and POIs (`map.worldforge.json`)
- Relationship nodes (`relationships.worldforge.json`)

Empty / omitted `acts` = **campaign-wide** (visible under every act filter).

Legacy: tags named `act0`..`act4` still count as act membership for filtering (backward compatible). Prefer the `acts` field for new authoring.

## Editor lens

World Forge toolbar **Act** combo: `All acts` | `Act 0` … `Act 4`.

Filters lists/canvases for Map, Quests, Dialogues, Hierarchy → Persons, and Relationships graph/nodes.

Does **not** filter Hierarchy → Religion/Factions or the Archetypes pane (campaign-wide catalogs).

## Validation

Unknown act ids → `WORLD-FORGE-ACT-ID`.

## Helpers

`include/engine/assets/world_forge_acts.h` — `matches_world_forge_act_filter`, `resolve_world_forge_acts`, `validate_world_forge_acts`.

## Related

- [`campaign-beat-sheet.md`](../story/campaign-beat-sheet.md)
- Format notes in quests / dialogues / map / relationships docs
