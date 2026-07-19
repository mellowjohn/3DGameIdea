# World Forge Scope Boundary

Status: product intent accepted; editor Viewports tab shipped (TICKET-0015/0016 list+detail; TICKET-0017 Relationships **Graph** canvas) — see [World Forge editor UI](editor-mvp.md#world-forge-editor-ui)  
Ticket: [TICKET-0010](../planning/tickets/TICKET-0010.md) · Epic: EPIC-0002  
Decisions: [DEC-0019](../decisions/index.md#dec-0019-world-forge-editor-home-and-story-canon-split), [DEC-0020](../decisions/index.md#dec-0020-world-forge-narrative-tooling-umbrella)

World Forge is the **narrative tooling home** for Tessera: it encapsulates lore and story vision as engine-integrable, diffable data and is where relationship, map, quest, dialogue, and story-event authoring live. It does **not** replace Scene/Sculpt terrain, prefab placement, materials, or live MCP scene/terrain apply paths.

## Product purpose

| Intent | Meaning |
| --- | --- |
| Encapsulate lore & story vision | Turn `context/story/` canon into structured project assets the engine and tools can validate, reference, and automate |
| Integrate story into the engine | Stable IDs link narrative entities to regions, POIs, quests, dialogue, events, and (by reference) scene entities/prefabs |
| One authoring umbrella | Relationship graph, map/story geography, quests, dialogue, and story events share one World Forge product surface inside the editor |

Milestone delivery for quests/dialogue runtime may still track **M6/M7** (EPIC-0006), but those **editors and schemas product-belong to World Forge**, not a separate tooling brand.

## Product home

World Forge lives as **mode(s) / dockable panels inside the integrated editor** (alongside Scene / Sculpt / Game), not a separate application. All World Forge mutations must be **command-backed** per [DEC-0003](../decisions/index.md#dec-0003-automation-first-tools).

## Canon split

| Layer | Owns | Does not own |
| --- | --- | --- |
| `context/story/*.md` | Narrative canon (premise, faction prose, beat sheets, continuity) | Runtime schemas, editor commands |
| World Forge project data | Diffable structured authoring keyed to story IDs — people, factions, clans/cultures, regions/POIs, relationships, quests, dialogue graphs, story events | Replacing story markdown as the lore essay source of truth |

Story markdown remains the **human/agent narrative source of truth**. World Forge JSON is the **engine/integration layer** keyed to those IDs ([DEC-0019](../decisions/index.md#dec-0019-world-forge-editor-home-and-story-canon-split)).

## World Forge owns (product intent)

### Core (near-term EPIC-0002)

1. **Hierarchy authorship** — Religion (pantheon), Factions, and Persons as tree+detail pages under the Hierarchy tab ([DEC-0035](../decisions/index.md#dec-0035-world-forge-hierarchy-authorship)). Formats: [`../formats/world-forge-pantheon.md`](../formats/world-forge-pantheon.md), [`../formats/world-forge-factions.md`](../formats/world-forge-factions.md); persons via relationship nodes + `parentId` ([`../formats/world-forge-relationships.md`](../formats/world-forge-relationships.md)). Companions are a Persons filter (`companion` tag), not a separate tab.
1b. **Archetype catalog** — Starting/advanced player archetypes as a top-level World Forge pane ([DEC-0009](../decisions/index.md#dec-0009-starting-archetype-character-creation)). Format: [`../formats/world-forge-archetypes.md`](../formats/world-forge-archetypes.md) (TICKET-0186). Optional unlock stubs (`moralityThreshold`, `factionId`); starter kit prefab refs. Not character-creation appearance UI.
2. **Relationship graph + editor** — Nodes for people, deities, artifacts (and related narrative entities); typed edges (ally, rival, member-of, etc.) including faction-id endpoints; graph view and edit surface. Format + validation: [`../formats/world-forge-relationships.md`](../formats/world-forge-relationships.md) (TICKET-0012); editor is first-class World Forge UI follow-on.
3. **Faction / culture / clan structured authoring** — Diffable data aligned with [`factions.md`](../story/factions.md); format: [`../formats/world-forge-factions.md`](../formats/world-forge-factions.md) (TICKET-0011). Authored under Hierarchy → Factions.
4. **Regions, POIs, and map links** — Story geography: named regions, POIs, travel/soft-gate links, map metadata, optional world-space **anchors**. Format: [`../formats/world-forge-map.md`](../formats/world-forge-map.md) (TICKET-0013). Editor Map **Canvas** (TICKET-0187/0188) authors anchors on an XZ overlay (optional terrain height underlay). Scene/Sculpt can show the same anchors as editor-only marker poles (TICKET-0190). IDs and links only — **not** mesh placement. Act lens ([DEC-0036](../decisions/index.md#dec-0036-world-forge-act-lens), [`../formats/world-forge-acts.md`](../formats/world-forge-acts.md)) filters geography by campaign act without splitting files. **Hydrology layout** (river/lake/sea regions, ferry route polylines linked to dock POIs) is planned on the Map canvas per [DEC-0038](../decisions/index.md#dec-0039-water-swim-and-hydrology-authoring); water **meshes** remain Sculpt-owned.
5. **World Forge MCP/CLI entry points** — Command-backed automation for the above after schemas stabilize (TICKET-0014; pantheon via TICKET-0183).

### Narrative systems (product home; delivery via EPIC-0006 / M6–M7)

6. **Quest authoring** — Quest data model, validation, and quest creator tooling (TICKET-0050 / 0051). Lives under World Forge panels; links to regions, POIs, factions, and people in the relationship graph. Format: [`../formats/world-forge-quests.md`](../formats/world-forge-quests.md) (multi-stage dialogue hooks per [DEC-0026](../decisions/index.md#dec-0026-quest-owned-dialogue-hooks-multi-stage)).
7. **Dialogue authoring** — Branching dialogue graphs, editor, and headless tests (TICKET-0052 / 0053; UX polish TICKET-0165–0168 Phase 1; follow-ons 0169–0179). Speakers and conditions reference World Forge people/faction IDs; trees may set `parentQuestId` when owned by a quest stage. Shared graph camera with relationship canvas ([DEC-0027](../decisions/index.md#dec-0027-shared-world-forge-graph-camera)).
8. **Story events** — Authorable event definitions (triggers, conditions, outcomes) that wire campaign beats into engine-consumable assets. Exact schema is a follow-on ticket; product home is World Forge.

### Integration contract

World Forge’s job is to make story vision **easy to integrate**: every authored narrative object has a stable ID, validation, and reference rules so runtime, MCP, and later vertical-slice systems resolve the same graph of meaning.

## Explicit non-overlap (Scene / Sculpt / MCP owned)

Do **not** implement these as World Forge duplicates:

| Capability | Owner today | Tools / docs |
| --- | --- | --- |
| Terrain height sculpt / material / foliage paint | Editor Sculpt + MCP | `engine_terrain_apply`; [terrain-authoring.md](terrain-authoring.md) |
| Water surface sculpt / fill | Editor Sculpt + MCP (planned) | `engine_water_apply` or terrain extension; [water-hydrology.md](water-hydrology.md) |
| Mesh / prefab placement, move, remove, transforms | Editor Scene + MCP | `engine_scene_apply`; [world-placement.md](../formats/world-placement.md) |
| Live scene / entity component apply | Editor + MCP | `engine_scene_apply`, `engine_entity_component_apply` |
| Prefab create/edit and prefab components | Asset Browser / Prefab Editor + MCP | `engine_prefab_apply`, `engine_prefab_component_apply` |
| Material inspect / save | Editor + MCP | `engine_asset_apply`; [materials.md](../formats/materials.md) |
| Lua handler bodies | MCP / file monitor | `engine_lua_apply` |
| Open-world partition addressing / cell format | World runtime | Unchanged by World Forge |
| Work routing classifier | Automation | `engine_scene_plan`; [content-vs-engine-workflows.md](../architecture/content-vs-engine-workflows.md) |

**POI / character rule:** World Forge stores narrative IDs, labels, graph membership, optional world-space anchors, and **references** to scene entity UUIDs or prefab asset IDs. Creating or moving placed meshes remains Scene/MCP work.

## Command-backed vs pure data (DEC-0003)

| Surface | Kind | Tracking |
| --- | --- | --- |
| Faction / culture / clan schemas + validation | **Pure data** first | TICKET-0011 |
| Relationship graph format + validation | **Pure data** first | TICKET-0012 |
| Map assets (regions, POIs, links) | **Pure data**, then **command-backed** UI | TICKET-0013 |
| World Forge MCP/CLI for core ops | **Command-backed** | TICKET-0014 (`engine_world_forge_apply`) |
| Relationship graph editor UI | **Command-backed** | Follows 0012; same command path as MCP |
| Quest model + quest creator | **Pure data** then **command-backed** UI | TICKET-0050 / 0051 (World Forge home); format [`world-forge-quests.md`](../formats/world-forge-quests.md) |
| Dialogue runtime + graph editor | **Pure data** then **command-backed** UI | TICKET-0052 / 0053 (World Forge home) |
| Story event schemas + editor | **Pure data** then **command-backed** UI | Follow-on under EPIC-0002/0006 |

Schema tickets ship formats and validators before UI. UI and MCP must share the same mutation commands.

## Out of scope (not World Forge)

- Player HUD mini-map **rendering** (EPIC-0007) — consumes World Forge region/POI IDs later
- Melee vertical slice, VFX, animation runtime polish (M5–M9 systems work)
- Replacing Scene/Sculpt for world building geometry
- Inventing quest/dialogue **runtime** ahead of M5 exit without owner override (milestone hold still applies; product ownership does not)

## Follow-on order

1. TICKET-0021 faction canon gaps (owner approval) → TICKET-0011 clarity.
2. TICKET-0011 → TICKET-0012 (faction data, then relationship graph format).
3. TICKET-0013 map authoring; relationship **editor** UI after 0012 format lands.
4. TICKET-0014 MCP/CLI for core World Forge ops.
5. EPIC-0006 (M6/M7): quest + dialogue under World Forge panels; story-event schema ticket when ready.
6. Mini-map (EPIC-0007) after region/POI IDs exist.

## Related context

- [world-forge-factions.md](../formats/world-forge-factions.md) — faction/culture/clan schema (TICKET-0011)
- [world-forge-archetypes.md](../formats/world-forge-archetypes.md) — starting/advanced archetype catalog (TICKET-0186)
- [world-forge-relationships.md](../formats/world-forge-relationships.md) — relationship graph schema (TICKET-0012)
- [world-forge-map.md](../formats/world-forge-map.md) — regions / POIs / links schema (TICKET-0013)
- [world-forge-mcp.md](../formats/world-forge-mcp.md) — `engine_world_forge_apply` (TICKET-0014)
- [content-vs-engine-workflows.md](../architecture/content-vs-engine-workflows.md) — C++ vs MCP routing
- [editor-mvp.md](editor-mvp.md) — Scene / Sculpt / Game tabs World Forge sits beside
- [mcp-live-editor.md](mcp-live-editor.md) — current MCP tool set
- [open-questions.md](../interviews/open-questions.md) — remaining World Forge design questions
- EPIC-0003 — story canon content that World Forge structures
- EPIC-0006 — quest/dialogue milestone delivery hosted in World Forge
