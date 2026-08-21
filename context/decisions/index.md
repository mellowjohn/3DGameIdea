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

- Status: accepted (world extent superseded by [DEC-0054](#dec-0054-continent-scale-seamless-world--stream-budget))
- Date: 2026-07-02
- Context: The engine needs a concrete product and release target.
- Decision: Build a Windows 10/11, single-player, offline, third-person action RPG engine for a seamless open world (original v1 footprint was 4×4 km; see DEC-0054).
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
- Decision: Add character customization at new-game start. The player creates a protagonist by choosing a starting archetype (base class) and customizing their character. The melee starter is one archetype among others—not the fixed player identity. The initial **three-lane** set (melee / ranged / magic) remains; **display names and home orgs** are defined by [DEC-0044](#dec-0044-starting-archetype-lane-orgs-and-rename) (**Ashfell Blade**, **Outrider**, **Runecaster**). Later morality and allegiance milestones unlock advanced archetypes as already proposed in story context.
- Rationale: A named starting class preserves the drafted-war premise for every path while giving players distinct combat roles and progression from the first session. Separating a job title from “the protagonist” removes a long-standing story ambiguity.
- Consequences: Character creation must support archetype selection and player customization (appearance details remain to be defined). Narrative, companions, and tutorial copy should address “the protagonist” rather than assuming one melee fantasy. Engine and RPG vertical-slice work need a character-creation flow, per-archetype starter kits, and tests for invalid or incomplete creation input. Exact customization fields, pronoun options, and creation-time obligations remain open (lane orgs: DEC-0044).
- Supersedes: none
- Amended by: [DEC-0044](#dec-0044-starting-archetype-lane-orgs-and-rename) (display names, ids, lane home orgs)

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
- Evolution (2026-07-27): `grass_blade` is a 4-segment crossed tapered strip (~0.7 height). Foliage VS bend is tip-squared lean + trample (not flat XZ push); cheap tip wind flutter uses existing `time_seconds`. No skeletal bones; Tsushima-style Bézier compute blades remain deferred.
- Evolution (2026-07-27, TICKET-0230): Shared `WindFieldParams` drives traveling gust tip lean on foliage layers, height-weighted canopy sway on `tree` / `dead-tree` prop meshes, and a camera-local ambient wind particle emitter (`wind_trail.particle.json` + `wind_streak.png`). Particle `texture` path is wired for that streak; campfire keeps the procedural soft disc.
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
- Related reopen (2026-08-03): Dom + owner draft lean toward **Palworld-style placeable open-world camp/base** (companions at camp; optional craft later). **Not yet an amend** — track as Dom **D-P1-23** + **TICKET-0254**; provenance [`../design/recording_ld_character_concepts_2026-08-03.md`](../design/recording_ld_character_concepts_2026-08-03.md). Keep this DEC until a superseding decision lands.
- Supersedes: none

### DEC-0034: Tessera is the world’s primary land

- Status: accepted
- Date: 2026-07-16
- Context: Story docs left the world-map / continent title TBD and blurred “Tessera” (setting) with “Kingdom of Tessera” (polity). Owner clarified: Tessera is the Middle-earth of the world.
- Decision:
  1. **Tessera** is the named primary land of the setting — the Middle-earth-scale geography where the campaign takes place. It is the world-map name; do not invent a separate continent title above it.
  2. **Kingdom of Tessera** is a political power *within* Tessera (dominant human occupying power), not a synonym for the whole land. Other factions and regions (Imperium, Cristallo, Arrotrebae, orc warbands, wilds, etc.) also inhabit Tessera.
  3. Lands or seas beyond Tessera remain unspecified. The seamless playable world **is** the official Tessera continent map window ([DEC-0054](#dec-0054-continent-scale-seamless-world--stream-budget)) — not a tiny inset slice that leaves most of the illustrated land off-map.
- Rationale: Resolves the kingdom-vs-setting naming clash with a Tolkien-shaped split (land vs polities) without inventing extra geography.
- Consequences: Update [`story-vision.md`](../story/story-vision.md), [`factions.md`](../story/factions.md), and open-questions that treated the world-map name as TBD. Item 3 world-extent wording updated 2026-08-06 for DEC-0054.
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

### DEC-0038: Authored Rigidbody — dynamic bodies for player and entities

- Status: accepted
- Date: 2026-07-17
- Context: Player locomotion today is a standalone `CharacterVirtual` (`CharacterController`) while props use `CollisionWorld` dynamic flags with no Unity-like Rigidbody authored component. Owner wants one physics component for the player and other entities that need physics, and chose **true dynamic rigidbodies** (not CharacterVirtual-under-a-mode).
- Decision:
  1. Introduce an authored **`rigidbody`** component (Add Component on prefabs/entities; DEC-0016/0017 inherit/override) that owns motion: mass, drag/friction (material or fields), constraints (e.g. freeze rotation), gravity toggle, and kinematic vs **dynamic** mode.
  2. The component is **universal**: the same Add Component path applies to the **player prefab and any other prefab** that needs physics (NPCs, props, pushables, etc.). No player-only physics API once migration completes.
  3. Entities that need physics use this component with **dynamic** rigidbody locomotion — input/scripts apply forces / target velocities on the Jolt dynamic body; friction and collisions come from the physics material / body, not a separate CharacterVirtual velocity integrator.
  4. Authored **`collider`** volumes remain the shape source; Rigidbody is the motion body. Runtime body handles stay non-serialized (same rule as today’s transient physics ids).
  5. Today’s `CharacterController` / `CharacterVirtual` path is **transitional** until the Rigidbody component ships and player spawn migrates. Root-motion sync ([DEC-0030](../decisions/index.md#dec-0030-animation-driven-root-motion)) must be retargeted from `CharacterController` to the Rigidbody-backed entity when that lands.
  6. Pure static world colliders (terrain, buildings) stay static bodies — they do not require a Rigidbody component.
- Rationale: Matches owner intent for Unity-like Rigidbody friction/forces and **prefab reuse** (drop Rigidbody + Collider on any prefab); avoids a permanent CharacterVirtual vs Rigidbody split.
- Consequences: **EPIC-0015** (TICKET-0196 schema → 0197 spawn → 0198 player migrate → 0199 root-motion/samples). Update [`../architecture/components.md`](../architecture/components.md) and [`../features/character-controller.md`](../features/character-controller.md) as tickets land.
- Supersedes: none (does not remove CharacterVirtual until migration tickets complete)

### DEC-0039: Water, swim, and hydrology authoring

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
- Consequences: EPIC-0016; feature doc [`../features/water-hydrology.md`](../features/water-hydrology.md). Update [`open-world-navigation.md`](../features/open-world-navigation.md), [`terrain-authoring.md`](../features/terrain-authoring.md), [`character-controller.md`](../features/character-controller.md), [`world-forge-scope.md`](../features/world-forge-scope.md). Blockers: blended material/water render pass, `WaterStore` (or equivalent) persistence, swim mode, deep-water stamina/damage rules, World Forge ferry-route schema. Boats remain script-driven rather than full physics sim in v1.
- Supersedes: none

### DEC-0040: Discrete Cartography zoom layers with fog and frame

- Status: accepted
- Date: 2026-07-20
- Context: Continuous multi-LOD tiling of the official Tessera map fought AI quadrant seams and did not match TES/Witcher/HOI-style strategic map UX. Owner chose discrete zoom layers shared by editor Cartography and the future player discovery map, with fog transitions and an ornate parchment frame.
- Decision:
  1. **Discrete plates, not continuous tile pyramids**, are the primary Cartography backdrop. Plates are crops of one continuous master (`tools/build_world_map_layers.py`); AI quadrant inject stays off by default.
  2. **Layer swap** selects the highest-priority plate whose UV contains the view center and whose `minZoom` is met. Continent remains the base; theater/local plates overlay their UV rects.
  3. **Fog transition** (~0.35s parchment mist) covers plate swaps; fog draws under the ornate frame.
  4. **Ornate frame** is screen-space chrome (toggleable), not baked into geography art. Shared assets feed TICKET-0061.
  5. **Top-down** mode stays the geo-aligned terrain underlay path; Cartography remains campaign parchment reference (not 1:1 heightmap).
  6. Legacy `world-map-tiles` LOD pyramid remains a fallback only when the layers manifest is missing.
- Rationale: Avoids Frankenstein geography from mismatched detail plates; matches discrete strategic-map UX; reuses one plate/frame/fog kit for editor and player map.
- Consequences: Manifest at `world-map-layers/manifest.json`; frame/fog under `assets/ui/cartography/{frame,fog}/`. Update [`../story/official-world-map.md`](../story/official-world-map.md), [`../art/cartography-design.md`](../art/cartography-design.md). TICKET-0061 should consume the same layer model.
- Supersedes: continuous multi-LOD Cartography backdrop as the preferred path (tiles remain fallback)

### DEC-0041: Rig metadata before IK solver

- Status: accepted
- Date: 2026-07-21
- Context: TICKET-0106 (IK hooks + retargeting metadata) was a stub. Owner chose schema-first authoring now, with a full IK solver later; also requested a Diagnostics-adjacent Animation manage/preview panel (tracked as TICKET-0135, not this ticket).
- Decision:
  1. Ship authorable `*.rig.json` (`RigAsset`) with `ikHooks[]` and `boneRoles[]`, optional `character.rig` path.
  2. Validate schema + optional joint-name checks against `ImportedSkin::joint_names`.
  3. Do **not** implement runtime IK solve or GPU skinning in this ticket.
  4. Full IK solve remains a follow-on after skinning/playback foundations; Animation tools UI is TICKET-0135.
- Rationale: Unblocks retarget/IK authoring and MCP-friendly data without committing to a solver API; matches the literal ticket title and mesh-assets pending note.
- Consequences: Format [`../formats/rig-assets.md`](../formats/rig-assets.md); sample `player.rig.json`. Solver + Animation panel are separate tickets.
- Supersedes: none

### DEC-0042: Online co-op session model (mode-locked saves)

- Status: accepted
- Date: 2026-07-22
- Context: Story draft lock (2026-07-22) calls for single-player primary plus optional up-to-2-player online co-op with shared campaign progression, shared faction standing, and party size up to four (two humans + up to two companions). D-P2-11 left networking, host authority, companion slots, and session rules open. Owner clarified engine/product rules in design sessions the same day.
- Decision:
  1. **Two campaign modes, chosen at new game or load — never mixed on one save.** `sessionMode` is **`solo`** or **`coop`**. Solo saves run with one human and never accept a guest. Co-op saves require **two connected human players** for the full session; do **not** fall back to solo play when the guest disconnects or leaves.
  2. **Online drop-in only** (no couch split-screen v1 path). **Host-authoritative** simulation: host owns world sim, quests, NPCs, companions, physics for AI, and shared campaign state; guest sends input and receives replicated state/effects.
  3. **Shared campaign tracks on both modes (where applicable):** quest progression, faction standing ([DEC-0029](../decisions/index.md#dec-0029-continuous-faction-standing-with-hostility-transfer)), **morality**, act/world flags, discovery/fog, camp persistence. Per-player: archetype, appearance, inventory, gear, HUD, input device.
  4. **Party caps:** solo — **1 human + up to 3 companions** (≤4 total); co-op — **2 humans + up to 2 companions** (≤4 total). Companion slots remain active in co-op; recruitment blocked when at mode cap.
  5. **Lobby flow:** host creates a co-op lobby (new co-op game or load co-op save); guest joins; **both players ready** before the session starts. Character creation for co-op happens in lobby (host and guest each configure their protagonist before ready/start).
  6. **Major story forks** (allegiance lock-in, act gates, and similar high-impact choices): **both players must agree** (unanimous confirm) before the choice applies.
  7. **Guest disconnect:** enter **`paused_waiting_guest`** — **untimed** reconnect pause (world frozen). Host may **end session** if the other player cannot rejoin. Do not downgrade `sessionMode` to solo or continue co-op save alone. On end, return to menu with co-op save intact for a future paired session.
  8. **Save blob** (TICKET-0114): root **`sessionMode`**, **`hostProfile`**, **`guestProfile`** (absent in solo), and **`sharedCampaign`** (quests, standing, morality, flags, discovery, camp, …). Refuse load/start when mode requirements are unmet (co-op without guest connected).
  9. **Replication scope (implementation):** replicate player entity state and reliable session deltas; host runs Lua/handlers and companion AI — guests receive effects/UI, not a second sim. No wholesale blackboard replication.
- Rationale: Matches Fable-like shared-campaign co-op while honoring owner intent that co-op is a committed paired experience, not optional drop-out solo. Host authority fits quest/standing/morality spine already centralized in C++ runtimes. Lobby + dual ready avoids starting co-op saves in a broken one-player state. Unanimous major forks preserve shared morality/allegiance as one campaign voice.
- Consequences: Partially supersedes DEC-0001 multiplayer-out-of-v1 for **product intent** — shipping may still SP-first, but session/save/party layers must be co-op-shaped before netcode. Resolve D-P2-11. Implement in dependency order: `GameSession` + mode fork → multi-profile save → `PartyRuntime` companion caps → online lobby/reconnect → co-op UI gates (ready, unanimous forks). Feature doc [`../features/co-op-sessions.md`](../features/co-op-sessions.md); save sketch [`../formats/rpg-save.md`](../formats/rpg-save.md). Networking library, NAT, and invite UX remain implementation tickets — not chosen here.
- Supersedes: DEC-0001 (partial — co-op is in-scope product/engine design; offline SP ship path retained)

### DEC-0043: NVIDIA reference GPU, multi-vendor D3D12 support

- Status: accepted
- Date: 2026-07-23
- Context: Owner asked whether the product should have NVIDIA support for graphics cards. DEC-0001/0002 already lock Windows + Direct3D 12; DEC-0004 already names an RTX 4070–class PC as the 1440p/60 performance contract machine.
- Decision:
  1. **NVIDIA mid/high desktop (RTX 4070–class)** remains the **reference / acceptance GPU** for ship budgets, benchmarks, and visual QA ([DEC-0004](#dec-0004-diagnostics-and-performance-contract)).
  2. The runtime **must not require NVIDIA**. Ship for any GPU that meets the documented Direct3D 12 feature level and minimum VRAM/driver bar (NVIDIA, AMD, Intel).
  3. Store/marketing copy may list NVIDIA as **recommended**, not required.
  4. Vendor-exclusive features (DLSS, Reflex, etc.) stay **optional follow-ons** — not v1 blockers and not gatekeepers for playability on other vendors.
- Rationale: Keeps a measurable quality bar on the machine class already used for TICKET-0139 without cutting Windows AMD/Intel players or forcing a partnership for v1.
- Consequences: Continue capturing benchmarks and visual regressions primarily on NVIDIA reference hardware; add multi-vendor smoke when machines are available. Do not add NVIDIA-only `#ifdef` paths for core rendering. Update ship-requirements language when a public min-spec page lands.
- Supersedes: none

### DEC-0044: Starting archetype lane orgs and rename

- Status: accepted
- Date: 2026-07-24
- Context: Starting archetypes were labeled **Squire / Archer / Acolyte** — generic class tags with thin play fantasy. Owner locked a rename plus per-lane **home orgs** that carry their own mid-game story, then bind into major faction politics. Magic must stay boon/relic/item-native (runes/sigils), not a wizard seminary, and must not be conflated with the Cristallo crystal-guardian order (White Lotus draft label).
- Decision:
  1. Keep **three starting lanes** (melee / ranged / magic) under [DEC-0009](#dec-0009-starting-archetype-character-creation).
  2. **Display names + stable ids:** **Ashfell Blade** (`ashfell_blade`), **Outrider** (`outrider`), **Runecaster** (`runecaster`). Former Squire / Archer / Acolyte labels are legacy only.
  3. **Hybrid org model (C):** each starter begins in a **lane home org** with its own quests/ranks/NPCs; later that org **binds** into major faction politics (Cristallo / Arrotrebae / Kingdom). Not three fully separate campaigns; not org-less kits.
  4. **Home orgs + sub-themes:**
     - Melee — **House Ashfell** (draft name; Dom may confirm/rename) → Fighter / Brawler
     - Ranged — **Outrider Lodge** → Ranger / Forager / Nomad
     - Magic — **Runecaster Guild** → Rune / Sigil (create runes / draft sigils; caster fantasy, item/rune-sourced)
  5. Crystal-guardian order (White Lotus draft) stays a **separate, mostly non-magic** custodian path — not the Runecaster starter.
  6. Advanced specialization lists and full combat **systems/comps** remain open; this decision locks naming + org spine only.
- Rationale: Tessera-specific titles and org homes make starters readable and authorable without exploding Act 0 into three unrelated openings. Rune/sigil casting respects the low-fantasy magic boundary.
- Consequences: Update story canon, World Forge `archetypes.worldforge.json` ids, Act 0 archetype/art readiness labels, save/profile examples, and tests. Seed House Ashfell / Lodge / Guild faces as Dom+owner authoring. Do not invent Dom-owned person names here.
- Supersedes: DEC-0009 starting archetype **display names** and implied generic class tags only (creation flow retained)

### DEC-0045: JSON event timelines with C++ sequencer (World Forge home)

- Status: accepted
- Date: 2026-07-24
- Context: Act Zero Landfall MVP readiness (`coding_event_timeline` / `cine_event_timeline_ready`) needs theatrical beats (prologue, siege backdrop, Luceran, Creotar, camp wake) driven by camera, wait, dialogue, VFX hooks, and control lock. Animation controller `timelineEvents` (DEC-0031) are clip hit-frames, not campaign cinematics. Owner chose data-driven JSON + C++ sequencer, then World Forge authoring integration — not Lua-only sequence scripts.
- Decision:
  1. **Authored timelines** are versioned JSON under World Forge product home (`events.worldforge.json` or equivalent; format ticket owns path). Sequences are ordered typed **steps**, not free-form Lua graphs.
  2. **C++ `EventTimelineRuntime`** (name may match ticket) owns load/bind/start/tick/cancel, step advancement, and fail-closed validation. Lua / quest / interaction scripts may **start** or **cancel** a sequence by id and react to step hooks — they do not author the step list.
  3. **MVP step kinds** (Landfall): `wait`, `lock_control` / `unlock_control`, `start_dialogue` (tree id → existing `DialogueRuntime`), `emit` (named signal / Lua hook for VFX or content), and camera steps once camera helpers land (`look_at` / path blend — TICKET-0222). Unknown step kinds fail closed at validate/load.
  4. **World Forge integration** (TICKET-0223): Events list/detail (and MCP `kind=events`) under the World Forge umbrella ([DEC-0020](../decisions/index.md#dec-0020-world-forge-narrative-tooling-umbrella)); runtime schema ships first (TICKET-0221) so content is not blocked on editor chrome.
  5. Distinct from **animator** `timelineEvents` ([DEC-0031](../decisions/index.md#dec-0031-controller-authored-animation-timeline-events)).
- Rationale: Matches quest/dialogue/animator patterns (diffable data + C++ backend + thin Lua drive), unlocks Act 0 cinematic content with headless suites, and keeps story-event product home on World Forge without requiring the Events pane on day one.
- Consequences: Implement TICKET-0221 (asset + runtime + sample), TICKET-0222 (camera path / play input lock wiring), TICKET-0223 (WF Events pane + MCP). Particle draw remains on `coding_particle_system_mvp`; timeline only emits hooks until that lands. Do not invent a parallel Lua DSL for the same beats.
- Supersedes: none

### DEC-0046: Session story flag runtime

- Status: accepted
- Date: 2026-07-24
- Context: Act 0 MVP readiness `coding_quest_runtime_flags` needs session flags to gate corridors and record forks. `QuestRuntime` (TICKET-0180 / DEC-0028) already advances objectives; dialogue can author `setFlags` and quests author fork `outcomeFlags`, but nothing persisted them in a queryable session store. Soft-gates and journal UI are separate readiness rows.
- Decision:
  1. **`FlagRuntime` owns session story/outcome flags** as a set of freeform string ids (`act0.helped_larrell`, …). API: set / clear / has / list; empty ids fail closed (`FLAG-RUNTIME-*`).
  2. **Dialogue choice `setFlags` apply into `FlagRuntime`** on choose (same explicit path as standing adjust) — does not auto-advance quests.
  3. **Quest fork outcomes** are applied by explicit `QuestRuntime::resolve_fork(questId, forkId, outcomeFlag, FlagRuntime&)`: validates the flag is authored on that fork, clears sibling `outcomeFlags`, then sets the chosen flag.
  4. **Lua + MCP** mirror the API (`flag_*`, `quest_resolve_fork`, `engine_flag_call`, `quest_call` kind `resolve_fork`).
  5. **Save**: `sharedCampaign.outcomeFlags` capture/hydrate through `FlagRuntime` (TICKET-0114 shape already reserved the field).
  6. **Out of this decision:** soft-gate region pressure, quest journal UI, co-op flag replication (later tickets).
- Rationale: Completes the missing half of Act 0 quest/stage runtime without expanding into presentation or corridor systems; keeps fork resolution explicit like quest objective completion.
- Consequences: Implement under **TICKET-0225** (owner Act 0 P0). Soft-gate and journal remain separate checklist rows / tickets.
- Supersedes: none

### DEC-0047: Frame upload ring and GPU LBS skinning

- Status: accepted
- Date: 2026-07-24
- Context: Debug play-tests were CPU-bound (~20–30 ms prep) with ~1–3 ms GPU. Shared permanently mapped upload CBs forced a full fence drain after Present. Player deformation used CPU LBS + `patch_mesh_vertices` each pose change. Owner asked to offload visual work toward the GPU (skinning + multi-buffered uploads); GPU-driven culling/LOD deferred.
- Decision:
  1. **2-slot UPLOAD CB ring** keyed to swapchain `frame_index_` for frame, water-frame, shadow, SSAO, composite, and bone palette CBs (`frame_count = 2`). Steady-state render must not drain the fence after successful Present; allocator reuse still waits `frame_fence_values_[frame_index_]` at frame start.
  2. **GPU linear-blend skinning** for the play-test player: bind-pose vertex buffer stores JOINTS (`R8G8B8A8_UINT`) + WEIGHTS (`R8G8B8A8_UNORM`); CPU keeps `sample_skinned_local_poses` + `build_skin_matrices`; VS skins position/normal when weight sum > 0 (lit + shadow). **`MAX_BONES = 64`**.
  3. Do **not** reintroduce per-pose CPU vertex patch for the player path; skins above the bone cap leave bind pose / fail closed.
  4. Culling and mesh distance LOD remain CPU-side this pass.
- Rationale: Removes the Present serialize and the expensive mesh Map rewrite while keeping pose composition on CPU (cheap vs per-vertex LBS). Matches existing 2-frame swapchain/allocator ring without triple-buffer complexity.
- Consequences: Implement under **TICKET-0226** (upload ring) and **TICKET-0227** (GPU skinning). Catalog-wide NPC skinning and GPU-driven culling remain follow-ons. Update mesh/debug-world/character-controller docs when shipping.
- Supersedes: none

### DEC-0048: Terraria-shaped gearing with soft archetype affinity

- Status: accepted
- Date: 2026-07-27
- Context: Design session (John + Dom, 2026-07-27) locked a Terraria-inspired item/trinket fantasy for Wrathful Conquest: depth from unique item effects, not complex combat animation trees. Follow-up chat clarified cross-archetype use, rare chase loot, and Thrator mount reward. Existing lanes are Ashfell Blade / Outrider / Runecaster ([DEC-0009](#dec-0009-starting-archetype-character-creation), [DEC-0044](#dec-0044-starting-archetype-lane-orgs-and-rename)). Inventory foundation remains TICKET-0111.
- Decision:
  1. **Combat feel:** simple **action combat** (Souls-lite), not tab-target WoW and not Black Desert–complexity animation. Baseline = **three weapon chains** (melee / ranged / magic) plus item-driven procs/on-use/passives. **Ability caveats** allowed (resource costs, cooldowns, situational gates) without abandoning the simple-chain core.
  2. **No hard gear locks:** any archetype may equip and use any weapon/item and its weapon ability. **Soft affinity:** matching lane benefits via **stat allocation / efficiency multipliers** (off-lane stays usable). **Amended by [DEC-0050](#dec-0050-inventory-ux-item-kinds-and-positive-soft-affinity):** affinity is a **positive bonus** on matching gear — do not nerf off-lane below a 1× baseline. Co-op item gifting between players is expected.
  3. **Act-tier scaling:** item power bands track campaign acts (Act 0 weakest commons → later acts stronger). **Obscure rare chase items** may punch above their act band (including early-act rares that stay relevant later); first-playthrough discovery should be hard without external knowledge / deep exploration — not required for story completion.
  4. **Acquisition loops:** world finds, vendors (incl. Ledgeport undermarket), mining ores/crystals → craft materials, boss common + rare tables with optional farm replay. Full craft loop after inventory basics. **Act 0 Landfall ships a small playable loot slice** (starter kit + a handful of finds/rewards along the siege → camp path)—not a full Terraria catalog and not Thrator/Ledgeport vendor depth.
  5. **Thrator:** draft easter-egg **orc warlord** champion (Act **1 or 2**, not Act 0). Side quest with a warband, Orgrimmar-flavored set piece; kill reward includes a **glad mount**. Carriage-post **fast travel remains** ([DEC-0032](#dec-0032-open-world-travel-discovery-map-and-dual-soft-gates)); ground mounts are traversal toys/rewards. Exotic mounts (glad mount) **soft-extend** DEC-0032’s near-term “horses only” when that content ships.
  6. **Animation budget:** prefer unique **item effects** over unique attack animations per weapon.
- Rationale: Matches Dom/John intent (item-driven depth, soft lanes, replayable rares) without exploding animation scope or contradicting FT policy.
- Consequences: Feature note [`../features/gearing-system.md`](../features/gearing-system.md); epic **EPIC-0018**; extend TICKET-0111 / combat slice tickets; seed **SQ-13 Thrator** in side-quest catalog. Do not invent full exotic mount roster in Act 0.
- Supersedes: none (soft-extends DEC-0032 mount near-term when glad mount lands)

### DEC-0049: Agent-writable material shader profiles

- Status: accepted
- Date: 2026-07-28
- Context: Owner wants stronger artistic capability for MCP and AI agents. Research compared Unity/Unreal-style shader graphs to code-first master shaders with JSON parameters. Agents already author well via diffable JSON (materials, particles); node graphs are poor for MCP validation, diff review, and agent reasoning. Current materials are scalar PBR only; shaders are embedded HLSL in `render_app.cpp`.
- Decision:
  1. **No Unity-style shader graph** for v1 (or near-term EPIC-0005). Do not build node-editor codegen as the agent authoring path.
  2. **Code-first master shaders** owned in C++/HLSL; materials select a `shader` profile enum and fill documented parameters in `.material.json`.
  3. **MCP/agents** create and tweak looks through `engine_asset_apply` (and future particle apply), validate, and screenshot-iterate.
  4. **Expand vocabulary in slices:** TICKET-0238 (profiles + emissive pulse), TICKET-0239 (masked cutout), TICKET-0240 (material map slots), TICKET-0241 (particle MCP + recipes).
  5. A human-facing surface graph remains a possible later option; if added, it must still compile to the same master+params (or HLSL files) so agents keep a text path.
- Rationale: Matches content-vs-engine workflows, text-first assets, and agent tooling; delivers most stylized-art flexibility without multi-month graph infrastructure.
- Consequences: TICKET-0041 → needs-approval; implement TICKET-0238–0241 under EPIC-0005; update materials format + features index (“shader profiles” not “shader graphs” as the primary path).
- Supersedes: none (resolves TICKET-0041 interview)

### DEC-0050: Inventory UX, item kinds, and positive soft affinity

- Status: accepted
- Date: 2026-07-29
- Context: John + Dom design recording (2026-07-29) answered remaining inventory/item open questions after [DEC-0048](#dec-0048-terraria-shaped-gearing-with-soft-archetype-affinity). Provenance: [`../design/recording_item_system_2026-07-29.md`](../design/recording_item_system_2026-07-29.md).
- Decision:
  1. **Use model (Terraria-shaped hotbar + light armor doll):** weapons and utility tools live on an **8-slot hotbar**; select a slot to use it (including situational utility weapons). Equipped gear uses named slots: **`head`**, **`chest`**, **`legs`**, plus **four accessory/trinket slots** (`trinket0`…`trinket3`; experiment; may tune later). Stats/effects can come from armor pieces, trinkets, and weapons. Shields may occupy a trinket slot (Terraria-style), not a deep MMO offhand tree for v1. Class/lane **abilities are a separate UI** (draft: right-side, ~3–4 active slots)—not hotbar slots.
  2. **Bag:** base capacity **20 slots**, not weight. **One item entry per bag slot** (no multi-slot tetris footprints). Capacity can **increase** by crafting or looting **bag upgrade items** (exact upgrade steps / soft max are implementation-tunable; must remain slot-based). **Stacking:** gather resources and similar stack up to **99** per bag stack; **dedicated ammo slots** may stack higher (target **~1000**).
  3. **Camp storage:** **per-player** chests (co-op partners open their own stash in a shared camp). Persist for the life of the save. Players may **trade/gift**. **Quest items** use a **separate quest inventory**, not bag slots.
  4. **Soft affinity (positive):** any lane may use any gear. Matching archetype gains **bonus** efficiency/stat benefit on matching gear. Off-lane stays at a **1× baseline** — do **not** apply a punitive underperformance tax. Reward on-lane play; do not diminish off-lane experimentation.
  5. **Effect authoring:** **both** data-driven effect ids/params **and** Lua hooks for unique item behavior.
  6. **Item classification:** **primary kind tag** (bucket) plus optional **labels** for sorting/filters. Primary buckets include at least: `weapon`, `armor`, `trinket`, `consumable`, `material` (resource). Labels e.g. `healing`, `utility`. Quest-bound items may also live in the quest inventory regardless of combat tags.
  7. **Durability:** **none for v1** — gear does not wear out / no repair gold sink. Difficulty-scaled durability remains explicitly deferred.
  8. **Save shape (inventory):** per-player profile stores **bag**, **hotbar**, **equipped** (accessories/armor strip), and references/state for **camp storage**; each entry has `itemId`, stack `count`, and resolves against authored defs (kind tag + labels + effects). **Currencies** are **non-slot counters** owned by the inventory/economy API (icons + amounts)—not bag entries. Co-op **shared gold** default in `sharedCampaign.economy` remains ([`../formats/rpg-save.md`](../formats/rpg-save.md)) unless a later decision splits purses.
  9. **Act 0 concrete defs:** ship the six concept-backed items (3 starters + Field Bandage + Soldier's Scrap Pouch + **Vein-Iron Pendant**) and expand with light commons/uncommons (potions, ammo, uncommon finds) plus optional extras—see [`../features/gearing-system.md`](../features/gearing-system.md).
- Rationale: Matches Dom/John preference for Terraria clarity (slots, hotbar use, accessory trinkets) and positive reward psychology for soft lanes, without inventing durability chores or weight simulation.
- Consequences: Update gearing feature note, open-questions, rpg-save inventory stub, TICKET-0111 / 0232 / 0237 acceptance detail; hotbar HUD sample should move toward **8** slots when inventory UI lands.
- Supersedes: none (amends DEC-0048 soft-affinity tone: bonus-not-nerf)

### DEC-0051: No-XP power progression and quest UX

- Status: accepted
- Date: 2026-07-29
- Context: Follow-on to quest UI session ([`../design/recording_quest_ui_progression_2026-07-29.md`](../design/recording_quest_ui_progression_2026-07-29.md)). Owner + Dom answered the progression / journal questionnaire in [`../design/recording_archetype_quests_power_progression_2026-07-29.md`](../design/recording_archetype_quests_power_progression_2026-07-29.md). Aligns with Terraria-shaped gearing ([DEC-0048](#dec-0048-terraria-shaped-gearing-with-soft-archetype-affinity), [DEC-0050](#dec-0050-inventory-ux-item-kinds-and-positive-soft-affinity)) and quest-owned dialogue ([DEC-0026](#dec-0026-quest-owned-dialogue-hooks-multi-stage)).
- Decision:
  1. **No traditional XP / player level.** Character power does **not** come from an XP bar or combat level. Power comes from **gear**, **act/boss loot bands**, **archetype quest unlocks**, and **story/act milestones**.
  2. **Ability unlock paths (combined):**
     - **Archetype quest lines** and **story/act milestones** unlock lane abilities / power.
     - **Gear and trinkets** may grant **archetype-affinity abilities or benefits** while equipped (positive soft affinity; off-lane still usable at 1× baseline per DEC-0050).
     - There is **no** pure XP talent tree.
  3. **Archetype quest rewards** are **per-quest authored**: signature gear, signature ability, or both. Act 1 vs Act 3 rewards scale in power; do not force one reward type for every quest.
  4. **Archetype quests are optional for main-story completion** but are the path to **full lane / archetype power**. Players who skip stay viable via gear; players who want max lane power should run the lines.
  5. **Loot-band unlocks** are gated by **known main-storyline bosses**, including **mid-act chapter bosses** (not only act finales). Completing act main-quest beats may also advance bands. Bosses should be **player-visible / well-known**. Boss tables remain **farmable** for gear ([DEC-0048](#dec-0048-terraria-shaped-gearing-with-soft-archetype-affinity)). **Act 0:** no named chapter boss (Luceran A0-07 theatrical); band advance via Landfall completion; first main-story boss = Pneumyra (A1-05) — owner lock 2026-08-05.
  6. **Journal tabs (locked):** **Main · Side · Faction · Archetype** (+ Completed). Primary filtration; more tabs later only with owner ask.
  7. **Faction tab** holds standing quests for **Cristallo**, **Arrotrebae**, Kingdom of **Tessera**, and other major polities — same tab, separate quest lists. A quest may **blend** (e.g. archetype + Cristallo) when story-fit: one primary `kind` plus optional `factionId` / lane refs or tags so it can surface under multiple filters.
  8. **World markers:** classic floating **`?`** (available) and **`!`** (active / turn-in), with a light bob animation. Generate UI icons later.
  9. **HUD tracking:** up to **3** tracked quests at once, with kind filter (prefer mixing main / side / faction / archetype). Clicking a chip should surface **minimap / map** location UX (TICKET-0062). Chip corner remains top-left draft until polish.
  10. **Abandon:** main quests **cannot** be abandoned. Side / faction / archetype **can** abandon and **re-accept** later.
  11. **Events:** quests are the **parent**; cinematic / EventTimeline steps are **children** quests may reference/trigger by id (start / objective / complete).
  12. **Complete panel:** shows reward copy, optional **lore journal** beat, **items** (hover for effects), gold/consumables, and **standing ±** for affected factions. **`standingRewards` apply on quest complete** via QuestRuntime (closes prior “scripts must call standing_adjust” gap for authored rewards).
- Rationale: Removes cheese-leveling while keeping Terraria gear fantasy; separates lane power (archetype lines) from campaign spine; keeps journal filters transparent; binds standing to completion for predictable authoring.
- Consequences: Update gearing + character-creation + quest format + `quest-ui.pen` + open-questions. Schema: widen `kind` with `archetype` when lane-org seeds land; wire `standingRewards` on complete (QuestRuntime follow-up). Journal/map markers remain TICKET-0062. ~~Act 0 boss presence~~ **locked 2026-08-05** (none; Pneumyra first).
- Supersedes: none (amends quest standing-apply gap noted in [`../formats/world-forge-quests.md`](../formats/world-forge-quests.md))

### DEC-0052: Dual-edit animation clips (engine override + sync to glTF)

- Status: accepted
- Date: 2026-08-03
- Context: Owner asked for an Animation Studio viewport (EPIC-0019) that can edit clip keyframes in-engine. Clips today import from glTF ([`../formats/animation-clip-assets.md`](../formats/animation-clip-assets.md)); Blockbench remains the art source. Owner chose a **dual** save model so the engine always knows authored changes and the glTF source stays good when syncing.
- Decision:
  1. **Engine override is the live authoring layer.** In-engine keyframe edits persist to a project-local engine-owned clip override asset (target shape: `*.anim.json` or equivalent under `assets/`; exact schema lands with TICKET-0253). Runtime sampling prefers the override when present for a `(clipSource, clipName)` pair.
  2. **Sync to source updates the glTF.** An explicit **Sync to source** (or equivalent Save-both) action writes the override channels back into the referenced `.gltf` / `.glb` `animations[]` so the art source matches what the engine plays. Fail-closed on unsupported paths (e.g. CUBICSPLINE) with stable error codes.
  3. **Import still starts from glTF.** Fresh import / bake without an override uses glTF channels. Re-import that would clobber an existing override must surface a clear conflict choice (keep override / replace from source) — silent overwrite is rejected.
  4. **Scope of dual-edit:** bone TRS keyframes for LINEAR/STEP clips. Controller `timelineEvents`, gear, and handAttach remain their existing asset homes (animator JSON / item catalog); they are not folded into `*.anim.json`.
- Rationale: Keeps Blockbench/glTF as a healthy source while giving the engine a durable, text-friendly override for studio polish and agent tooling.
- Consequences: TICKET-0253 implements format + library merge + Sync UI; update [`../formats/animation-clip-assets.md`](../formats/animation-clip-assets.md) when schema ships; Animation Studio feature note owns the UX contract ([`../features/animation-studio.md`](../features/animation-studio.md)).
- Supersedes: none (fills the deferred “compiled intermediate `.anim.json`” note in the clip format doc)

### DEC-0053: Native game module hot-reload (C ABI)

- Status: accepted
- Date: 2026-08-04
- Context: C++ changes to `engine_core` still require kill → rebuild → restart. Owner asked for faster native iteration without abandoning [DEC-0023](#dec-0023-live-lua-host-api-agent-iteration-path) Lua/content hot reload. The editor is a single static `engine_core` linked into `engine.exe` with no existing plugin seam ([`components.md`](../architecture/components.md) already excludes arbitrary C++ component plugins).
- Decision:
  1. **Live Lua + assets remain the primary iteration path** for gameplay expressible in script/project data (DEC-0023). Native hot-reload is a complementary side channel.
  2. **Host keeps platform and runtime ownership:** D3D12, ImGui, Jolt, EnTT, `LuaRuntime`, editor/MCP, `GameSession`, and first-class RPG runtimes stay in `engine_core` / the process. The game DLL must not create a second device, ImGui context, physics world, or Lua state.
  3. **C ABI only across the boundary** (`game_module_abi.h` v1): host provides log + blackboard set/get; module exports `abi_version` / `name` / `init` / `tick` / `shutdown`. No STL, EnTT, Jolt, or C++ classes in the ABI.
  4. **Windows load pattern:** copy `game_module.dll` to a generation path before `LoadLibrary` so MSBuild can overwrite the canonical DLL while a previous generation stays mapped until unload/reload.
  5. **MVP omits DLL-registered `lua_CFunction`s** (static Lua / dual-heap risk). After reload the host may call optional Lua `on_game_module_reloaded` with name/ABI/generation.
  6. **Out of scope:** marketplace/C++ component plugins, hot-reloading `engine_core` itself, moving quest/inventory/renderer into the DLL.
- Rationale: Unlocks native C++ tune/verify loops for code that must stay C++ without a full process restart, while preserving the existing content workflow and a fail-closed ABI check.
- Consequences: Implement under EPIC-0020 / TICKET-0257. Document workflow in content-vs-engine and [`../features/game-module-hot-reload.md`](../features/game-module-hot-reload.md). `engine_core` changes still require the kill → rebuild → restart loop.
- Supersedes: none (complements DEC-0023; does not replace it)

### DEC-0054: Continent-scale seamless world + stream budget

- Status: accepted
- Date: 2026-08-06
- Context: Owner wants the playable seamless world to **encapsulate the official Tessera continent map** (not a 4×4 km inset), feel like a real open-world RPG (Skyrim-class Act 0 density), and raise streaming/view distance with a proper stress test while staying amortize-bounded.
- Measured map: master `4096×2730` (aspect **~1.500**); Cartography `local_calrenoth` plate is **~37%×37%** of that frame. Scaling so that opening theater ≈ **6 km** across (Skyrim-hold / Act 0 corridor) implies a full-continent plate of **~16.2×10.8 km**.
- Decision:
  1. **Playable world extent:** seamless square **16×16 km** (`worldSizeMeters: [16000,16000]`, partition half-extent **8000 m**, 128 m cells). Contains the cartography plate with north/south ocean padding.
  2. **Cartography plate:** **16000×10667 m** (map aspect), centered on the playable window — the official continent PNG is the full overland, not a backdrop for a smaller slice.
  3. **Play stream budget:** terrain/water neighborhood radius **4** / support **2** (~160–200 m resident ring) with existing view-bias + amortize + look-gate. Scene/Sculpt + main-menu preview may use a wider full-disc radius (editor view radius ≥5) for establishing shots.
  4. **View distance LOD:** raise placed-mesh far cull and foliage scatter falloff to match the wider stream (near ~280 m / far cull ~360 m; foliage falloff ~200–340 m). Keep hitch amortization; do not load the whole continent.
  5. **Stress:** suite walks the **256 km²** extent at coarse step and asserts resident cell count stays within the designed neighborhood bound for the new radius.
- Rationale: Compresses Middle-earth-shaped Tessera into a shippable seamless playable map while keeping Act 0 Calrenoth corridor Skyrim-scale; streaming stays a ring around the camera, not whole-map residency.
- Consequences: Update partition defaults, sample worlds, `map.worldforge.json` plate, streaming/LOD docs, architecture overview, and terrain stress expectations. Density/landmark guidance in map-design-language scales with area. Follow-on: relocate Act 0 content across the plate as LD locks coords (D-P2-08).
- Supersedes: DEC-0001 world-extent wording (4×4 km / 16 km²); DEC-0034 item 3 “4×4 km slice inside Tessera”

### DEC-0055: Reloadable-native gameplay is the default C++ iteration path

- Status: accepted
- Date: 2026-08-10
- Context: The initial game-module boundary (DEC-0053) proves that a native C++ DLL can reload without restarting the editor, but its v1 log/blackboard API is too narrow to prevent many ordinary gameplay edits from landing in `engine_core` and triggering full rebuilds.
- Decision:
  1. For new native **gameplay-facing** work, first evaluate whether it can be authored in Lua/project data; if it must be C++, it should default to the reloadable `game_module` boundary.
  2. Grow the host API deliberately through versioned, POD-only C ABI tables: gameplay commands, read-only queries, events, timers, opaque stable handles, and reload-state serialization are allowed directions. Each addition must define ownership, lifetime, invalid-handle behavior, threading, and reload semantics.
  3. `engine_core` remains the owner of D3D12, ImGui, Jolt, EnTT, Lua, editor/MCP, save I/O, and low-level runtime lifecycle. Do not expose their objects, STL types, C++ classes, raw engine pointers, or allocator ownership across the DLL ABI.
  4. Engine-core changes remain appropriate when a feature changes core capability, schema, engine/editor behavior, rendering, physics, or a cross-system runtime contract. Arbitrary in-place hot swapping of `engine_core` is not a goal.
- Rationale: Makes fast native gameplay iteration the normal path while retaining an ABI that can fail closed instead of corrupting a running editor after a DLL reload.
- Consequences: EPIC-0020 should prioritize safe gameplay API slices over a wider but unsafe plugin surface. New gameplay tickets must state why Lua is insufficient, which ABI capability they use or add, and their reload-state verification. This amends DEC-0053's MVP-only scope; existing first-class core runtimes are not migrated merely for hot reload.
- Supersedes: none (amends DEC-0053 iteration routing; its ownership and C-ABI restrictions remain in force)

### DEC-0056: Per-cinematic-instance terrain data

- Status: accepted
- Date: 2026-08-10
- Context: Appearance Courtyard is a self-contained cinematic instance. Its MCP-authored terrain was sharing the open-world sculpt, paint, and foliage stores, which let one presentation's visual work affect another world.
- Decision: Each `cinematic_instance` world owns a separate terrain data set. When MCP terrain sculpt, paint, or foliage operations run while such a world is active, they load and save only that instance's terrain files. Open-world and menu worlds retain the shared terrain stores.
- Rationale: A cinematic instance is a self-contained presentation; its authored exterior needs independently repeatable terrain without contaminating the overland or other instances.
- Consequences: The editor, terrain MCP context, renderer streaming, validation, and save routing must resolve terrain storage from the active world presentation. Opening another cinematic instance must swap its terrain stores and refresh streamed cells; missing instance terrain files begin empty. Water remains out of this scope until an instance needs it.
- Supersedes: none.

### DEC-0057: Marble-bag RNG for gameplay rolls

- Status: accepted
- Date: 2026-08-19
- Context: Owner wants a single, predictable fairness model for every **gameplay** random system (loot, crits, procs, drop counts, encounter/table rolls, and similar). Independent Bernoulli rolls (`math.random()`, per-hit 20% crit, weighted loot with replacement) produce long droughts and lucky streaks that fight the Terraria-shaped loot fantasy ([DEC-0048](#dec-0048-terraria-shaped-gearing-with-soft-archetype-affinity)) and Souls-lite combat feel. Act 0 pouch/chest Lua currently uses independent weighted picks (`loot_container_interaction.lua`) — that path is **non-canon** until it draws from bags.
- Decision:
  1. **Gameplay RNG is marble-bag only.** Authored outcomes are **integer marble counts** in a named bag. A roll **draws without replacement**. When the bag is empty (or cannot satisfy the requested draw), it **refills** to the authored composition and continues. Multiple grants from one interaction (e.g. two loot items) are **sequential draws from the same bag**, not independent re-rolls of the full table.
  2. **Named bags, not one universe bag.** Each table/stat owns a bag id (loot table, crit bag, proc bag, …). Authors may **share** a bag id across surfaces when they want one pity/cycle to cover both; they must not silently merge unrelated systems.
  3. **C++ owns the draw.** A single engine runtime (commands + tests) shuffles/draws, persists remaining marbles, and exposes Lua/MCP APIs. Content authors composition and bag ids; scripts **must not** call `math.random` / `rand` for gameplay outcomes. Fail closed if a gameplay path would roll outside a bag.
  4. **Save the remainder.** Remaining marble state is part of the RPG save (per player / per bag id) so reload and co-op profiles keep pity/cycle progress. Tests use a seeded shuffle so a given seed + composition is deterministic.
  5. **Player UI is hidden in v1.** Feel comes from the cycle itself; no required on-HUD marble count. Diagnostics / MCP may dump remaining counts. A later visible pity UI needs a separate lock.
  6. **Out of scope (not marble bags):** visual/simulation noise — particle spawn, foliage scatter, flipbook start, camera shake, editor yaw randomize. Those stay independent (seeded where the feature already requires determinism).
- Rationale: Marble bags bound droughts and jackpot streaks while staying authorable as integer weights. One engine API keeps loot, combat, and future tables honest instead of each Lua handler inventing odds.
- Consequences: Feature note [`../features/marble-bag-rng.md`](../features/marble-bag-rng.md). Migrate Act 0 loot Lua off independent picks when the runtime ships. Combat crit/proc work must draw bags, not `if random < p`. Schema + save stub land with the implementing ticket (EPIC-0018 / combat slice). Do not treat current `math.random` loot as the shipping contract.
- Supersedes: none (overrides the Act 0 loot script’s independent-weight behavior as product intent; does not change DEC-0048 acquisition loops)

### DEC-0058: Shared UI theme tokens

- Status: accepted
- Date: 2026-08-19
- Context: Inventory chrome buttons authored `color` as the plate fill, and the HUD draw path reused that same RGBA for the label — gold on gold. Authors and MCP also had no single object to restyle iron/gold chrome across canvases.
- Decision:
  1. Project UI chrome lives in `assets/ui/ui-theme.json`: named **tokens** (RGBA) plus **roles** (`primaryButton`, `secondaryButton`, `title`, …) that point at tokens.
  2. Widgets may set `themeRole` / `colorToken` / `textColorToken` / `textColor`. Literal `color` still wins for fill when present.
  3. Button labels never copy fill. Unspecified label color uses the role’s text token, else luminance contrast (ink on gold, chrome on iron).
  4. MCP writes the theme via `engine_asset_apply` `kind: ui_theme`; mutate `style` can assign roles; Lua `ui_theme_set_token` tweaks live values. The editor UI tab exposes token color pickers and role dropdowns.
- Rationale: Matches existing JSON asset + MCP apply patterns without inventing a second chrome language. One file restyles inventory (and later pause/HUD) without rewriting every widget.
- Consequences: Format [`../formats/ui-theme-assets.md`](../formats/ui-theme-assets.md). Sample inventory chrome buttons use `themeRole` instead of gold-on-gold plates. Draw defaults in `hud_runtime.cpp` remain fallbacks when a canvas has no theme loaded.
- Supersedes: none

### DEC-0060: Status effects runtime + typed combat text

- Status: accepted
- Date: 2026-08-20
- Context: Owner wants melee combo damage (hit 1 min → hit 3 max), test weapons with poison/bleed, DoT ticks as colored floating `-N` text, and status modifiers on the player health bar.
- Decision:
  1. **C++ `StatusEffectRuntime`** owns DoT timers (poison/bleed v1). Catalog weapons may list `stats.onHit[]` (`status`, `damagePerTick`, `duration`, `tickInterval`). Lua applies via `engine.status_apply` / clears via `status_clear`.
  2. **Melee combo damage:** `CombatContactEvent.attacker_combo_step` 1/2/3 maps to attribute-scaled weapon min / mid / max. Step 0 (ranged / unknown) keeps the damage marble bag ([DEC-0057](../decisions/index.md#dec-0057-marble-bag-rng-for-gameplay-rolls)).
  3. **`CombatTextRuntime` kinds:** hit / crit / bleed / poison / heal with distinct colors; DoT ticks use a minus prefix. Spawn above the target name plate ([DEC-0059](../decisions/index.md#dec-0059-world-anchored-combat-text-floaters)).
  4. **Stacking + target HUD:** each apply of the **same** kind adds a stack (soft cap 10), refreshes duration, and scales tick damage by stacks. **Bleed and poison run together.** Afflicted **targets** show status droplet icons above their world HP chip (remaining-seconds ticker + duration bar; stack badge when `stacks > 1`) — not on the player vitals bar. Player DoT damage applies in C++ with resist curves; world targets react via optional Lua `on_status_tick`.
- Rationale: Matches DEC-0011 (C++ capability / Lua content) and keeps Act 0 sandbox testable with cloned bleed sword / poison bow items.
- Consequences: Feature [`../features/status-effects.md`](../features/status-effects.md). Sample `assets/items/status_test_weapons.json` plus school foci burn/slow. Lightning chain hops are a MagicCast combat proc (`chainHop` on `CombatContactEvent`), not a lasting status. HUD art: `assets/ui/hud/hud-status-bleed.png` / `hud-status-poison.png`. Extend kinds later (blight) without changing the apply/tick contract.
- Supersedes: none

### DEC-0059: World-anchored combat text floaters

- Status: accepted
- Date: 2026-08-20
- Context: Hits need readable damage feedback with juice (impact scale, rise, fade). Crits need distinct color and a bigger punch. Persistent world billboards already cover NPC name/HP chips; they are not ephemeral damage floaters.
- Decision:
  1. **C++ owns** a `CombatTextRuntime` pool of short-lived world-projected numbers (impact → rise → fade). Not a UI canvas widget and not the HP billboard chip.
  2. **Lua spawns** via `engine.combat_text({x,y,z,amount|text,crit})` from combat hurt handlers.
  3. **Anchor above the name:** floaters spawn just above the enemy/NPC name (and HP chip label stack), not at the contact point and not on top of the name glyphs.
  4. **Crit styling:** gold accent text, larger base size, stronger impact peak / rest scale than normal hits (chrome text).
- Rationale: Matches DEC-0011 (C++ capability / Lua content), reuses world projection like billboards, and keeps name plates readable while damage pops above them.
- Consequences: Feature [`../features/combat-text.md`](../features/combat-text.md). Wire Act 0 dummy sandbox first; other hurt scripts call the same API. Heal/miss/block glyphs may reuse `text=` later.
- Supersedes: none

### DEC-0061: Masked override animator layers (upper-body overlay)

- Status: accepted
- Date: 2026-08-20
- Context: Player Block was a full-body base-layer state, so holding guard replaced Walk/Run and froze the legs. Owner chose an upper-body overlay over extra walk-while-blocking clips.
- Decision:
  1. Animator layers remain **override** (not additive). A layer may author `mask.joints[]` plus `includeChildren` (default true). Skinning applies later layers only on masked joints; the first layer is the full-body base.
  2. `motion.type = "none"` is a passthrough state (no clips). Overlay default is `empty`.
  3. `animator` `defaultState` override applies only to the **first** layer so overlay defaults stay `empty`.
  4. Root motion ignores masked overlay layers. Additive blending stays deferred.
- Rationale: One Block hold clip can sit on spine/arms/head while the existing locomotion blend tree keeps the legs. Matches the requested walk-and-block read without a second locomotion set.
- Consequences: Format [`../formats/animator-controller-assets.md`](../formats/animator-controller-assets.md); sample `player.animator.json` `upperBody` layer (Block + Attack/Attack2/Attack3 + BowDraw/BowAim/BowRelease + MagicCast). TICKET-0283. Additive / per-joint weights remain follow-on.
- Supersedes: none (extends DEC-0022 layer contract; TICKET-0103 v1 was override-only with no mask)
