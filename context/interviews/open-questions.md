# Open Questions

No question blocks milestones 1 or 2. Re-run the project interview skill before decisions that expand shipping platforms, online services, world scale, mod support, or multiplayer.

## Entity component authoring

Resolved 2026-07-13 as [DEC-0016](../decisions/index.md#dec-0016-entity-attached-components-and-dual-mcp-apply-paths) and [DEC-0017](../decisions/index.md#dec-0017-prefab-and-scene-component-authoring-with-unity-like-inheritance): Add Component on prefab assets and scene entities; Unity-like prefab→instance inherit/override; MCP dual path (dedicated tools + `engine_prefab_apply` / `engine_scene_apply`). Implementation: TICKET-0147. No open blockers for this topic.

## Prefab composition (non-blocking)

These do not block starting v2 compositional prefab work under [DEC-0008](../decisions/index.md#dec-0008-compositional-prefab-meshes-from-primitives):

- Default low-poly segment counts per primitive (`cube`, `pyramid`, `cylinder`, `sphere`)
- Whether nested entity hierarchies inside a prefab should inherit part transforms before world placement
- Whether the editor should offer optional bake-to-glTF export for compositional prefabs

## Story / faction canon (blocks World Forge schema TICKET-0011; soft-blocks mid-campaign beats)

Filed from TICKET-0021 review. Full gap list: [`context/story/factions.md`](../story/factions.md#gaps-blocking-world-forge-schema-ticket-0011-and-mid-campaign-beats). Do not invent answers in schema or beat docs.

- Luceran–Frangitur–Shroud: what grants Chaotic Imperium command, and how much agency Luceran retains?
- Cristallo: faith, hierarchy, politics, and relationship to Creo before the fall?
- Arrotrebae: council/decision rules when chieftains disagree; named tribes for relationship graph?
- Orc warbands: stable names/IDs; which warband held the Nefarium Shroud and why; name of the slain great leader?
- Player influence: **model** resolved as [DEC-0029](../decisions/index.md#dec-0029-continuous-faction-standing-with-hostility-transfer) (continuous standing + hostility transfer + lock-in fields). **Numeric thresholds** for Cristallo/Arrotrebae lock-in, destruction/reform, and morality binding remain open.
- Kingdom of Tessera: playable faction choice vs political arena around other factions?
- Campaign structure: reconcile draft chapter-locked flow with seamless open world ([DEC-0001](../decisions/index.md#dec-0001-product-and-platform-target)) — also in [`story-vision.md`](../story/story-vision.md).

**Update 2026-07-15:** Open-world vs chapter-lock **resolved** as [DEC-0021](../decisions/index.md#dec-0021-soft-gates-with-rare-optional-instances). Beat sheet: [`campaign-beat-sheet.md`](../story/campaign-beat-sheet.md). Remaining story opens from Act 0 Twine:

- Creotar vs Creo/Frangitur; destroy-Shroud guidance vs Frangitur “rip Tessera apart” irony
- Crystal location (Twine stubs empty); who holds it
- Wake-up / O’hlundian evergreens vs first hub after Calrenoth
- Wild God revival chronology vs Calrenoth Act 0 spine
- Acts 1–4 not yet in Twine (planning beats only)
- Which consequential side quests ([side-quest-catalog.md](../story/side-quest-catalog.md)) can flip faction lock-in vs only nudge standing (blocked on morality thresholds)

## Campaign gating (TICKET-0020)

Resolved 2026-07-15 as [DEC-0021](../decisions/index.md#dec-0021-soft-gates-with-rare-optional-instances): soft gates on seamless world + rare optional instances (dungeons/set pieces/visions); minimize load screens. Beat sheet: [`campaign-beat-sheet.md`](../story/campaign-beat-sheet.md).

Still open from beat sheet / Act 0 Twine import:

- Wake-up / camp location after Creotar vision.
- Whether Calrenoth remains ruined on the seamless map after Act 0.
- Creotar identity vs Creo/Frangitur.
- Wild God revival chronology vs Calrenoth Act 0 spine.
- Vanessa introduction beat timing.
- Morality thresholds and ending matrix (Act 4).
- Twine draft orc names (Grul’thaz / Shadowpaw) — not established until owner review (also TICKET-0021).
- Act 0 supporting cast (Grenge / Larrell / Damius) survival and later-act roles — draft nodes in relationships asset; still open for continuity.

## UI canvas scale modes (non-blocking)

v1 letterbox/pillarbox is locked in [DEC-0025](../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp). Owner wants a later option to **scale to viewport edges** (no side bars). Open when scheduling that follow-on:

- Per-canvas vs global scale mode (`letterbox` | `fill_edges` / cover / stretch)
- Whether HUD layer and modal screens can differ (edge-flush HUD + letterboxed pause)

## World Forge scope (TICKET-0010)

Resolved 2026-07-15 as [DEC-0019](../decisions/index.md#dec-0019-world-forge-editor-home-and-story-canon-split) + [DEC-0020](../decisions/index.md#dec-0020-world-forge-narrative-tooling-umbrella): editor-home panels; story markdown canon + World Forge JSON keyed to IDs; narrative tooling umbrella (relationship graph/editor; quests/dialogue/story events product-home). Full boundary: [`context/features/world-forge-scope.md`](../features/world-forge-scope.md).

Open (do not invent in TICKET-0011–0014 without owner input):

- Project path layout for World Forge assets (e.g. under `assets/` vs project root) and naming convention.
- POI anchoring model: world-space coordinates only, references to scene entity UUIDs, or both (and which is authoritative for mini-map).
- Whether region polygons/cells must align to 128 m partition cells or may be freeform overlays.
- Story-event schema shape (triggers, conditions, outcomes) and how it binds to beat-sheet IDs from EPIC-0003.
- Relationship edge type vocabulary (minimum set for v1 graph editor).
