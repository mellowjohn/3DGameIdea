# Project Epics and Tickets

Authoritative planning backlog for humans and agents. Status and **Priority** here are source of truth for scope, acceptance, and work order. The Notion project [Wrathful Conquest](https://app.notion.com/p/Wrathful-Conquest-30bba218df874253b6493ddfca75cffa) mirrors these IDs for assignment and discussion under [DEC-0015](../decisions/index.md#dec-0015-hybrid-project-tracking). Sync steps: [`notion-sync.md`](notion-sync.md). Population rule: [`.cursor/rules/epic-ticket-population.mdc`](../../.cursor/rules/epic-ticket-population.mdc). Templates: [`epic-template.md`](epic-template.md) · [`ticket-template.md`](ticket-template.md). Assignment skill: [`skills/engine-ticket-workflow/SKILL.md`](../../skills/engine-ticket-workflow/SKILL.md). Ticket briefs: [`tickets/`](tickets/).

Statuses: `proposed` | `ready` | `active` | `needs-approval` | `done` | `deferred`.  
- `needs-approval` = agent finished implementation + verification; waiting for human owner approval.  
- `done` = owner approved / ready to ship (or already shipped). Agents never set `done`.  
Priorities: `P0` (critical path) | `P1` (parallel now) | `P2` (ready / next, not ahead of P0) | `P3` (held / later).

Kanban (drag Status columns): **[Work Board](https://app.notion.com/p/39ad3efc569581b0a9c9cd4fa0d38868?v=39ad3efc569581bc94d5000c60541f4a)** — details in [`notion-sync.md`](notion-sync.md#kanban-board). Only put work that can start now in `ready`. Owner approves by dragging `needs-approval` → `done`.

## Sync rules

- Create or change epic/ticket text in this file (and linked `context/` docs) first.
- Mirror the same `EPIC-` / `TICKET-` IDs, Status, and Priority in Notion.
- Do not treat Notion-only cards as accepted scope until they appear here.
- Story canon stays in `context/story/`; engine milestones stay in `context/roadmap.md`.
- When assigning agents: set Notion **Agent** + **Status**, keep **Priority** aligned with this file (see [`notion-sync.md`](notion-sync.md#assigning-work-to-agents)).

## Priority ladder

| Priority | Meaning | Typical work |
| --- | --- | --- |
| P0 | Engineering critical path — do first when Ready | M5 animation clips (TICKET-0102) after 0101 done |
| P1 | Parallel now — story, nav/map design, World Forge scope | EPIC-0003, EPIC-0004, TICKET-0010 |
| P2 | Ready or next, but not ahead of P0 | PBR (0040/0143), M5 follow-ons, World Forge schemas after 0021 |
| P3 | Held / later — do not start without owner override | M6/M7/UI, M8–M11, deferred items |

Agents without an explicit ticket ID: prefer **Agent = cursor-agent**, then lowest Priority number among `ready` (then `active`), skipping blocked rows.

## EPIC-0001: Project tracking integration

- Status: done
- Goal: Hybrid tracking — in-repo backlog plus Notion Wrathful Conquest board.
- Roadmap home: Planning / DEC-0015 (not a runtime milestone).
- Priority guidance: Complete; keep sync rules current when board properties change.
- External board: [Engine Planning Board](https://app.notion.com/p/39ad3efc569581309306e0d8e84cb026)

| ID | Title | Status | Priority | Notes |
| --- | --- | --- | --- | --- |
| TICKET-0001 | Choose external board (Notion or Jira) | done | P3 | Notion Wrathful Conquest |
| TICKET-0002 | Create board structure matching EPIC/TICKET IDs | done | P3 | Epics + Tickets DBs seeded under Engine Planning Board |
| TICKET-0003 | Document sync checklist for agents and humans | done | P3 | [`notion-sync.md`](notion-sync.md) |

## EPIC-0002: World Forge

- Status: ready
- Goal: Narrative tooling umbrella inside the editor — lore/story vision as engine-integrable data; relationship graph/editor; regions/POIs; faction/culture/clan authoring; product home for quests, dialogue, and story events.
- Roadmap home: extends M10 Integrated Editor; hosts M6/M7 authoring surfaces (delivery still gated); consumes `context/story/` and open-world partition data.
- Priority guidance: P1 for TICKET-0010; P2 schemas (0011–0014) then UI shell/inspectors (0015–0016); graph canvas 0017; quest schema (0050) owner-overridden P2 before M5 exit; remaining quest/dialogue tools P3 until M5 unless overridden further.
- Scope boundary: [`context/features/world-forge-scope.md`](../features/world-forge-scope.md) ([DEC-0019](../decisions/index.md#dec-0019-world-forge-editor-home-and-story-canon-split), [DEC-0020](../decisions/index.md#dec-0020-world-forge-narrative-tooling-umbrella)).

| ID | Title | Status | Priority | Notes |
| --- | --- | --- | --- | --- |
| TICKET-0010 | Define World Forge scope vs existing editor/MCP | done | P1 | Owner approved 2026-07-15; [`../features/world-forge-scope.md`](../features/world-forge-scope.md); [`tickets/TICKET-0010.md`](tickets/TICKET-0010.md) |
| TICKET-0011 | Faction / culture / clan data model (diffable) | needs-approval | P2 | Schema + sample + C++ asset + `world_forge` suite; [`tickets/TICKET-0011.md`](tickets/TICKET-0011.md); [`../formats/world-forge-factions.md`](../formats/world-forge-factions.md) |
| TICKET-0012 | Entity and character relationship graph format | needs-approval | P2 | Schema + sample + C++ asset + `world_forge` suite; [`tickets/TICKET-0012.md`](tickets/TICKET-0012.md); [`../formats/world-forge-relationships.md`](../formats/world-forge-relationships.md) |
| TICKET-0013 | Map-asset authoring surface (regions, POIs, links) | needs-approval | P2 | Schema + sample + C++ asset + `world_forge` suite; UI/MCP follow-on; [`tickets/TICKET-0013.md`](tickets/TICKET-0013.md); [`../formats/world-forge-map.md`](../formats/world-forge-map.md) |
| TICKET-0014 | Editor/MCP entry points for World Forge ops | needs-approval | P2 | `engine_world_forge_apply`; [`tickets/TICKET-0014.md`](tickets/TICKET-0014.md); [`../formats/world-forge-mcp.md`](../formats/world-forge-mcp.md) |
| TICKET-0015 | World Forge editor viewport tab (shell) | needs-approval | P2 | 5th Viewports tab beside UI; scaffold + asset sub-tabs; [`tickets/TICKET-0015.md`](tickets/TICKET-0015.md) |
| TICKET-0016 | World Forge list/detail inspectors (factions, graph, map) | needs-approval | P2 | Edit via shared `apply_world_forge_operation`; [`tickets/TICKET-0016.md`](tickets/TICKET-0016.md) |
| TICKET-0017 | Relationship graph visual canvas | needs-approval | P2 | Graph mode in Relationships pane; [`tickets/TICKET-0017.md`](tickets/TICKET-0017.md) |

## EPIC-0003: Narrative content (storyline, factions, side quests)

- Status: active
- Goal: Define main storyline beats, faction canon pass, and side-quest inventory as durable story context.
- Roadmap home: content for M6/M7/M9; not a runtime milestone by itself.
- Priority guidance: All child tickets P1 — parallel now with nav/map and World Forge scope.

| ID | Title | Status | Priority | Notes |
| --- | --- | --- | --- | --- |
| TICKET-0020 | Main storyline beat sheet (campaign acts) | done | P1 | Owner approved 2026-07-15; [`../story/campaign-beat-sheet.md`](../story/campaign-beat-sheet.md); DEC-0021 |
| TICKET-0021 | Faction canon review and gaps | done | P1 | Owner approved 2026-07-15; gaps remain open in [`factions.md`](../story/factions.md); [`tickets/TICKET-0021.md`](tickets/TICKET-0021.md) |
| TICKET-0022 | Side-quest catalog (regions, hooks, rewards) | done | P1 | Owner approved 2026-07-15; [`../story/side-quest-catalog.md`](../story/side-quest-catalog.md) |
| TICKET-0023 | Continuity checklist for new story docs | done | P1 | Owner approved 2026-07-15; [`continuity-checklist.md`](../story/continuity-checklist.md) |

## EPIC-0004: Open-world navigation and map design

- Status: ready
- Goal: Design conversations and acceptance criteria for traversal, landmarks, and map readability in the 4×4 km world ([DEC-0001](../decisions/index.md#dec-0001-product-and-platform-target)).
- Roadmap home: builds on M4 navigation grid; feeds World Forge and UI.
- Priority guidance: P1 for 0030/0031; P2 for 0032 after design notes.

| ID | Title | Status | Priority | Notes |
| --- | --- | --- | --- | --- |
| TICKET-0030 | Open-world navigation design notes | ready | P1 | Fast travel, roads, barriers, soft gates; [`tickets/TICKET-0030.md`](tickets/TICKET-0030.md) |
| TICKET-0031 | Map design language (biomes, landmarks, density) | ready | P1 | Align with dark-fantasy art direction; [`tickets/TICKET-0031.md`](tickets/TICKET-0031.md) |
| TICKET-0032 | Streaming/LOD/budget acceptance for authored regions | proposed | P2 | Needs design notes first |

## EPIC-0005: Materials, shaders, and post-process

- Status: ready
- Goal: Shader/material authoring path and post-process effects (including ambient occlusion).
- Roadmap home: extends active material assets; visual polish toward M8/M11.
- Priority guidance: P2 — not ahead of M5 P0 (TICKET-0102).

| ID | Title | Status | Priority | Notes |
| --- | --- | --- | --- | --- |
| TICKET-0040 | Dynamic material / PBR rendering slice | done | P2 | Opaque PBR shipped (owner approved); [`tickets/TICKET-0040.md`](tickets/TICKET-0040.md) |
| TICKET-0041 | Shader authoring strategy (graphs vs code-first) | proposed | P2 | Decision interview before large investment |
| TICKET-0042 | Post-process stack with ambient occlusion | done | P2 | SSAO v1 shipped (owner approved); [`tickets/TICKET-0042.md`](tickets/TICKET-0042.md) |

## EPIC-0006: RPG systems — quests and dialogue

- Status: proposed
- Goal: Quest creator and dialogue system for immersion and RPG vertical slice.
- Roadmap home: **M6** quests/RPG data; **M7** dialogue tools.
- Product home: **World Forge** ([DEC-0020](../decisions/index.md#dec-0020-world-forge-narrative-tooling-umbrella)) — editors/schemas live under World Forge panels; milestone hold still applies.
- Hold: do not pull ahead of M5 animation (EPIC-0008) without an explicit decision. **TICKET-0050** started under owner override 2026-07-15 ([DEC-0026](../decisions/index.md#dec-0026-quest-owned-dialogue-hooks-multi-stage)).

| ID | Title | Status | Priority | Notes |
| --- | --- | --- | --- | --- |
| TICKET-0050 | Quest data model and validation | needs-approval | P2 | Owner override; `quests.worldforge.json` + multi-stage dialogue hooks; [`tickets/TICKET-0050.md`](tickets/TICKET-0050.md); [`../formats/world-forge-quests.md`](../formats/world-forge-quests.md) |
| TICKET-0051 | Quest creator tooling (command-backed) | needs-approval | P2 | Owner override; Quests pane + MCP kind=quests; Twine Act 0 seed; [`tickets/TICKET-0051.md`](tickets/TICKET-0051.md) |
| TICKET-0052 | Branching dialogue runtime | needs-approval | P2 | Owner override; Twine import + DialogueRuntime; [`tickets/TICKET-0052.md`](tickets/TICKET-0052.md); [`../formats/world-forge-dialogues.md`](../formats/world-forge-dialogues.md) |
| TICKET-0053 | Dialogue graph editor and headless tests | needs-approval | P2 | Owner override; Dialogues→Graph canvas + headless edit helpers; [`tickets/TICKET-0053.md`](tickets/TICKET-0053.md) |
| TICKET-0054 | Twine → World Forge dialogue import | needs-approval | P2 | Owner override; `import_twee` MCP/editor + C++ parser; Act 0 lore draft sync; [`tickets/TICKET-0054.md`](tickets/TICKET-0054.md) |
| TICKET-0165 | Shared World Forge graph camera + minimap helpers | needs-approval | P2 | Owner override; extract pan/zoom/fit/minimap for rel+dlg; [`tickets/TICKET-0165.md`](tickets/TICKET-0165.md); [DEC-0027](../decisions/index.md#dec-0027-shared-world-forge-graph-camera) |
| TICKET-0166 | Dialogue node readability + display modes | needs-approval | P2 | Owner override; Compact/Standard/Expanded; speaker/lines/type/icons; [`tickets/TICKET-0166.md`](tickets/TICKET-0166.md) |
| TICKET-0167 | Dialogue graph navigation | needs-approval | P2 | Owner override; search, history, bookmarks, frame/zoom; [`tickets/TICKET-0167.md`](tickets/TICKET-0167.md) |
| TICKET-0168 | Dialogue graph toolbar + shortcuts | needs-approval | P2 | Owner override; toolbar + undo stack + shortcuts; [`tickets/TICKET-0168.md`](tickets/TICKET-0168.md) |
| TICKET-0169 | Dialogue auto-layout / align / distribute | proposed | P3 | Phase 2; LTR hierarchical; pinned nodes stay |
| TICKET-0170 | Dialogue connection styles + focus fade | proposed | P3 | Phase 2; curved/orthogonal, reroutes, hover highlight |
| TICKET-0171 | Dialogue inline editing + port drag-create | proposed | P3 | Phase 2 |
| TICKET-0172 | Dialogue schema v2 + choice editor | proposed | P3 | Phase 3; conditions/effects/reorder; schema bump |
| TICKET-0173 | Dialogue inspector collapsible + Advanced Mode | proposed | P3 | Phase 3 |
| TICKET-0174 | Dialogue narrative metadata (Advanced) | proposed | P3 | Phase 3; align TICKET-0118 |
| TICKET-0175 | Dialogue regions / comments / collapse | proposed | P3 | Phase 4 |
| TICKET-0176 | Dialogue validation lint + node badges | proposed | P3 | Phase 4 |
| TICKET-0177 | In-editor dialogue preview (DialogueRuntime) | proposed | P3 | Phase 5; not TICKET-0163 player canvas |
| TICKET-0178 | Dialogue test presets | proposed | P3 | Phase 5 |
| TICKET-0179 | Large dialogue graph performance | proposed | P3 | Phase 6; 500–2000 nodes |
| TICKET-0180 | Quest progression runtime (API + Lua + MCP) | needs-approval | P2 | Owner override of M6 hold; [DEC-0028](../decisions/index.md#dec-0028-explicit-quest-progression-runtime); [`tickets/TICKET-0180.md`](tickets/TICKET-0180.md) |
| TICKET-0181 | Faction standing / reputation (schema + runtime) | needs-approval | P2 | Owner override; [DEC-0029](../decisions/index.md#dec-0029-continuous-faction-standing-with-hostility-transfer); [`tickets/TICKET-0181.md`](tickets/TICKET-0181.md) |

## EPIC-0007: Gameplay UI and accessibility

- Status: ready
- Goal: Player-facing UI/UX including accessibility, mini-map, and a responsive UI canvas stack (editor + MCP) for HUD and menus.
- Roadmap home: supports M9 vertical slice; typography already planned in features.
- Priority guidance: Owner P2 override for canvas stack ([DEC-0025](../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp)); mini-map remains P3 after World Forge region/POI IDs; accessibility baselines stay P3 (0060) with thin focus hooks in canvas epic.

| ID | Title | Status | Priority | Notes |
| --- | --- | --- | --- | --- |
| TICKET-0060 | UI/UX accessibility requirements | proposed | P3 | Scale, contrast, input remapping baselines |
| TICKET-0061 | Mini-map (data, rendering, fog-of-war policy) | proposed | P3 | Depends on map/region IDs from World Forge |
| TICKET-0062 | HUD information architecture for open-world RPG | proposed | P3 | Quest markers, faction cues; builds on canvas/HUD runtime |
| TICKET-0153 | MCP HUD toolkit v1 + player health bar | needs-approval | P2 | Stepping stone; migrate under DEC-0025; [`tickets/TICKET-0153.md`](tickets/TICKET-0153.md) |
| TICKET-0154 | MCP engine_lua_call script event dispatch | done | P2 | Agent-friendly fire interaction/combatHurt/handler without overlap |
| TICKET-0155 | UI canvas format + responsive draw | done | P2 | `*.uicanvas.json`, 1920×1080, letterbox scale; migrate player HUD; [`tickets/TICKET-0155.md`](tickets/TICKET-0155.md) |
| TICKET-0156 | Engine UI canvas stack + MCP/Lua clients | done | P2 | push/pop/show/hide; [`tickets/TICKET-0156.md`](tickets/TICKET-0156.md) |
| TICKET-0157 | MCP UI canvas mutate (add/remove/move/style) | needs-approval | P2 | Structural ops; parallel with 0158; [`tickets/TICKET-0157.md`](tickets/TICKET-0157.md) |
| TICKET-0158 | Editor Canvas view (drag + inspector) | needs-approval | P2 | Parallel with 0157; [`tickets/TICKET-0158.md`](tickets/TICKET-0158.md) |
| TICKET-0159 | Interactive focus + button + pause sample | needs-approval | P2 | Button type, modal input, pause sample |
| TICKET-0160 | Toggle + slider widgets | needs-approval | P2 | Interactive controls; editor + mutate; [`tickets/TICKET-0160.md`](tickets/TICKET-0160.md) |
| TICKET-0161 | Fill-edge scale + settings sample | needs-approval | P2 | `scaleMode` cover; sample `settings.uicanvas.json`; [`tickets/TICKET-0161.md`](tickets/TICKET-0161.md) |
| TICKET-0162 | Inventory UI canvas sample | needs-approval | P2 | Modal inventory screen stub; stack + focus; [`tickets/TICKET-0162.md`](tickets/TICKET-0162.md) |
| TICKET-0163 | Dialogue UI canvas sample | needs-approval | P2 | Modal dialogue screen stub; stack + focus; [`tickets/TICKET-0163.md`](tickets/TICKET-0163.md) |
| TICKET-0164 | UI image assets (textures on widgets) | needs-approval | P2 | Schema + placeholder draw; GPU textures follow-on; [`tickets/TICKET-0164.md`](tickets/TICKET-0164.md) |

## EPIC-0008: Character animation and audio

- Status: active
- Goal: Complete M5 remaining work — skeletal animation, root motion, events, IK/retargeting, audio, and related editor picking/nav polish.
- Roadmap home: **M5**.
- Priority guidance: TICKET-0101 done; TICKET-0102 is next M5 engineering (P0); remaining follow-ons P2; 0109 deferred P3.

| ID | Title | Status | Priority | Notes |
| --- | --- | --- | --- | --- |
| TICKET-0101 | glTF skeletal/skin data import path | done | P0 | Owner approved 2026-07-15; skeletal subset shipped; [`tickets/TICKET-0101.md`](tickets/TICKET-0101.md) |
| TICKET-0102 | Animation clip asset format + hot reload | needs-approval | P0 | Clip import + library hot reload shipped; awaiting owner approval; [`tickets/TICKET-0102.md`](tickets/TICKET-0102.md) |
| TICKET-0103 | Blend trees + layered animation state machines | proposed | P2 | Animator component + controller + Lua drive ([DEC-0022](../decisions/index.md#dec-0022-c-animator-backend-with-lua-drive-api)); [`tickets/TICKET-0103.md`](tickets/TICKET-0103.md) · [`../features/animator.md`](../features/animator.md) |
| TICKET-0104 | Root motion extraction and character sync | proposed | P2 | |
| TICKET-0105 | Animation events → gameplay/collision hooks | proposed | P2 | |
| TICKET-0106 | IK hooks + retargeting metadata | proposed | P2 | |
| TICKET-0107 | miniaudio integration + spatial/event playback | proposed | P2 | |
| TICKET-0108 | Full triangle mesh viewport picking | proposed | P2 | Deferred from M4; editor polish |
| TICKET-0109 | Recast/detour navmesh integration (beyond grid) | deferred | P3 | Not blocking M5 exit |
| TICKET-0110 | M5 exit: animation tests + CLI/editor previews | proposed | P2 | Exit gate |

## EPIC-0009: Editor completion and specialized tools

- Status: proposed
- Goal: Finish M10 specialized tools beyond the active editor MVP slice.
- Roadmap home: **M10**.
- Priority guidance: TICKET-0147–0151 are P2 (component authoring through collider overlays + expose existing colliders); other children remain P3 until M10 pull-forward.

| ID | Title | Status | Priority | Notes |
| --- | --- | --- | --- | --- |
| TICKET-0131 | Imported mesh thumbnails | proposed | P3 | |
| TICKET-0132 | Viewport gizmos for prefab part editing | proposed | P3 | |
| TICKET-0133 | Play-state save/resume | proposed | P3 | Beyond test-session reset |
| TICKET-0134 | World partition authoring UI | proposed | P3 | |
| TICKET-0135 | Animation preview/authoring tool | proposed | P3 | After EPIC-0008 |
| TICKET-0136 | Dialogue graph editor surface | deferred | P3 | **Superseded** by TICKET-0165–0168+ (extends TICKET-0053) |
| TICKET-0137 | Particle/VFX editor preview | proposed | P3 | Pairs with TICKET-0125 |
| TICKET-0138 | Profiler panel integration | proposed | P3 | |
| TICKET-0147 | Entity component authoring + MCP add component/script | needs-approval | P2 | DEC-0016/0017 implemented; awaiting owner approval; [`tickets/TICKET-0147.md`](tickets/TICKET-0147.md) |
| TICKET-0148 | Component reference: catalog and how they work | ready | P2 | Doc pass after 0147; core ECS + authored collider/scriptBinding; [`tickets/TICKET-0148.md`](tickets/TICKET-0148.md) |
| TICKET-0149 | Inspector: edit component props + open script | needs-approval | P2 | Implemented; awaiting owner approval; [`tickets/TICKET-0149.md`](tickets/TICKET-0149.md) |
| TICKET-0150 | Viewport green collider overlays (box/sphere) | needs-approval | P2 | Implemented; awaiting owner approval; [`tickets/TICKET-0150.md`](tickets/TICKET-0150.md) |
| TICKET-0151 | Expose existing prefab colliders as entity components | needs-approval | P2 | Implemented; awaiting owner approval; [`tickets/TICKET-0151.md`](tickets/TICKET-0151.md) |

## EPIC-0010: Particles and visual effects

- Status: proposed
- Goal: CPU/GPU emitters, effect graphs, budgets, and integration with gameplay events.
- Roadmap home: **M8**. Hold behind M5 animation unless needed earlier for foliage disturb hooks.

| ID | Title | Status | Priority | Notes |
| --- | --- | --- | --- | --- |
| TICKET-0122 | CPU/GPU emitter foundation + pooling | proposed | P3 | |
| TICKET-0123 | Effect graph assets + deterministic seeds | proposed | P3 | |
| TICKET-0124 | VFX collision hooks, LOD, bounds, budgets | proposed | P3 | |
| TICKET-0125 | VFX hot reload + CLI capture + editor preview | proposed | P3 | |
| TICKET-0126 | Integrate VFX with animation/combat/interaction | proposed | P3 | |

## EPIC-0011: Melee vertical slice integration

- Status: proposed
- Goal: Playable M9 slice in one streamed region.
- Roadmap home: **M9**. Depends on M5–M8 systems.

| ID | Title | Status | Priority | Notes |
| --- | --- | --- | --- | --- |
| TICKET-0127 | Lock-on + melee attack/dodge/stamina loop | proposed | P3 | |
| TICKET-0128 | Damage reactions + one weapon + one enemy | proposed | P3 | |
| TICKET-0129 | Wire inventory/quest/dialogue/save-load in one region | proposed | P3 | |
| TICKET-0130 | Combat VFX + audio + collision feedback pass | proposed | P3 | |

## EPIC-0012: Profiling, packaging, and ship gate

- Status: proposed
- Goal: Meet performance target, harden failures, package playable slice.
- Roadmap home: **M11**.

| ID | Title | Status | Priority | Notes |
| --- | --- | --- | --- | --- |
| TICKET-0139 | 1440p/60 FPS benchmark gate + GPU budgets | proposed | P3 | |
| TICKET-0140 | Failure-path hardening pass | proposed | P3 | |
| TICKET-0141 | Full regression suite green + publish benchmarks | proposed | P3 | |
| TICKET-0142 | Package playable vertical-slice build | proposed | P3 | |

## EPIC-0013: Rendering polish and typography

- Status: ready
- Goal: Cross-cutting visual QA — PBR implementation tracking, fonts, atmosphere regression, GPU diagnostics.
- Roadmap home: spans materials / M10 / M11.
- Priority guidance: P2 — same PBR workstream as TICKET-0040; not ahead of M5 P0.

| ID | Title | Status | Priority | Notes |
| --- | --- | --- | --- | --- |
| TICKET-0143 | PBR + transparency rendering slice | done | P2 | With 0040; masked/blended deferred fail-closed; [`tickets/TICKET-0143.md`](tickets/TICKET-0143.md) |
| TICKET-0144 | Typography/font roles + fallback + licenses | needs-approval | P2 | Roboto engine + Cinzel scene; [`tickets/TICKET-0144.md`](tickets/TICKET-0144.md) |
| TICKET-0145 | Dark-fantasy visual regression screenshot tests | proposed | P2 | |
| TICKET-0146 | GPU context in structured diagnostics/crash bundles | proposed | P2 | |

## M6 engineering (under EPIC-0006 hold)

These expand M6 beyond quest authoring tickets 0050–0051. Keep `proposed` / P3 until M5 animation exit or owner override.

| ID | Title | Status | Priority | Notes |
| --- | --- | --- | --- | --- |
| TICKET-0111 | Stats/items/inventory data models + validation | proposed | P3 | M6 |
| TICKET-0112 | Quest asset schema + validation CLI | proposed | P3 | Soft-aligned with TICKET-0050 (schema shipped); runtime is TICKET-0180 |
| TICKET-0113 | Abilities schema + Lua bindings (safe subset) | proposed | P3 | |
| TICKET-0114 | Versioned RPG save format + migrations | proposed | P3 | Persist QuestRuntime + StandingRuntime session state (follow-on to TICKET-0180/0181) |
| TICKET-0115 | Sandboxed Lua collision-query bindings | proposed | P3 | |
| TICKET-0116 | Extend Lua beyond interaction/combat hurt handlers | proposed | P3 | |
| TICKET-0152 | Lua host API v1 (log / json_decode / blackboard) | needs-approval | P2 | Owner override of M6 hold; [DEC-0023](../decisions/index.md#dec-0023-live-lua-host-api-agent-iteration-path); [`tickets/TICKET-0152.md`](tickets/TICKET-0152.md) |

## M7 engineering (under EPIC-0006 hold)

| ID | Title | Status | Priority | Notes |
| --- | --- | --- | --- | --- |
| TICKET-0117 | Branching dialogue graph runtime | deferred | P3 | **Covered** by TICKET-0052 |
| TICKET-0118 | Localization keys + voice reference fields | proposed | P3 | Soft dep for TICKET-0174 |
| TICKET-0119 | Dialogue save/resume state persistence | proposed | P3 | |
| TICKET-0120 | Headless dialogue traversal tests | deferred | P3 | **Covered** by TICKET-0052 / `world_forge` suite |
| TICKET-0121 | Command-backed dialogue graph editor | deferred | P3 | **Superseded** by TICKET-0165–0168+ (extends TICKET-0053) |

## Suggested work order

1. EPIC-0001 done — use Notion kanban + Priority for day-to-day flow.
2. **P0 needs-approval:** TICKET-0102 (animation clip format + hot reload) — then M5 follow-ons (0103+).
3. **P1 parallel:** EPIC-0003 story tickets, EPIC-0004 nav/map design; World Forge scope (TICKET-0010) awaiting owner approval — then schemas after 0021.
4. **P2 when capacity:** M5 follow-ons after 0102; World Forge schemas after 0021.
5. Schema-first World Forge (0011–0014, P2) after faction canon (0021).
6. Hold **P3** EPIC-0006/0007 and M6–M8 runtime tickets until M5 animation exit unless explicitly overridden.
7. Mini-map (EPIC-0007) after region/POI IDs exist.
8. M9–M11 after vertical-slice dependencies land.
