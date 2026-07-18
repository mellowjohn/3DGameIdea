# Decision Log

Accepted decisions are append-only. A later decision may supersede an earlier one.

## Template

### DEC-0000: Short title

- Status: proposed | accepted | superseded
- Date: YYYY-MM-DD
- Context: What forced the decision.
- Decision: The selected approach.
- Rationale: Why it was selected.
- Consequences: Important tradeoffs and follow-up work.
- Supersedes: Decision ID or none.

## Decisions

### DEC-0001: Product and platform target

- Status: accepted
- Date: 2026-07-02
- Context: The engine needs a concrete product and release target.
- Decision: Build a Windows 10/11, single-player, offline, third-person action RPG engine for a seamless 4×4 km open world.
- Rationale: A game-specific target keeps the engine testable and prevents general-purpose scope expansion.
- Consequences: Multiplayer, consoles, non-Windows releases, and runtime generative AI are outside v1.
- Supersedes: none

### DEC-0002: Native implementation stack

- Status: accepted
- Date: 2026-07-02
- Context: Runtime, graphics, scripting, and dependency boundaries determine the entire architecture.
- Decision: Use C++20, CMake, pinned vcpkg manifests, Direct3D 12, a hybrid ECS/hierarchy, and Lua for content-heavy gameplay behavior. Focused third-party libraries may provide commodity foundations.
- Rationale: This provides native performance and established Windows graphics tooling while preserving custom engine architecture.
- Consequences: Memory safety, ownership, ABI boundaries, dependency licenses, and script containment require explicit validation.
- Supersedes: none

### DEC-0003: Automation-first tools

- Status: accepted
- Date: 2026-07-02
- Context: Humans and AI agents must operate and test the same tools reliably.
- Decision: Make project data diffable and expose engine/editor operations through stable headless commands with human and JSON output.
- Rationale: GUI-only workflows cannot be reproduced or validated reliably by automated agents.
- Consequences: Editor operations must be command-backed, deterministic where possible, undoable, and noninteractive when requested.
- Supersedes: none

### DEC-0004: Diagnostics and performance contract

- Status: accepted
- Date: 2026-07-02
- Context: “Great error handling” and “works well” need measurable behavior.
- Decision: Use typed recoverable errors, structured local diagnostics, Windows minidumps, and a 1440p/60 FPS target on an RTX 4070-, Ryzen 7 5800X-, 32 GB-, NVMe-class PC.
- Rationale: Failures must be actionable without hosted telemetry, and performance claims require a reproducible baseline.
- Consequences: Subsystems receive explicit error categories and frame-time budgets; hosted crash upload remains deferred.
- Supersedes: none

### DEC-0005: RPG content and collision systems

- Status: accepted
- Date: 2026-07-02
- Context: The RPG requires production authoring and runtime support beyond movement and basic combat.
- Decision: Treat animation, dialogue, particle effects, and collision as first-class engine systems with editor tooling, versioned assets, diagnostics, automation commands, and tests.
- Rationale: These systems drive combat correctness, narrative delivery, visual feedback, and world interaction and cannot remain game-specific afterthoughts.
- Consequences: Asset compilation, hot reload, save state, sequencing, localization hooks, performance budgets, visualization, and graceful missing-resource behavior must cover these systems.
- Supersedes: none

### DEC-0006: Smooth low-poly art direction

- Status: accepted
- Date: 2026-07-02
- Context: Terrain representation must be chosen before terrain rendering, collision, navigation, foliage, and authoring tools are expanded.
- Decision: Target an Unturned-inspired blocky stylized art direction using smooth low-poly heightfield terrain with modular low-poly props, presented through a dark-fantasy atmosphere. Do not build v1 terrain as stepped blocks or an editable voxel world.
- Rationale: Smooth terrain supports natural traversal, roads, navigation, and third-person combat while keeping asset production achievable for a small team with limited art capacity.
- Consequences: Terrain tooling prioritizes sculpted heightfields, deliberately coarse tessellation, simple material regions, flat or restrained shading, stylized foliage, and strong silhouettes. Lighting uses cold ambient tones, warm local focal lights, fog, and selective supernatural accents. Runtime voxel editing and cube-by-cube terrain meshing are out of scope.
- Supersedes: none

### DEC-0007: Commercial-use resource licensing

- Status: accepted
- Date: 2026-07-02
- Context: The engine and game must be commercially usable and safely modifiable without later replacing resources of uncertain provenance.
- Decision: Prefer permissive open-source dependencies and use only resources whose recorded terms permit the project's intended commercial use, modification, and redistribution. Reject noncommercial, personal-use-only, editorial-only, no-derivatives, attribution-unknown, and provenance-unknown resources. Strong copyleft or source-distribution obligations require a separate accepted decision before use.
- Rationale: License review at intake prevents legal and production risk from becoming embedded in code, content, builds, and marketing material.
- Consequences: Every external code or content resource requires provenance, author/source, version, license identifier or license-file reference, modification status, attribution requirements, and distribution notes. Required Windows, Direct3D, GPU-driver, and platform SDK/runtime components are documented platform exceptions and are not represented as open source or project-modifiable.
- Supersedes: none

### DEC-0008: Compositional prefab meshes from primitives

- Status: accepted
- Date: 2026-07-02
- Context: Early sample props (trees, campfires) need fast iteration without external DCC tools or one-off procedural importers per asset. The low-poly art direction favors simple silhouettes built from basic shapes.
- Decision: Prefabs may be authored from multiple mesh parts. Each part is either a referenced glTF/GLB asset or a built-in low-poly primitive (`cube`, `pyramid`, `cylinder`, `sphere`) with a local transform relative to the prefab root. Parts are movable building blocks used to compose props such as trees and campfires. A prefab may mix primitives and imported meshes in the same definition.
- Rationale: Compositional authoring matches the stylized blocky look, keeps assets diffable and automation-friendly, and avoids bespoke C++ mesh generators for every early prop.
- Consequences: Prefab schema, editor placement/picking, renderer import, validation, and tests must treat multi-part prefabs as first-class. The current single top-level `mesh` field remains supported for simple v1 prefabs. Implementation adds per-entity or per-part mesh descriptors, primitive tessellation in the mesh pipeline, and aggregated bounds for selection and collision. Baked glTF export may be added later but is not required for v1 compositional authoring.
- Supersedes: none

### DEC-0009: Starting archetype character creation

- Status: accepted
- Date: 2026-07-03
- Context: Story context treated the protagonist as always being “the Squire,” with Squire, Archer, and Acolyte listed as separate progression paths. That left character creation ambiguous and blocked defining appearance, class kits, and tutorial framing.
- Decision: Add character customization at new-game start. The player creates a protagonist by choosing a starting archetype (base class) and customizing their character. **Squire** is one starting archetype among others—not the fixed player identity. The initial archetype set is **Squire**, **Archer**, and **Acolyte**; later morality and allegiance milestones unlock advanced archetypes as already proposed in story context.
- Rationale: A named starting class preserves the drafted-war premise for every path while giving players distinct combat roles and progression from the first session. Separating the job title “squire” from “the protagonist” removes a long-standing story ambiguity.
- Consequences: Character creation must support archetype selection and player customization (appearance details remain to be defined). Narrative, companions, and tutorial copy should address “the protagonist” rather than assuming a Squire player. Engine and RPG vertical-slice work need a character-creation flow, per-archetype starter kits, and tests for invalid or incomplete creation input. Exact customization fields, pronoun options, and noble-house/tribe obligation at creation remain open.
- Supersedes: none

### DEC-0010: Live editor MCP bridge

- Status: accepted
- Date: 2026-07-03
- Context: AI agents and automation need to edit scenes while the editor is running without corrupting undo history, collision sync, or in-memory scene authority.
- Decision: Expose a native MCP stdio server in the `engine` executable and a project-scoped Windows named-pipe bridge inside the running editor. Route live scene, prefab, and Lua mutations through existing command/validation paths; reject silent direct world JSON writes while the editor is open.
- Rationale: DEC-0003 requires command-backed, deterministic automation. The editor already owns authoritative scene state through `CommandHistory`.
- Consequences: MCP tools must detect editor availability, classify engine-vs-content work, and return structured diagnostics. Offline Lua writes may validate without a running editor but live hot reload requires the bridge.
- Supersedes: none

### DEC-0011: Engine vs content workflow routing

- Status: accepted
- Date: 2026-07-03
- Context: Agents and contributors need a consistent rule for when to change C++ engine code versus using MCP tools to edit Lua scripts, prefabs, scenes, and other project data.
- Decision: Treat C++ as the home for new runtime capabilities (movement, physics, rendering, input, asset schemas, editor behavior, and future Lua bindings). Prefer MCP scene, prefab, asset, and Lua apply tools for project content that existing engine commands and handlers already support. Call `engine_scene_plan` before ambiguous edits; never bypass live editor scene authority with direct world JSON writes.
- Rationale: DEC-0002 assigns Lua to content-heavy gameplay logic while keeping performance-critical simulation in C++. DEC-0010 already provides command-backed automation; routing prevents duplicating engine features in scripts or editing scenes outside undo/validation paths.
- Consequences: Document the decision tree in `context/architecture/content-vs-engine-workflows.md`. Movement mechanics such as jump remain engine implementations until Lua exposes movement APIs. Agents update context when introducing new content tool surfaces or engine APIs.
- Supersedes: none

### DEC-0012: Ground-cover-first foliage authoring

- Status: accepted
- Date: 2026-07-03
- Context: Artists need an optimized way to place grass and flowers on terrain without thousands of per-object draw calls. Discrete bush, stone, and tree brushes require different placement, collision, and LOD models.
- Decision: Ship foliage v1 as **density-painted ground cover only**: grass and flower layers stored in versioned JSON, scattered deterministically into GPU-instanced draws that stream with terrain. Defer discrete placement brushes, wind, impostors, and MCP automation until the editor path is stable.
- Rationale: Terrain paint and sculpt already use 33×33 per 40 m cells with undo and streaming hooks; extending that grid for density masks reuses proven authoring and reload patterns while instancing solves the draw-call budget.
- Consequences: Layer palette at `assets/foliage/ground-cover.layers.json`, density at `assets/terrain/foliage-density.json`, Sculpt-tab **Foliage** tool, `StreamedFoliageField`, and `foliage` test suite. Stylized clump primitives (`grass_clump`, `flower_clump`) stand in until a dedicated foliage shader slice lands.
- Supersedes: none

### DEC-0013: Hybrid foliage interaction (instancing + WorldInfluence)

- Status: accepted
- Date: 2026-07-03
- Context: Foliage v1 shipped density-painted instanced ground cover with clump meshes. Artists need faceted single-blade grass, easier density authoring, and walk-through bend without rebuilding scatter on every player move. A full particle/VFX system is planned but not built.
- Decision: Extend ground-cover foliage v2 as **GPU-instanced blades** with a shared **`WorldInfluenceBus`** (position, velocity, radius, strength) consumed by a dedicated foliage vertex shader for height-weighted bend. Keep painted density and streaming unchanged. Add optional per-layer `disturbVfxId` as a forward hook for future footstep/dust effects; do not render grass as particles in v2.
- Rationale: Instancing preserves deterministic authoring, streaming, and draw-call budget. Shader-only bend avoids Jolt collision and scatter rebuilds. `WorldInfluenceBus` gives the future particle system a shared input without blocking foliage on VFX MVP.
- Consequences: `grass_blade` primitive, layer bend fields, foliage-only PS/root constants, character feeds influence during play test, `world_influence` test suite, updated format docs. Bend is visual-only; fly camera has no influence. `disturbVfxId` is inert until particle milestone.
- Supersedes: none

### DEC-0014: Discrete foliage layers for bushes

- Status: accepted
- Date: 2026-07-03
- Context: Ground-cover foliage v2 handles dense grass and flowers well, but bushes need sparse placement with different density semantics. Scene prefabs already define bush variants manually at high entity count.
- Decision: Extend the foliage layer palette with `scatterMode: discrete` and `discreteMinDensity`. Discrete layers spawn at most one GPU-instanced bush per density sample when painted strength meets the threshold. Add built-in `bush`, `bush_wide`, and `bush_tall` meshes aligned with existing prefab silhouettes. Bushes reuse `WorldInfluenceBus` bend with low strength on upper foliage.
- Rationale: Keeps one Sculpt **Foliage** tool and density mask format while avoiding grass-style multiplication that would carpet bushes unrealistically. Instancing preserves streaming and draw-call budget versus per-bush scene entities.
- Consequences: Updated palette sample, scatter rules in `foliage_scatter`, bush primitives in `mesh_asset`, editor toolbar hint for discrete layers. Discrete bushes remain visual-only (no collision) in this slice; scene prefab bushes may coexist until a migration pass.
- Supersedes: none

### DEC-0015: Hybrid project tracking

- Status: accepted
- Date: 2026-07-10
- Context: The owner wants Notion or Jira-style epics and tickets for World Forge, narrative planning, shaders, quests, dialogue, open-world design, and UI/accessibility, while the repo already uses `context/` as durable agent memory under DEC-0003.
- Decision: Use **hybrid tracking**. Authoritative epic and ticket definitions live in `context/planning/epics.md` with stable `EPIC-` / `TICKET-` IDs. The external human board is the Notion project **Wrathful Conquest** (`https://app.notion.com/p/Wrathful-Conquest-30bba218df874253b6493ddfca75cffa`), which mirrors those IDs for assignment and discussion. Board-only cards are not accepted scope until recorded in context. Story canon remains in `context/story/`; engine milestones remain in `context/roadmap.md`.
- Rationale: Keeps planning diffable and automation-friendly while giving humans a familiar Notion board for prioritization and narrative planning.
- Consequences: Sync is manual until a Notion API integration exists; agents update context first. No runtime dependency on Notion. Agents cannot create or edit Notion pages without owner-provided API access.
- Supersedes: none

### DEC-0016: Entity-attached components and dual MCP apply paths

- Status: accepted
- Date: 2026-07-13
- Context: TICKET-0147 — owners want Unity-like add-component (colliders, scripts) on game objects, including via MCP. Today collision volumes live on prefab JSON and scene entities are placements without an Inspector add-component path.
- Decision: **Scene entities own components** after placement (Unity-like). Prefabs may still **seed** default components when an object is placed; afterward the entity’s components are authoritative for edit, undo, save, and MCP. MCP exposes **both** dedicated entity component/script apply tools **and** equivalent actions on `engine_scene_apply` (same command/undo path). Minimum first slice: collider volumes and script/handler binding on a target entity.
- Rationale: Matches the requested Unity authoring model and agent workflows. Dual MCP surfaces keep a discoverable dedicated API for agents while preserving batch/scene apply and existing live-bridge routing (DEC-0010/0011). Prefab seeding preserves reusable defaults without making prefab JSON the only place to add colliders.
- Consequences: Scene/world formats must persist entity components (not only placement + prefab path). Prefab `collision[]` remains a seed/template with documented compatibility or migration rules. Inspector gains Add/Remove Component. Implement shared command ops used by GUI, `engine_scene_apply`, and dedicated MCP tools. `engine_scene_plan` and content-vs-engine docs must list the new tools/actions. Rejected for this decision: prefab-only authoring and hybrid per-placement overrides as the primary model.
- Supersedes: none

### DEC-0017: Prefab and scene component authoring with Unity-like inheritance

- Status: accepted
- Date: 2026-07-13
- Context: TICKET-0147 follow-up — owners want Add Component on **custom prefabs** as well as scene entities, with Unity-like prefab→instance linkage rather than copy-on-place-only independence from DEC-0016.
- Decision: Components are first-class on **prefab assets** and **scene entities**. Placing a prefab links/copies component definitions onto the entity. Later prefab component edits **propagate** to instances that have not overridden that component; overridden instance components stay local. MCP exposes add/remove/edit via **dedicated tools** and via `engine_prefab_apply` / `engine_scene_apply` (same command/undo paths as DEC-0016). Minimum first slice remains collider volumes and script/handler binding.
- Rationale: Prefab authoring keeps reusable templates for custom assets; scene authoring matches Unity game-object workflows; inherit/override avoids silently forking every instance while still allowing per-placement customization.
- Consequences: Scene/world format needs override metadata per linked component. Prefab editor and scene Inspector both get Add/Remove Component. Prefab save triggers propagation to non-overridden instance components. Tests must cover inherit vs override. DEC-0016 dual MCP and entity-owned runtime components remain; this decision replaces DEC-0016’s copy-on-place-only / fully independent-after-place consequence.
- Supersedes: DEC-0016 (partial — instance sync and prefab-as-first-class authoring surface only; dual MCP and entity component ownership retained)

### DEC-0018: MCP terrain sculpt and paint apply

- Status: accepted
- Date: 2026-07-15
- Context: Agents could sample terrain height via `engine_scene_apply`, but height sculpt and material paint lived only on the Sculpt tab, blocking automated flatten/paint workflows.
- Decision: Expose a dedicated `engine_terrain_apply` MCP tool (bridge op `terrain_apply`) that mutates the same `TerrainEditStore` / `TerrainPaintStore` / `FoliageDensityStore` and histories as the Sculpt tab. Supported actions: `raise`, `lower`, `flatten`, `paint`, `paint_foliage`, `paint_foliage_mixed`, `sample`, `undo`, `redo`, `save`, and `batch` with `ops[]` (coalesced height/paint/foliage undo entries and a single reload per changed store). Flatten blends sample heights toward `targetHeight` (default: height at brush center) using strength as max meters per stroke with quadratic falloff. Foliage paint accepts `layer` as palette index or id (`grass`/`flower`/`bush`/…) plus optional `erase`. Live mutate/save require the editor MCP connection; `sample` may run offline. Add a Sculpt **Flatten** tool for GUI parity.
- Rationale: Matches DEC-0010/0011 live-bridge patterns and keeps terrain/foliage undo/save separate from scene `CommandHistory`, while giving agents a discoverable tool instead of overloading `engine_scene_apply`.
- Consequences: `EditorSessionContext` binds terrain and foliage stores plus reload callbacks; `engine_scene_plan` classifies terrain sculpt/paint/foliage as `terrain_data`; docs in MCP live editor and content-vs-engine workflows must list the tool. Offline JSON writes while the editor owns the stores remain rejected.
- Supersedes: none

### DEC-0019: World Forge editor home and story canon split

- Status: accepted
- Date: 2026-07-15
- Context: TICKET-0010 needed a durable product boundary so World Forge schemas/UI (TICKET-0011–0014) do not fork the integrated editor or replace narrative canon in `context/story/`.
- Decision: (1) World Forge ships as **mode(s) / panels inside the integrated editor**, not a separate app. (2) **`context/story/*.md` remains narrative canon**; World Forge holds **diffable structured project data keyed to story IDs** (factions/cultures/clans, regions/POIs/links, relationship graph). Dual-write of lore essays is rejected.
- Rationale: Matches DEC-0003 command-backed tools and M10 editor completion; keeps story workflow agent-friendly while giving runtime/authoring a stable ID-keyed JSON layer.
- Consequences: Scope contract in [`context/features/world-forge-scope.md`](../features/world-forge-scope.md). Terrain, prefab/scene placement, materials, and live MCP scene/terrain apply stay editor-owned. Schema tickets precede UI; UI/MCP must share command paths.
- Supersedes: none

### DEC-0020: World Forge narrative tooling umbrella

- Status: accepted
- Date: 2026-07-15
- Context: Owner clarified World Forge’s real job: encapsulate lore and story vision as engine-integrable data, with a relationship graph/editor for factions, clans, and people, and as the home for dialogue, quest, and story-event tools — not only map/faction schemas.
- Decision: World Forge is the **narrative tooling umbrella** inside the integrated editor. It owns (product intent): relationship graph + editor; faction/culture/clan data; regions/POIs/links; and the **product home** for quest authoring, dialogue authoring, and story events. Milestone delivery for quests/dialogue may remain M6/M7 (EPIC-0006), but those editors/schemas are World Forge surfaces, not a separate tooling brand. DEC-0019 canon split and Scene/Sculpt non-overlap remain in force.
- Rationale: One place for story→engine integration avoids split lore tools and keeps IDs/validation shared across map, relationships, quests, and dialogue.
- Consequences: Update [`world-forge-scope.md`](../features/world-forge-scope.md); EPIC-0006 notes point to World Forge as product home; relationship graph editor is first-class (not format-only); story-event schema is an expected follow-on. Do not start M6/M7 implementation ahead of M5 without owner override.
- Supersedes: none (extends DEC-0019 product scope; does not change editor-home or canon-split rules)

### DEC-0021: Soft gates with rare optional instances

- Status: accepted
- Date: 2026-07-15
- Context: TICKET-0020 must reconcile draft chapter-locked Twine flow with [DEC-0001](#dec-0001-product-and-platform-target) seamless 4×4 km world. Owner wants engine capability for both open-world travel and instanced spaces (e.g. dungeons), optimized to avoid frequent loading screens.
- Decision: Campaign acts are **narrative arcs on the seamless world**, advanced by soft gates, story flags, and region pressure — not separate always-loaded chapters. The engine **must support rare optional instances** (dungeons, set-piece arenas, vision/dream spaces) when isolation or density requires it. Prefer streaming and soft handoffs; minimize full load screens. Default play remains in the open world.
- Rationale: Keeps DEC-0001 authoritative while preserving Twine-style set pieces (Calrenoth siege density, Realm of Darkness) and future dungeon content without a chapter-load campaign spine.
- Consequences: Beat sheet in [`campaign-beat-sheet.md`](../story/campaign-beat-sheet.md). World Forge regions/POIs and later quest tools author soft gates and instance entry points. Instance streaming/loading tech remains a future engine ticket — not invented here.
- Supersedes: none

### DEC-0022: C++ animator backend with Lua drive API

- Status: accepted
- Date: 2026-07-15
- Context: After TICKET-0102 clip import, owner clarified animation ownership: users should not author the playback/transition backend in scripts; combat and movement scripts still need to drive character animation.
- Decision: **C++ owns** the animator backend — clip playback, animator controller assets (states, transitions, parameters), blending, and safe fallbacks when clips or transitions are missing. Entities/prefabs carry an **`animator` component** (same authored-component model as DEC-0016/0017) that references a controller asset. **Lua drives and reacts** via a small API (set parameters, request/crossfade states, listen to animation events) from movement, combat, and interaction scripts — Lua does **not** own the state-machine graph.
- Rationale: Keeps animation deterministic and testable in C++; matches existing scriptBinding patterns for gameplay hooks; avoids rewriting transition graphs inside every combat/movement script.
- Consequences: Expand TICKET-0103 around animator component + controller asset + Lua drive bindings. Animation events → Lua remain aligned with TICKET-0105. Document intent in [`animator.md`](../features/animator.md). Do not implement a Lua-authored transition graph without a new decision.
- Supersedes: none

### DEC-0023: Live Lua host API (agent iteration path)

- Status: accepted
- Date: 2026-07-15
- Context: Owner wants agents and humans to iterate gameplay systems in Lua while the editor/runtime is up, without rebuilding C++ for every gameplay tweak. Existing dispatch already hot-reloads `.lua` handlers, but scripts had no host API to read payloads or produce side effects.
- Decision: **Live Lua + MCP/file-monitor hot reload** is the primary iteration path for gameplay expressible in script. The sandboxed **`engine.*` host API** grows as a versioned surface; **v1** exposes `log`, `json_decode`, and a bool/number/string **blackboard**. Handler contract stays one JSON string argument; scripts opt into tables via `json_decode`. New runtime capabilities still land in C++ first, then gain thin Lua bindings so agents can use them without rebuilds.
- Rationale: Matches DEC-0002 (Lua for content-heavy gameplay) and DEC-0011 (C++ for capabilities / MCP for content). A small mutable blackboard unlocks testable live systems before damage/audio/VFX APIs exist.
- Consequences: Implement under TICKET-0152. Document the live agent loop in [`lua-scripting.md`](../features/lua-scripting.md). Follow-ons: hot-reload `bindings.script.json`, more event kinds (TICKET-0116), abilities/queries (0113/0115), animator drive (DEC-0022). Do not expose scene mutation, damage, audio, or particles in v1.
- Supersedes: none

### DEC-0024: MCP HUD toolkit with Lua value binds

- Status: accepted
- Date: 2026-07-15
- Context: Owner wants combat, movement tuning, and UI editable via MCP/Lua with hot reload. A one-off C++ health bar would require rebuilds for every new widget; a small toolkit matches the scene/prefab/Lua apply pattern.
- Decision: **C++ owns** widget primitives (`bar`, `text`, `panel`) and play-test overlay drawing. **Project data** owns HUD layouts as versioned `*.hud.json` assets edited through **`engine_hud_apply`**. **Lua** pushes runtime values via `engine.hud_set_number` / `hud_set_text` / `hud_set_visible` and thin sugar such as `engine.set_health`. HUD layout reloads are allowed during play test (non-scene). Gameplay rules (damage/heal) stay in hot-reloadable Lua handlers.
- Rationale: Agents can invent UI layout and wire combat/heal without rebuilding; new *widget types* remain deliberate C++ work. Aligns with DEC-0011 content-vs-engine workflows and EPIC-0007 / TICKET-0062 HUD IA foundation.
- Consequences: Implement under TICKET-0153. Document in [`hud-toolkit.md`](../features/hud-toolkit.md) and [`formats/hud-assets.md`](../formats/hud-assets.md). Follow-ons: buttons/input, richer anchors, editor preview, stamina/mana as content-only. Destination UI model superseded by DEC-0025 (canvas stack); v1 HUD assets remain valid until migrated.
- Supersedes: none

### DEC-0025: Responsive UI canvas stack (editor + MCP)

- Status: accepted
- Date: 2026-07-15
- Context: Owner wants a first-class UI canvas for menus and HUD — create/edit layouts in the editor and via MCP (add/remove/move, color/font), with a full screen stack (modals, focus), not a HUD-only overlay. Must stay AI-agent friendly.
- Decision:
  - **Destination:** full UI canvases (HUD, pause, inventory, dialogue, etc.), not HUD-only.
  - **Assets:** versioned **`*.uicanvas.json`** (schemaVersion 1+). Default **design resolution 1920×1080**. Migrate sample `player.hud.json` into a canvas. Thin `*.hud.json` load may remain only as a temporary shim if needed during migration.
  - **Responsive layout:** scale uniformly from the design resolution to the viewport with **letterbox/pillarbox** (no stretch, no crop).
  - **Runtime:** **engine-owned canvas stack** (`push` / `pop` / `show` / `hide`); **MCP and Lua** are equal clients. Top interactive canvas captures input; Esc/back pops (or explicit pop). Always-on HUD layer + modal screens.
  - **Widgets (interactive v1):** retain bar/text/panel; add at least **`button`** with mouse click + keyboard/gamepad focus navigation.
  - **Authoring (parallel epic):** structural **MCP mutate** (add/remove/move/resize/style) **and** an in-editor **Canvas** view (select, drag, inspector for color/font) ship as one destination, ticketed in dependency order.
  - **Sample proof:** HUD canvas always visible in play test + pause canvas push/pop.
- Rationale: Matches DEC-0003 automation-first tools and the owner’s agent-friendly iteration path. Reference resolution + uniform scale is the usual responsive game-UI model; a dedicated canvas format avoids overloading “hud” for menus.
- Consequences: Track under EPIC-0007 (owner P2 override) as TICKET-0155–0159. Document in `ui-canvas` feature/format pages when implementation starts. Does not complete accessibility product IA (TICKET-0060) or mini-map (0061). Extended remapping UI stays thin hooks + defaults. Supersedes DEC-0024 as the **destination** UI architecture; DEC-0024 remains the shipped HUD toolkit until migration. **Follow-on (owner 2026-07-15):** optional scale mode that fills to viewport edges (no letterbox/pillar bars) so chrome can hit screen edges — keep letterbox as default until then.
- Supersedes: DEC-0024 (destination only)

### DEC-0026: Quest-owned dialogue hooks (multi-stage)

- Status: accepted
- Date: 2026-07-15
- Context: Owner chose quest↔dialogue link model A for World Forge. Quests are the spine for objectives/rewards/flags; dialogue is the speech layer. A quest may need **different dialogue trees depending on stage** (start, mid-objective, fork, complete). Dialogue graphs may declare a parent quest for authorship and validation.
- Decision:
  1. **Quests own dialogue hooks.** A quest asset may reference zero or more dialogue tree IDs: quest-level (`dialogue.startId` / `completeId` / optional `abandonId`) plus **per-objective** and **per-fork** `dialogueId` fields so progress through the quest selects which tree plays.
  2. **Dialogue trees may declare `parentQuestId`** pointing at the quest they belong to (optional for shared/generic banter; required when the tree is authored as a child of a specific quest stage). Parent is the quest; children are dialogues — not the reverse write path for objectives/rewards.
  3. Dialogue does **not** embed the full quest graph; it may set/read story flags that quests and later beats consume.
- Rationale: Matches the side-quest catalog (ordered objectives + forks) and keeps one spine for Save/validation; multi-stage hooks avoid a single mega-tree or dialogue owning quest advancement as the primary authoring path.
- Consequences: Implement under TICKET-0050 (`quests.worldforge.json`). Dialogue schema/runtime (TICKET-0052) must accept `parentQuestId` and validate hooks against known dialogue IDs when both assets are present. Soft string refs allowed in 0050 until dialogue files exist. Creator UI is TICKET-0051. Owner override: start schema work before M5 exit.
- Supersedes: none

### DEC-0027: Shared World Forge graph camera

- Status: accepted
- Date: 2026-07-16
- Context: Relationship graph (TICKET-0017) and dialogue graph (TICKET-0053) duplicated pan/zoom/fit/hit-test code. Dialogue UX polish (TICKET-0165–0179) and future World Forge graphs need the same primitives without a third copy.
- Decision:
  1. Ship a shared **World Forge graph camera + minimap helper** used by relationship and dialogue canvases (and future WF graphs).
  2. Dialogue UX polish **extends** the existing Dialogues → Graph surface (TICKET-0053); do not invent a parallel dialogue editor. TICKET-0121 / TICKET-0136 are superseded by TICKET-0165+.
  3. Keep `dialogues.worldforge.json` at **schemaVersion 1** until TICKET-0172 (choice conditions / metadata / regions). Layout positions, pins, and bookmarks remain **ephemeral session state** unless a later decision persists them.
- Rationale: One camera util prevents drift between WF graphs; schema stability avoids blocking Phase 1 readability/navigation on a format bump.
- Consequences: Implement under TICKET-0165–0168 (Phase 1, owner override). Later phases 0169–0179 stay proposed/P3 until pulled. Player dialogue presentation remains UI canvas (TICKET-0163 / DEC-0025), separate from editor preview (TICKET-0177).
- Supersedes: none

### DEC-0028: Explicit quest progression runtime

- Status: accepted
- Date: 2026-07-16
- Context: Quest authoring (TICKET-0050/0051) exists; players cannot yet fulfill objectives. Completion may come from dialogue scripts, collect/kill handlers, or agent MCP tests — different triggers, one spine. Auto-advance from `DialogueRuntime` would invert DEC-0026 (quests own progress).
- Decision:
  1. **`QuestRuntime` owns session quest state** (Inactive / Active / Completed / Abandoned) over `WorldForgeQuestsAsset`. Ordered objectives; `complete_objective` succeeds only for the current (first incomplete) objective.
  2. **Explicit API only** — C++/Lua/`engine_quest_call` MCP all call the same start / complete_objective / abandon / status path. Dialogue finish does not auto-complete; scripts may call complete after a tree ends when appropriate.
  3. **Dialogue hooks remain lookups** (`dialogue.startId`, per-objective `dialogueId`, complete/abandon) via `dialogue_for_stage`; they do not advance quests.
  4. **Session-only for v1** — no RPG save blob (TICKET-0114). Minimal HUD bind `quest.objectiveText` for the current objective summary.
- Rationale: Flexibility for dialogue vs collect/kill vs agent testing without coupling speech to progression; agents need a live MCP path that mirrors gameplay.
- Consequences: Implement under **TICKET-0180** (owner override of M6 P3 hold → P2). Follow-ons: save (0114), journal/markers (0062), story-event triggers. Does not bump quest schemaVersion.
- Supersedes: none

### DEC-0029: Continuous faction standing with hostility transfer

- Status: accepted
- Date: 2026-07-16
- Context: Story wants player reputation with factions, cross-faction fallout when factions are hostile, lock-in to an allegiance track, and quest gates by standing. Morality thresholds and exact Cristallo/Arrotrebae numbers remain open; influence mechanics must not invent canon.
- Decision:
  1. **Continuous standing score** per faction that `tracksPlayer`, clamped to authored min/max; optional **ranks** (`minScore` + id) for gates; optional **lockIn** (`threshold` + `exclusiveFactionIds`).
  2. **Hostility fallout** from relationship edges: when both endpoints are `target=faction` and kind is `rival` or `opposes`, optional `standingTransfer` applies — primary delta `+D` applies `−D * standingTransfer` to the other faction (clamped).
  3. **Quests** may declare `standingRequirements` and `standingRewards`; rewards are applied by explicit caller (Lua/MCP), not auto from QuestRuntime in v1.
  4. **Morality is a separate track** (not shipped here). Session-only `StandingRuntime` + Lua/`engine_standing_call` until TICKET-0114.
  5. Keep World Forge `schemaVersion: 1` with **optional** fields (backward compatible). Do not invent story threshold numbers in sample seeds.
- Rationale: Matches relationship graph already in World Forge; numeric standing fits side-quest catalog forks; keeps morality/Act 4 independent.
- Consequences: Implement under **TICKET-0181** (owner override → P2). Soft-update open-questions: model resolved; numeric thresholds still story-open. Persist standing in TICKET-0114; faction HUD cues in TICKET-0062.
- Supersedes: none

### DEC-0030: Animation-driven root motion

- Status: accepted
- Date: 2026-07-16
- Context: TICKET-0104 needs a sync contract between clip root deltas and `CharacterController`. Hybrid additive risked double movement; extract-only deferred playability.
- Decision:
  1. When a controller (or instance) has **`applyRootMotion: true`**, weighted clip **root joint translation deltas** drive capsule horizontal displacement each tick. WASD / wish-velocity is **not** used for walk distance — input drives animator parameters and facing yaw.
  2. Root joint defaults to authored `rootJoint` (fallback name match `Root` then `Hip`). Clip-space +Z is forward; callers rotate deltas by character yaw before applying.
  3. **Y from root is opt-in** (`rootMotionY`); default off so gravity/jump remain controller-owned.
  4. Root motion is **not** max-speed clamped (authored clip distance wins). Missing root channels yield zero delta (fail soft) with diagnostics.
  5. Visual in-place root zeroing for GPU skinning remains follow-on; this ticket ships extraction + capsule sync.
- Rationale: Matches melee/root-locked attacks and DEC-0022 (C++ owns playback; Lua drives params/facing).
- Consequences: Implement under **TICKET-0104**. Document in [`animator-controller-assets.md`](../formats/animator-controller-assets.md) and [`character-controller.md`](../features/character-controller.md).
- Supersedes: none

### DEC-0031: Controller-authored animation timeline events

- Status: accepted
- Date: 2026-07-16
- Context: TICKET-0105 needs a place for hit-frame / footstep markers that Lua (and later collision) can react to. glTF extras and per-clip sidecars were considered.
- Decision:
  1. **Timeline events live on the animator controller** (`timelineEvents[]`: `state`, `time`, `name`, optional `layer`, optional `payload` object).
  2. C++ `AnimatorRuntime` fires an event when playback in that state crosses `time` (loop-aware; once per cycle).
  3. **Lua reacts** via `on_animation_event` (JSON payload: entityId, name, state, layer, time, payload) — aligned with DEC-0022. Engine does not auto-enable combat volumes in v1; scripts/MCP may do so.
  4. Invalid state references fail closed at controller validate time.
- Rationale: Keeps the graph + markers in one engine-owned asset; matches future animator graph UI; avoids DCC-only metadata.
- Consequences: Implement under **TICKET-0105**. Document in [`animator-controller-assets.md`](../formats/animator-controller-assets.md).
- Supersedes: none

### DEC-0032: Open-world travel, discovery map, and dual soft gates

- Status: accepted
- Date: 2026-07-16
- Context: TICKET-0030/0031 design notes left FT cost, mounts, soft-gate denial, hubs, and Act 1 wake geography open. Owner clarified product intent for Tessera’s seamless 4×4 km world.
- Decision:
  1. **Fast travel** is a first-class system, not a late unlock skill: discover **tavern / carriage-post** anchors in play, then pay **gold** at a post (or via the player map once known) to travel to other **discovered** towns/POIs. No wilderness FT without a post. Deny in combat / instances / blocked story flags.
  2. **Player map** shows fog-of-war on unseen areas and a dust/reveal effect as regions are discovered; FT destinations appear from discovered posts. Heavy discovery is intentional.
  3. **Mounts (near-term):** horses only if any; party is player + up to **three** companions (mount design must account for that later). Boats/other mounts deferred.
  4. **Soft-gate denial is dual-mode by region/link tag:**
     - **Border / checkpoint style** — polite dialogue, guards, story refusal (“not yet”).
     - **Hostile frontier style** — player may physically enter, but faces extreme enemies, disease/affliction, and/or item/key requirements; death or attrition is the gate.
     Silent invisible walls are rejected for soft gates.
  5. **Hubs:** about **one major hub per campaign act**, chosen for story fit (not a uniform grid of capitals).
  6. **Snow biome** only when climate/story justifies it — not a mandatory v1 band.
  7. **Act 1 wake:** after Act 0 / Creotar vision, wake in **O’hlundian evergreens**; player navigates on foot to the first village (no auto-drop into the hub).
  8. **Calrenoth** remains on the seamless map as a **ruined, impacted** revisit location after Act 0.
  9. **Opening spine:** Act 0 Calrenoth is authoritative; Wild God revival stays alternate/open chronology, not the default spine.
  10. ~~**World-map name** stays TBD; **Tessera** remains the kingdom/setting name~~ — **superseded** by [DEC-0034](#dec-0034-tessera-is-the-worlds-primary-land).
- Rationale: Matches dark-fantasy discovery pacing, gold-as-immersion carriage travel, and DEC-0021 soft gates without forcing one denial flavor for every frontier.
- Consequences: Update [`open-world-navigation.md`](../features/open-world-navigation.md), [`map-design-language.md`](../features/map-design-language.md), beat sheet Act 1 wake / Calrenoth notes. Later FT/map/soft-gate tickets and World Forge link tags must support dual denial modes + carriage-post POIs. Mini-map TICKET-0061 inherits fog/discovery UX.
- Supersedes: recommended defaults in TICKET-0030 draft that said “no FT gold cost”

### DEC-0033: Anywhere player camp as editable instance (DAO-style)

- Status: accepted
- Date: 2026-07-16
- Context: After DEC-0032 evergreen wake, owner wants a companion/camp loop: story-tied tutorial in the evergreens, then the ability to set up camp from (nearly) anywhere on the overland map. Reference feel: *Dragon Age: Origins* party camp — a dedicated space to manage party, talk to NPCs, and edit camp setup.
- Decision:
  1. **Camp is a first-class optional instance** entered from the open world ([DEC-0021](../decisions/index.md#dec-0021-soft-gates-with-rare-optional-instances)): short transition into a camp instance; exit returns to the overland pitch point.
  2. **Act 1 evergreen beat** teaches camp: setup, talk to camp NPCs/companions, and basic camp loop, with story ties (survivor retreat / Arkand path).
  3. After unlock, the player may **set up camp from the overland map** at will (not only at authored camp POIs). Camp contents (layout edits, companion staging, camp services) persist across pitches — same camp “home,” different world entry points.
  4. Camp is for party management, rest/talk, and light prep — not a substitute for hub towns or carriage-post fast travel.
  5. **Camp must not negate combat:** deny pitch while the player is in an **active combat situation** — engaged fight, active combat encounter/zone, or any state where camping would let them escape or skip combat mechanics. Also deny while already inside another instance.
  6. Quiet overland (including dangerous regions when **not** in an active fight) may still allow camp unless a later tag blocks it; the hard rule is “no combat escape hatch.”
- Rationale: Gives a persistent social/management space without chapter loads; matches companion-heavy party (player + up to three) and discovery-driven overland travel. Camp is prep/rest, not a panic button.
- Consequences: Document in [`open-world-navigation.md`](../features/open-world-navigation.md) and beat sheet A1-01. Future tickets: camp instance asset, enter/exit commands, persistence, evergreen tutorial beat, combat-state / combat-zone checks before pitch. Do not invent full RPG inventory/crafting scope here.
- Supersedes: none

### DEC-0034: Tessera is the world’s primary land

- Status: accepted
- Date: 2026-07-16
- Context: Story docs left the world-map / continent title TBD and blurred “Tessera” (setting) with “Kingdom of Tessera” (polity). Owner clarified: Tessera is the Middle-earth of the world.
- Decision:
  1. **Tessera** is the named primary land of the setting — the Middle-earth-scale geography where the campaign takes place. It is the world-map name; do not invent a separate continent title above it.
  2. **Kingdom of Tessera** is a political power *within* Tessera (dominant human occupying power), not a synonym for the whole land. Other factions and regions (Imperium, Cristallo, Arrotrebae, orc warbands, wilds, etc.) also inhabit Tessera.
  3. Lands or seas beyond Tessera remain unspecified; v1’s seamless 4×4 km slice is authored inside Tessera and does not require mapped outer continents.
- Rationale: Resolves the kingdom-vs-setting naming clash with a Tolkien-shaped split (land vs polities) without inventing extra geography.
- Consequences: Update [`story-vision.md`](../story/story-vision.md), [`factions.md`](../story/factions.md), and open-questions that treated the world-map name as TBD.
- Supersedes: DEC-0032 item 10 (world-map name TBD)

### DEC-0035: World Forge Hierarchy authorship

- Status: accepted
- Date: 2026-07-16
- Context: Owner wants pantheon, factions, and persons organized as first-class Hierarchy authorship pages (not only the relationship graph).
- Decision:
  1. World Forge gains a top-level **Hierarchy** tab with nested **Religion** / **Factions** / **Persons** authorship sub-pages (tree + detail + quick-create).
  2. **Religion** uses a new `pantheon.worldforge.json` asset (`parentId` tree; kinds deity/aspect/force). Seed only known draft/established deities (`frangitur`, `creotar`); do not invent Creo/Wild God.
  3. **Factions** tree authorship uses existing `factions.worldforge.json` `parentId`. The flat top-level Factions tab is removed; standing/detail live under Hierarchy → Factions.
  4. **Persons** uses relationship nodes (`person`/`organization`) with optional node `parentId`; faction membership stays as `member_of`/`leads` edges with Hierarchy helpers to upsert those edges. **Companions** are not a separate Hierarchy page — they are a Persons filter over person nodes tagged `companion`.
  5. **Relationships** tab remains for the non-hierarchical graph (edges + Graph canvas). Pantheon is source of truth for religion; relationship deity nodes keep aligned ids for edge endpoints until a later migration.
- Rationale: Separates org-chart authoring from freeform relationship graphs; pantheon needs its own registry for faith hierarchy without inventing theology.
- Consequences: TICKET-0183/0184/0185; MCP `kind=pantheon`; update editor-mvp and world-forge-scope.
- Supersedes: none

### DEC-0036: World Forge Act lens

- Status: accepted
- Date: 2026-07-16
- Context: Owner wants World Forge content organized by campaign acts without hard file splits (option 1: Act lens).
- Decision:
  1. Keep shared worldforge JSON files; do not split assets per act.
  2. Optional `acts: ["act0"…"act4"]` on quests, dialogue trees, map regions/POIs, and relationship nodes. Empty = campaign-wide.
  3. World Forge toolbar exposes a global Act filter (All / Act 0–4) that hides non-matching Map/Quests/Dialogues/Persons/Relationships content. Hierarchy Religion/Factions and Archetypes stay campaign-wide.
  4. Legacy `actN` tags remain readable for filter membership; prefer `acts` for new authoring.
- Rationale: Matches DEC-0021 seamless-world acts while making authoring lists readable as content grows.
- Consequences: TICKET-0189; [`../formats/world-forge-acts.md`](../formats/world-forge-acts.md).
- Supersedes: none

### DEC-0037: Git-backed authoring sync (in-editor)

- Status: accepted
- Date: 2026-07-17
- Context: Multiple authors need to share World Forge / project content (e.g. one person on engine, another on World Forge) without a custom cloud save backend. Owner asked to polish the workflow and sync git from inside the engine.
- Decision:
  1. **Git is the universal authoring sync layer.** Diffable project data (World Forge JSON, scenes, prefabs, Lua, context docs) is shared by commit / push / pull against the project remote. Do not build a separate hosted “cloud save” service for authoring.
  2. **In-editor Project Sync** wraps the system `git` CLI for the opened project root: at least **status**, **fetch**, **pull**, **commit** (explicit message; stage only project content paths), and **push**. Prefer OS/git credential helpers and SSH agent — never store remotes secrets inside the engine.
  3. Operations are **command-backed** ([DEC-0003](../decisions/index.md#dec-0003-automation-first-tools)): GUI and headless/MCP share the same automation path with structured JSON errors.
  4. After a successful **pull**, the editor offers a **safe reload** of World Forge (and documents when a dirty Scene/Sculpt session must be saved or discarded first). Merge-conflict resolution stays with git; the editor surfaces conflicted paths and fails closed rather than inventing a custom merge UI in v1.
  5. This is **authoring/project sync**, not player save-game cloud sync and not live multi-user co-editing of one open session.
- Rationale: Project assets are already text-friendly and versioned; git already provides remotes, auth, history, and conflict tools. In-editor actions remove the “leave the engine to sync” friction without reinventing hosting.
- Consequences: EPIC-0014 (TICKET-0192–0195); feature doc [`../features/authoring-git-sync.md`](../features/authoring-git-sync.md). Requires `git` on PATH and a project that is a git working tree. Real-time collab and custom cloud backends remain out of scope.
- Supersedes: none

### DEC-0038: Water, swim, and hydrology authoring

- Status: accepted
- Date: 2026-07-18
- Context: Open-world design references docks, ferries, and deep-water barriers, but the engine had no water surfaces, swim mode, or hydrology authoring. Owner clarified product intent for gameplay water (swim, scripted vessels) and how Sculpt vs World Forge split responsibility.
- Decision:
  1. **Gameplay scope:** Water is gameplay, not decorative-only. Ship a **swim mode** on the character controller. **Ships and ferries** may use scripted motion (Lua/handlers) but must **float on water surfaces** and feel believable in presentation.
  2. **Sea level:** One **world-wide sea level** constant for v1. Land/ocean relative height is adjusted with existing terrain sculpt tools (raise/lower/flatten). **Dry basins** remain dry unless terrain and authored water placement make fill sensible — no automatic flood-fill of every depression.
  3. **Authoring split:** **Sculpt** owns water **placement and sculpting** (surfaces, fill levels, shore carving) with undo/save/MCP like terrain edits. **World Forge Map** owns **hydrology layout** at planning scale (rivers, lakes, seas, coastlines) and **ferry route** metadata linked to POIs — not mesh placement ([DEC-0019](../decisions/index.md#dec-0019-world-forge-editor-home-and-story-canon-split)).
  4. **Procedural generation:** **Fully authored** for v1 — no Perlin/noise-driven auto lakes or river networks. Optional procedural helpers may come later; they are not the primary path.
  5. **Water motion:** Water surfaces use a **scripted wave-motion simulation** (deterministic, tunable) so placed water reads natural in motion. Exact technique (e.g. summed sines, Gerstner) is an engine implementation choice.
  6. **Deep vs shallow:** **Deep water** is a hard barrier implemented through **swim fatigue drain** and **damage over time** when the player must sustain swimming (deep lakes, ocean). Shallow wading may remain walkable or low-cost swim — exact depth bands are implementation tuning.
  7. **Rendering:** Water uses **reflection and refraction** while matching **smooth low-poly** art direction ([DEC-0006](../decisions/index.md#dec-0006-smooth-low-poly-art-direction)). Requires a blended water material/render pass (prerequisite to opaque-only terrain today).
  8. **Shores:** Where terrain meets water, transition to **mud or sand** shore materials when sensible; add **shore wave/foam** treatment when feasible.
  9. **Open sea:** **Bounded sea regions** authored on the map — not an infinite ocean mesh for the whole 4×4 km slice. Map **edge fog-of-war** covers beyond authored bounds for now.
  10. **Foliage:** **Suppress** ground-cover foliage underwater.
  11. **Future liquids:** Lava and magic pools are **out of v1 scope** (same systems may extend later via materials + `physics.surface`).
- Rationale: Matches seamless-world navigation (deep water as real danger), SQ-10 ferry/dive beats, DEC-0006 stylized look, and existing Sculpt/MCP + World Forge map split without duplicating Scene placement.
- Consequences: EPIC-0015; feature doc [`../features/water-hydrology.md`](../features/water-hydrology.md). Update [`open-world-navigation.md`](../features/open-world-navigation.md), [`terrain-authoring.md`](../features/terrain-authoring.md), [`character-controller.md`](../features/character-controller.md), [`world-forge-scope.md`](../features/world-forge-scope.md). Blockers: blended material/water render pass, `WaterStore` (or equivalent) persistence, swim mode, deep-water stamina/damage rules, World Forge ferry-route schema. Boats remain script-driven rather than full physics sim in v1.
- Supersedes: none

