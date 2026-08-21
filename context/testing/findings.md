## 2026-08-21 — Iron-test armor sat correctly in Blockbench, floated in the engine

- Reproduction: Equip `iron_test_helmet` / chest / legs in combat-sandbox F5 (or Animation → Armor slots). Helmet hangs above/behind the head; greaves sit off the legs. The same shells look seated on `GoodPlayerModelCopy.bbmodel`.
- Impact: Modular kit authoring in Blockbench could not be trusted in play-test; Armor Fit TRS made it worse (origin scale).
- Cause: `player.gltf` is height-normalized to 2.75 m (`scale ≈ 1.124`). Kit bakes left vertices at raw Blockbench glTF scale and then reused the player's uploaded bone palettes (`jointWorld * playerIBM`). Those matrices are not valid on unscaled kit verts. Duplicate `keepMeshes` also kept the first mesh name instead of the densest copy.
- Resolution: Catalog `generic.matchPlayerBake` copies player scale + Hips offset onto kit shells. Draw uploads `armor:<itemId>` palettes from player animation locals remapped by joint name × the shell's own inverse binds. `keepMeshes` keeps the densest duplicate.
- Verification: Rebake `iron_test_helmet` / `chest` / `legs`; suite remap test; F5 / Armor slots screenshot with the helmet on the head.
- Remaining risk: If `GoodPlayerModelCopy` proportions drift from `GoodPlayerModel_rigged`, Hips alignment still cannot put a shell on a different head shape — refit in the copy or sync the body.



- Reproduction: F5 Runecaster cast — bolt went behind the player. Bow aim at a hostile — gold arc / shot peeled off the reticle as if bouncing.
- Impact: Ranged aim unusable at close/OTS ranges.
- Cause: `resolve_reticle_look_point` used an unfiltered solid ray that often hit the player’s Dynamic capsule between camera and hands, placing the look point **behind** the muzzle. Aiming at hostiles also competed with capsule hits.
- Resolution: Aim solid ray is StaticWorld-only (hurt volumes still set distance on the reticle ray). `muzzle_fire_direction` / `muzzle_look_direction` push aim forward along camera when the look point is behind the muzzle.
- Verification: Rebuild `engine`; F5 cast/fire at the hostile under the reticle.
- Remaining risk: Very close hostiles still use hurt-sphere projected distance (on-ray), not entity center.

## 2026-08-20 — Projectile solid ray still hit Dynamic capsules after owner ignore

- Reproduction: After owner hurt-sphere ignore + motion-body ignore, F5 bow/bolt still died on the player with impact VFX.
- Impact: Own shots still felt like self-collision.
- Cause: Closest `ray_cast` could still return the shooter’s Dynamic capsule (or another Character body) when the ignore body was missing/stale; combat volumes already owned fighter hits.
- Resolution: Projectile world stop uses `ray_cast` with `CollisionLayer::StaticWorld` only (terrain/static props). Fighters are hurt-volume only.
- Verification: Rebuild `engine`; combat suite; F5 fire/cast through the player into dummies.
- Remaining risk: Dynamic crates no longer stop arrows via solid ray (combat hurt still does if authored).

## 2026-08-20 — Projectile solid ray still “hit” the player without damage

- Reproduction: Combat sandbox F5, fire Outrider arrow or Runecaster bolt. Shot dies on the player with impact VFX; HUD HP unchanged (`combat_hurt.lua` already no-ops self `body` hits).
- Impact: Own shots feel like they collide with the shooter even though damage is skipped.
- Cause: Flight used an unfiltered `ray_cast` after the combat probe. The player’s dynamic capsule stopped the projectile even when the hurt-sphere owner ignore worked.
- Resolution: Pass the shooter’s motion body into `tick_play_test_projectiles` and ignore that body on the solid ray. Keep combat `ignore_placement_entity_id` + Lua no-op.
- Verification: Rebuild `engine`; F5 fire past the player into dummies with no self impact VFX.
- Remaining risk: Other dynamic bodies the shooter owns (none today) are not ignored; only the motion capsule.

## 2026-08-20 — Combat text / melee impact used hurt-sphere contact points

- Reproduction: Hit a moving hostile NPC (or Block an NPC swing). Damage numbers and sword/parry sparks appear offset from the mesh / name plate.
- Impact: Juice reads detached from the fighter; DEC-0059 wants text above the name, not on the contact surface.
- Cause: Overlap contact points sit on the hurt-sphere surface (fixed radial offset). `npc_hurt.lua` / Block text used `contactPoint`; melee VFX used the same.
- Resolution: Annotate `hurtEntityPosition` from the scene transform on combat events; Lua anchors floaters above feet+head; melee impact/parry VFX spawn at entity torso height.
- Verification: Rebuild `engine`; F5 hit the chasing NPC and confirm numbers/sparks sit on the body / above the HP chip.
- Remaining risk: Static dummy pad anchors remain as a fallback when the scene transform is missing.

## 2026-08-20 — Own arrows/bolts hit the player hurt sphere

- Reproduction: Combat sandbox F5, equip Outrider shortbow or Runecaster focus, fire. First flight sample overlapped the player's `combatHurt: body` (nock/muzzle starts inside it).
- Impact: Player took HUD damage from their own projectile; shot died on the shooter instead of reaching the dummy/NPC.
- Cause: Segment probes used attacker id `player_attack` (not the placement id). Melee already skipped `attacker_entity_id`; projectiles did not.
- Resolution: `query_combat_hits*` take `ignore_placement_entity_id`; play-test projectiles store the spawn entity and pass it. `combat_hurt.lua` also no-ops `player_attack` + `body`.
- Verification: `combat` suite overlapping-shooter ignore; live F5 after rebuild.
- Remaining risk: No faction filter yet — only the owning placement is ignored.

## 2026-08-20 — Moving player/NPC then Save broke combat-sandbox fighters

- Reproduction: Combat sandbox. Gizmo-move `player` and/or `npc_humanoid` (often the NPC stays selected), Ctrl+S, F5. Or Save during/after play-test after walking across a 128 m cell.
- Impact: Capsules and hurt sensors stayed at the old pose (mesh moved, write-back snapped them back, melee missed). Selecting the NPC then F5 possessed the clone because the placement had `characterAsset`. Save after physics follow could persist `WORLD-PLACEMENT-CELL-MISMATCH`, so the next world load failed closed.
- Cause: `PlacementCollisionTracker` skipped transform rebuilds for `physics_driven` bodies; `Scene::set_transform` did not retag `placement.cell`; NPC placement was stamped as a player spawn.
- Resolution: Authored moves teleport motion + follow-sensors; `set_transform` retags cells (including physics write-back and End Test restore); F5 spawn skips `npcAi` prefabs and sync strips their `characterAsset`.
- Verification: `combat` hurt-follow gizmo query; `world` set_transform cell retag; combat-sandbox `npc_humanoid` has no `characterAsset`.
- Remaining risk: The sandbox heightfield is a small pad around origin — moving characters far off it still has no floor.

## 2026-08-20 — Leftover `magicCast` base transition left the player in T-pose

- Reproduction: After moving MagicCast onto `upperBody`, play-test or scene attach of `player.animator.json`.
- Impact: Controller failed closed (`ANIM-CTRL-TRANSITION-FROM-MISSING`); skinned player stayed in bind / T-pose (no Idle).
- Cause: Base layer still had `from: magicCast` → `fall` after the state was removed from base. Overlay already had ungrounded → `empty`.
- Resolution: Delete the leftover base transition. Animator suite now loads the sample controller and rejects a leftover `transition.from`.
- Verification: Sample `player.animator.json` parse; play-test attach Idle.
- Remaining risk: Moving any state between layers still needs a full grep of `from`/`to` on the old layer (and timeline events).

## 2026-08-20 — `copy_pose_at` collapsed the Block skeleton

- Reproduction: Animation Studio `edit_clip` Block; `copy_pose_at` `fromTime=0.25` `time=0` (or other stamp times) on the player sagittal subject.
- Impact: Spine/chest/arm bind translations were replaced with `{0,0,0}` (and visual/reflected locals on arm trans tracks). The mesh shrank toward the hips; later `offset_keys` looked like they did nothing useful.
- Cause: The op sampled **skinned/visual** pose JSON (missing-path translation defaults to `{0,0,0}`) and upserted T+R+S for every joint, creating translation keys on rotation-only bones. Runtime clip translation **replaces** rest locals.
- Resolution: `copy_clip_pose_at` copies only existing authored channels; `copy_pose_at` / `set_pose` use that path (plus `times[]` to flatten a hold). Explicit `poses[]` writes rotation always, T/S only when the clip already has that path. `offset_keys` also honors `times[]`. `set_bone_gizmo` accepts `enableGizmo` as well as `enabled`.
- Verification: Suite `0263 copy_pose_at` checks; live Block hold flatten after rebuild.
- Remaining risk: Agents that pass skinned `sample_pose` JSON into `set_pose` `poses[]` still must not expect missing T/S paths to be created from rest.

**Recurring asset classes** (missing player clips, muddy foliage atlases, corrupted Tier-1 props): follow and extend [`recurring-asset-failures.md`](recurring-asset-failures.md). When the same failure class hits again, fix the asset, then update that playbook ? do not leave the next agent to rediscover the path.

## 2026-08-20 — Animation Studio stomped MCP `edit_clip` to Idle every frame

- Reproduction: Prefab subject with controller; runtime stays on Idle; `edit_clip` `RunStrafeLeft` (or WalkStrafe); wait a frame; `list_keys` / `upsert_keys` report `clipName: Idle`.
- Impact: Strafe/lean keys wrote the wrong clip or appeared lost; `offset_keys` looked broken on prefab subjects.
- Cause: Animation Studio UI called `ensure_anim_studio_clip_edit` on the runtime active clip every frame; `ensure_anim_studio_edit_clip_for_joint_source` did the same after preview.
- Resolution: Do not auto-attach active clip when an edit buffer is already open; keep dirty buffers across preview; clean `set_state` still pins the state's motion. Status can show `edit:… | play:Idle`.
- Verification: After rebuild, `edit_clip` → wait → `list_keys` stays on `RunStrafeLeft`; upsert + `save_override` writes the sidecar.
- Remaining risk: Human authors who relied on every-frame follow of the preview clip must use state buttons / `set_state` to switch the edit buffer.

## 2026-08-20 — Combat hits stopped after ~32 contacts (Lua cursor desync)

- Reproduction: Combat sandbox play-test; land many melee/projectile hits on dummies (or flood `recent_combat_events`). After the Diagnostics ring trims past 32, further swings/arrows stop updating dummy HP / HitReact even though contacts may still appear.
- Impact: Training pad feels broken mid-session; “hitting one dummy then everything stops.”
- Cause: `recent_combat_events` / `recent_interaction_events` erased from the front when `size() > 32` **without** repairing `lua_dispatched_*`. Dispatch then starts past `size()` and never consumes new appends. Trim also ran *before* dispatch. Continuous Hit-volume sweeps during play could accelerate the flood.
- Resolution: `trim_dispatched_event_queue` repairs the cursor; dispatch runs before trim; continuous Hit-body enqueue is skipped while a play-test session is running (melee/projectile own Lua combat). Dummy health chips no longer follow attack `contactPoint` (`dummy_hurt.lua` pad head anchors).
- Verification: `combat` suite trim/cursor checks; rebuild `engine`; live sandbox swings keep applying after dozens of hits.
- Remaining risk: Intentional multi-target cleave inside the 1 m melee probe can still hit more than one nearby hurt volume in one swing.

## 2026-08-19 — Scene move dropped per-world terrainData

- Reproduction: Open `worlds/combat-sandbox.world.json` with a `terrainData` footer, flatten/paint via MCP, then `engine_terrain_apply` `save` / `engine_scene_apply` `save`.
- Impact: Height/paint/foliage wrote into the **shared** overworld stores at origin; the world save omitted `terrainData`. Campaign sandbox/landfall origin would have been flattened.
- Cause: Same class as the cinematic `presentation_` move bug — `Scene` move ctor/assign copied presentation but not `terrain_data_paths_`, so `state.scene = std::move(loaded)` left instance paths on the moved-from object.
- Resolution: Move `terrain_data_paths_` in ctor/assign; restore shared terrain from git; re-flatten onto `assets/terrain/instances/combat-sandbox/`.
- Verification: `world` suite 63/63 (terrainData round-trip + move); live save writes instance JSON and keeps the world footer.
- Remaining risk: Water is still project-global (DEC-0056). Do not carve water on the combat pad.

## 2026-08-07 — Dirt roads stay bumpy with circular set_height stamps

- Reproduction: Grade a long approach corridor with many overlapping `set_height` / `flatten` / `gentle_hill` brushes along a path; paint with `paint_along`.
- Impact: Road reads as washboard / bumpy under the player even when average grade is walkable; `paint_along` only fixed material, not height continuity.
- Cause: Height MCP had polyline ops for rivers (`carve_channel`) and materials (`paint_along`) but no graded bed-along-polyline; circle overlaps leave ridges between stamp centers. Early `set_height_along` still stamped circles along the path.
- Resolution: `set_height_along` / `grade_along` use a continuous **strip** grade (`apply_grade_strip`) with lateral `halfWidth` + soft `skirtWidth`. `plateau` / improved `flatten_pad` use hard core + soft skirt for keep islands.
- Verification: Suite covers strip mid-path interpolate + plateau; live regrade of landfall approach corridor.
- Remaining risk: Very sharp cliffs outside the skirt still need a wider plateau/skirt; sample spacing for optional follow-up `smoothPasses` still matters.

## 2026-08-07 — stamp_compositions ignored part x/y/z/scale shorthand

- Reproduction: `engine_scene_apply` `stamp_compositions` with `parts:[{primitive:"capsule", y:0.9, scale:[0.6,1.8,0.6]}]` (no nested `transform`).
- Impact: Synthesized Graybox prefabs wrote identity transforms (`scale` 1,1,1 at origin) — wheelbarrows / scale mannequins looked like unit cubes/capsules until rewritten via `engine_prefab_apply`.
- Cause: `build_graybox_prefab_json` only copied `part.transform`; shorthand placement fields used by agents were dropped.
- Resolution: Accept nested `transform` **or** shorthand `x`/`y`/`z` plus `scale` number/array when building the prefab JSON.
- Verification: Rebuild `engine`; stamp a capsule with shorthand scale and confirm the written `*.prefab.json` stores the authored position/scale.
- Remaining risk: Existing broken Graybox prefabs written before the fix stay wrong until rewritten / re-stamped.

## 2026-08-07 — stamp_prefabs ignored top-level snapToTerrain=false

- Reproduction: On `presentation: cinematic_instance` worlds (`prologue-throne`, `appearance-courtyard`), call `engine_scene_apply` `stamp_prefabs` / `stamp_compositions` with root `snapToTerrain: false` and stamp transforms at `y: 0`.
- Impact: New graybox pews/towers/banners snapped onto the open-world heightfield (~±1–4 m), floating above or sinking through the cinematic floor plate until a follow-up `move` with per-op `snapToTerrain: false`.
- Cause: Each stamp op defaulted `snapToTerrain` to `true` when the stamp object omitted the key; the request-level flag was never copied onto ops.
- Resolution: Inherit `params.value("snapToTerrain", true)` when a stamp omits the key (both stamp paths in `editor_session.cpp`).
- Verification: Rebuild `engine`; stamp with root `snapToTerrain: false` and confirm placed `y` matches authored transform (not terrain height).
- Remaining risk: Per-stamp `snapToTerrain: true` still wins when set explicitly — intended.

## 2026-08-07 — Menu shows previous cinematic props + stacked UI chrome

- Reproduction: Appearance courtyard (or prologue) → restore `main-menu.world.json`. Game tab briefly/permanently shows courtyard graybox floating in the menu forest; Appearance titles/frames bleed behind the main menu plate.
- Impact: Main Menu Preview looked like mixed scenes; New Game loop felt broken after Confirm / Back.
- Cause: (1) Async `static_render_cache` keeps drawing the previous world's instances until the rebuild job finishes — `open_project_world_now` only set dirty and did not clear/discard. (2) `UiCanvasStack::draw_overlay` draws every modal on the stack; pushing `main_menu` without hiding `character_creation_appearance` leaves appearance chrome under transparent menu plates. `awaiting_landfall` also never pushed `main_menu`.
- Resolution: On world open, reset the cache job, clear `static_render_instances` / lights, then dirty. Lua `show_main_menu_clean` + `awaiting_landfall` push menu after hides; C++ menu-preview push strips opening canvases first.
- Verification: Appearance Confirm / Back → menu forest only (no courtyard ghost); stack top `main_menu` alone; no Appearance titles behind the plate.
- Remaining risk: Other long-running async caches (foliage pack) should follow the same clear-on-world-switch pattern if ghosts appear.

## 2026-08-07 — Prologue event timeline never ticked (static camera)

- Reproduction: New Game → `prologue-throne` cinematic instance under dialogue chrome. Authored `evt_prologue_throne` `look_at` pans never moved; Game view stayed on the establishing pose.
- Impact: A0-01 read as a still backdrop with UI only — no aisle approach / Luceran zoom / wide pullback.
- Cause: `EventTimelineRuntime::tick` ran only when `test_session_running() && simulation_allowed()`. Opening boot starts the sequence from Lua with **no** play-test session, so `look_at` alpha stayed at 0. Secondary: between beats / after unlock, `apply_opening_cinematic_camera` re-snapped establishing (or re-posed with height=2) and fought any motion.
- Resolution: Tick whenever a sequence is active unless play-test is paused; hold the last DebugCamera pose while control-locked or after a completed shot (`opening_cine_shot_pose_valid`); log `start_event_timeline` failures from `on_opening_boot`.
- Verification: Rebuild `engine`; New Game → Game tab — camera distance/yaw change across the three `look_at` beats while prologue plate is up.
- Remaining risk: World Forge `events` apply still writes disk only — live editor keeps the events asset loaded at boot until restart (same as other WF kinds without live rebind).

## 2026-08-07 — World `presentation` dropped on Scene move

- Reproduction: Load `prologue-throne.world.json` (`presentation: cinematic_instance`) via New Game / `open_project_world_now`. Status still streamed open-world terrain; Game stayed black / wrong focus cell until camera was forced onto graybox.
- Impact: Cinematic-instance sandbox plate and camera snap never activated; Ken Burns blackout path also hid any accidental 3D.
- Cause: `Scene` move ctor / move assign copied partition metadata but not `presentation_`, so `state.scene = std::move(loaded)` kept the previous world's presentation (or default `open_world`).
- Resolution: Move `presentation_` in ctor/assign; Game blackout only for 2D stills prologue on non-cinematic worlds; pose DebugCamera on cinematic open.
- Verification: New Game → graybox throne under dialogue at Terrain @ (0,0); Skip → `main-menu.world` + `character_creation`. Suite asserts move preserves presentation.
- Remaining risk: Other Scene fields added later need the same move coverage.

## 2026-08-06 — Prop / shadow flicker from instance rows rewritten while in flight

- Reproduction: `main-menu.world.json` Game tab at ~45 FPS. Record the window at the app frame rate and diff adjacent frames: tree canopies and their cast shadows change shading every few frames, and a cast shadow drops out entirely for one or two frames before returning.
- Impact: Visible texture/shading flicker on props and their CSM shadows once the editor ran fast enough to perceive per-frame changes. At ~8 FPS (before the GPU-hang fixes) it read as a slideshow and was missed.
- Ruled out first: mesh LOD hysteresis (`ENGINE_LOD_TRACE` shows 40 transitions on frame 2 then **0** for the rest of the session), cascade-0 caster culling (caster count settles at 390 and never changes), skipped draws (`draw_range_is_valid` never fires), vsync (editor presents with sync interval 1), and the debug layer (no messages, no device removal).
- Cause: `prop_instance_buffer_` and `shadow_instance_buffer_` were single upload heaps rewritten at **offset 0** by every pass. `wait_for_current_frame` only drains frame N-2, so the CPU overwrote rows that frame N-1's GPU draws were still reading. The same buffer was also rewritten *within* a frame — twice for the two editor viewports, and once per cascade whenever `shadow_props_all_cascades` is set — so earlier recorded draws read the last writer's rows.
- Confirmation before fixing: an `ENGINE_SERIALIZE_FRAMES` switch that drained to the newest submission removed the artifact completely (shadow-region p95 adjacent-frame delta 37,758 → 2,669) at the cost of dropping 45 → 30 FPS.
- Resolution: `InstanceArena` — one upload heap partitioned into a slot per in-flight frame, bump-allocated within the slot, with the base VA folded into the root SRV. Growth retires the old allocation (draws already recorded still point into it) and restarts the cursor in the fresh allocation. See [`csm-shadows.md`](../features/csm-shadows.md).
- Verification: Same measurement on the fixed build at full speed — p95 1,381 / max 2,017 (was 37,758 / 40,777), 46.7 FPS, busiest scanline down from 56 differing pixels to 4; six consecutive frames show a stable trunk, canopy, and cast shadow; zero errors or warnings logged for the session.
- Remaining risk: Other single-buffered per-frame uploads (`particle_vertex_buffer_`, terrain/water vertex buffers) still write offset 0 each frame. They did not flicker here because their contents are stable per frame, but they are the same hazard and should move onto the arena when they gain per-frame variation.

## 2026-08-06 — Scene render_submit dominated by per-prop CSM shadow draws

- Reproduction: Dense `main-menu.world.json` Scene tab after UI overlays were cheap; `render_submit` ~15–16 ms, GPU ~8 ms, ~19k instances.
- Impact: CPU-bound editor FPS (~35–40) while the GPU sat idle after ~8 ms.
- Cause: Lit props were instanced, but CSM casters issued one `DrawInstanced(..., 1)` + root-constant model upload per prop × 3 cascades.
- Resolution: Shadow VS reads a `StructuredBuffer` of model rows; casters batch by mesh+skin like the lit prop pass. See [`csm-shadows.md`](../features/csm-shadows.md).
- Verification: Live Scene after fix — `render_submit` ~12.5 ms (was ~15–16), FPS ~44–45 (was ~38), GPU still ~7 ms.
- Remaining risk: Scene only casts props into cascade 0; Game/play still uses all cascades. GPU cull / ExecuteIndirect / shadow LOD remain follow-ups.

## 2026-08-06 — Scene tab ~14 FPS from Show event zones (default on)

- Reproduction: Open `main-menu.world.json` Scene tab with Diagnostics **Show event zones** on (~400 entities).
- Impact: `editor_ui` ~45 ms/frame; Scene ~14–16 FPS while GPU stayed ~4–7 ms.
- Cause: Overlay called `effective_collision_volumes` for every entity every frame (Debug vector alloc/copy), even when most had no interaction volumes.
- Resolution: Default **Show event zones** off; cache entity ids that have interactions and rebuild only on `scene.edit_revision()`. See [`collision-debug.md`](../features/collision-debug.md).
- Verification: Live MCP — overlays off jumped Scene to ~38 FPS / `editor_ui` ~6.5 ms; rebuild confirms default-off path.
- Remaining risk: Enabling the overlay still walks interaction entities each frame for draw; dense interaction worlds may need frustum cull.

## 2026-08-06 ? Forest densify hit scene batch 100-op cap / no scatter / no float audit

- Reproduction: Calrenoth `main-menu.world.json` polish; `snap_to_terrain` by `namePrefix` and multi-stamp densify needed hundreds of moves; floaters found by hand-scanning list Y.
- Impact: Agents chunked snaps, hand-authored stamp grids, and missed canopy floaters.
- Resolution: Scene batch max **512**; `stamp_scatter` densify; `list`/`query` `floatGap` + `floatGapMin`/`floatGapMax`; `snap_to_terrain` `groundOffsetsByPrefab` / `usePrefabGroundDefaults`; terrain `batch` accepts `sample` + smart sculpt. See [`mcp-live-editor.md`](../features/mcp-live-editor.md).
- Verification: Suite tests for scatter/float audit/smart+sample batch; rebuild `engine`.
- Remaining risk: Scatter still AABB-only (no material mask); supply-chest editor labels can still draw high.

## 2026-08-06 ? Graybox keep/towers looked hollow (missing faces)

- Reproduction: Scene tab on `main-menu.world.json`; Calrenoth graybox cubes/cylinders showed black voids on tops and some walls.
- Impact: Keep silhouette unreadable; looked like broken primitives rather than solid graybox massing.
- Cause: Prop instance PSO used `D3D12_CULL_MODE_BACK` with D3D's default CW-front. Compositional primitives are CCW-wound, so tops were culled.
- Bad fix attempt: `FrontCounterClockwise = TRUE` made graybox solid but hollowed CW-wound kit meshes (oaks/bushes/stones).
- Resolution: Prop pipeline `CullMode = NONE` so mixed CW kit meshes and CCW graybox both draw. Updated [`materials.md`](../formats/materials.md).
- Verification: Rebuild `engine`; relaunch editor; graybox tops + kit props solid.
- Remaining risk: Slightly higher overdraw; long-term fix is unify mesh winding + optional per-material cull.

## 2026-08-05 ? Main-menu chrome looked green / chewed, title logo missing iron

- Reproduction: Boot `main-menu.world.json`, open Game / Main Menu Preview. Panel and buttons showed neon green bars; Wrathful Conquest logo looked hollow / sky showed through letter strokes.
- Impact: Menu chrome unusable for screenshots and playable first draft.
- Cause: Three stacked issues. (1) v2 green-screen PNGs had chroma green baked into RGB (up to ~6% of visible pixels on secondary buttons) ? alpha despill cannot fix that under ImGui straight-alpha blending. (2) `_punch_title_logo.py` keyed dark iron by luminance, opening ~1.7k interior pinholes. (3) ImGui's D3D12 backend clamps its sampler to mip 0, so a 1496?758 logo minified into a ~560?240 box dropped texels and looked chewed.
- Resolution: Regenerated opaque stained-glass plates (`glass-menu-*.png`, `_prepare_glass_chrome.py`); regenerated title logo v2 (the opaque v1 source itself had chewed C/T RGB, so punch could not invent missing geometry); CPU area-prefilter UI PNGs toward the widget box on load (`resample_rgba_area` / `ui_texture_prefilter_size`); scale nine-slice insets by canvas scale. `_repair_title_logo.py` remains for legacy punched copies.
- Verification: Hud suite + all 24 suite tests; live Game-tab capture with glass panel, no green bars, solid logo strokes.
- Remaining risk: Texture cache keys by draw-size bucket ? very large resizes may upload a second SRV. Glass plates are opaque (no soft fringe). Logo v2 is AI-generated prototype art pending a Dom redraw.

## 2026-08-05 ? Granted ammo was invisible in the inventory panel

- Reproduction: Start a play test as Outrider (20 `crude_arrow` granted), open the inventory. No arrows anywhere in the panel.
- Impact: Ammo appeared lost. Bow draw gating on ammo looked broken because the player had no way to see the stock.
- Cause: Two gaps. `InventoryRuntime` routes ammo-tagged items to a separate `ammo_` region (deep stacks) that `inventory.uicanvas.json` never rendered, and `push_inventory_status` pushed ammo stacks without the catalog decoration (`icon`, `displayName`, `kind`) that bag and hotbar entries receive, so `entry.icon` was nil in Lua.
- Resolution: Decorate ammo stacks from the item catalog in `push_inventory_status`; add an **Ammo** label plus three slots (`inventory.ammo.0..2`) to the inventory canvas and populate them in `sync_inventory_ui`; report the region as `ammo` from `engine_inventory_call status`.
- Verification: Rebuild `engine`; MCP status reports `ammo: crude_arrowx20`; inventory screenshot shows the arrow icon with `x20`.
- Remaining risk: Only three ammo slots are rendered while the region is unbounded. Ammo is display-only ? it cannot be selected, inspected, or dragged yet.

## 2026-08-05 ? Held melee input could leave a stale Attack2 trigger

- Reproduction: Hold LMB through the Ashfell three-hit string, release, then start another attack. Intermittently the next string appears to begin at Attack2.
- Impact: Attack1 is visually skipped, breaking the authored chain order and pose continuity.
- Cause: While the Attack1?Attack2 crossfade still reported `attack` as the current state, held-input polling re-armed the Attack2 trigger every frame. The first trigger started the transition; a duplicate could remain pending for the next string.
- Resolution: Track the current combo input state and latch once it advances. Held LMB may queue each successor once, but cannot reissue that state?s transition trigger during its crossfade.
- Verification: Rebuild `engine`; animator suite; repeat held and clicked strings in play-test and confirm every fresh press starts at Attack1.
- Remaining risk: Other gameplay systems that continuously set trigger parameters should use the same one-shot pattern rather than relying on transition consumption timing.

## 2026-08-05 ? delete_key failed with No channel after set_joint

- Reproduction: Animation Studio MCP `set_joint` `RightHand` then `delete_key` `time` on Attack sword-arm rotation ? `No channel` (exit 5).
- Impact: Agents could not sparsify dense Attack tracks; had to upsert first just to set `edit_path`, or avoid deletes.
- Cause: `set_joint` ignored `path`, so `anim_studio_edit_path` stayed translation. Sword-arm channels are rotation-only ? `find_anim_studio_edit_channel` returned null. Sagittal skin/channel remap was fine once path was rotation.
- Resolution: `set_joint` accepts optional `path`; `delete_key` / `delete_keys` accept `joint`+`path` (+ `times[]` / `keys[]`) like upsert. Docs in `animation-studio.md` + MCP schema blurb.
- Verification: Rebuild engine; `delete_key` with `joint=RightHand` `path=rotation` `time=?` removes the nearest key without a prior upsert.
- Remaining risk: Still cannot delete the sole remaining key on a channel (by design).

## 2026-08-05 ? Attack jitter from ease_segment + sync_gltf

- Reproduction: Polish Attack with `ease_segment` on dense sword tracks, then `save_override` and/or `sync_gltf`. Playback zigzags (e.g. RightUpperArm ~?70? at 0.28, ~+14? at 0.29, back to ?70?).
- Impact: Melee clip looks choppy; `replace_from_source` alone does not recover if Sync already rewrote `player.gltf`.
- Cause: `insert_ease_breakdowns` sampled only endpoints and upserted absolute eased keys between them, leaving denser authored keys in place ? LINEAR interp fought itself. Sync baked that into glTF.
- Resolution: Restored clean Attack from HEAD glTF; re-applied sparse free-arm `offset_keys` only (no ease); deleted conflicting free-arm originals at 0.48/0.58; `save_override` sidecar. `ease_segment` now clears keys strictly inside `(t0,t1)` before inserting breakdowns. Regression in animator suite.
- Verification: `list_keys` sword arm ~8 clean times; free-arm counterbalance without mid-key snaps; suite + live seek/screenshot.
- Remaining risk: Prefer pose-to-pose spacing over ease on already-dense Mixamo tracks; avoid `sync_gltf` for experimental polish.

## 2026-08-05 ? Fire particles visible through terrain (NDC soft occlusion)

- Reproduction: Play-test near campfire/torch; look so terrain (ridge, bank, ground) sits between the camera and the flame. Fire billboards show through solid ground depending on angle/distance.
- Impact: Soft-particle ?hide behind geometry? fails for terrain at typical play distances; reads as a terrain depth bug.
- Cause: Occlusion compared raw NDC depth with a scale of ~48. With near=0.1 / far=2000, mid-range meter-scale occluders produce NDC deltas ? 1/48, so `depth_fade` stays near 1. Hardware depth test is off (depth bound as SRV for soft particles), so there is no fallback.
- Resolution: Linearize scene and particle depth to eye-space meters; soft fade over a ~0.45 m band. Pass camera near/far on `WorldPassParams` into the particle cbuffer.
- Verification: Rebuild `engine`; reset editor+MCP; look at campfire through a terrain ridge ? flame should hard-hide behind ground, soft only at contact.
- Remaining risk: Soft band is a global constant; very large billboards that straddle a thin occluder may still fringe slightly at the edges.

## 2026-08-05 ? Particles never hide behind scene geometry

- Reproduction: Place campfire/torch or cast arcane bolt behind a rock/wall/character; billboards draw on top of occluders as if depth were ignored.
- Impact: Soft-particle occlusion (and hard hide-behind) appears broken in editor and play-test until the window is resized at least once.
- Cause: Soft particles sample scene depth via an SRV on the particle descriptor heap. That SRV was only created in `recreate_targets`, which runs during device init **before** `create_particle_pipeline` builds the heap ? so the first boot skipped the bind. `create_particle_pipeline` filled texture slots but never wired the depth slot; `particle_depth_gpu_` stayed null and the draw pass skipped the depth root table.
- Resolution: `update_particle_depth_srv()` called at the end of `create_particle_pipeline` and again from `recreate_targets` on resize.
- Verification: Rebuild `engine`; restart editor; particles behind opaque meshes should disappear (soft fade only at intersections).
- Remaining risk: Superseded by the linear-depth finding above (NDC soft scale was still wrong at range even after the SRV bind).

## 2026-08-04 ? Restarting only `engine.exe mcp` leaves live tools dead

- Reproduction: After a C++ rebuild, kill all `engine.exe`, relaunch (or let Cursor respawn) `engine.exe mcp --project samples/open-world-rpg`, then call `engine_animation_call` / `engine_editor_screenshot`. Status reports `editorRunning: false` / ?Editor live automation is not connected? even though MCP tools list.
- Impact: Animation Studio, screenshots, scene apply, and other bridge ops look ?broken MCP? when the editor window was simply never restarted. Easy to waste a cycle fixing MCP or reloading Cursor.
- Cause: Two processes. Cursor?s MCP server is a **windowless stdio proxy** (`engine.exe mcp`) that forwards over the named-pipe bridge. The **editor** (`engine.exe editor --project ?`) owns the UI and the pipe. Killing `engine` during rebuild takes both down; restarting only the MCP half is not enough. `engine_editor_live enable` only writes a request file ? nothing consumes it until an editor is running.
- Resolution: After kill ? rebuild, relaunch **both**: (1) editor window, (2) MCP (Cursor usually respawns `mcp` on the next tool call / `mcp_auth`). Confirm `engine_editor_status` shows `editorRunning: true` before animation/scene work. See [`reset-editor-after-edits.mdc`](../../.cursor/rules/reset-editor-after-edits.mdc) and [`mcp-live-editor.md`](../features/mcp-live-editor.md).
- Verification: Launch editor + MCP ? `engine_editor_status` live ? `engine_animation_call` `kind=status` succeeds.
- Remaining risk: Two `engine.exe` images both lock the binary; always `Get-Process engine` (no window-title filter) before MSBuild (see LNK1168 finding below).

## 2026-08-04 ? Viewport gizmos draw but never drag (weld/bone/placement)

- Reproduction: Animation (or Scene) ? Enable Weld Gizmo on outrider_shortbow ? axes appear at the mesh; click-drag does nothing. Grip Offset float fields still move the bow.
- Impact: HandAttach authoring via gizmo unusable; bone gizmo and entity gizmos share the same failure class.
- Cause: `ImGuizmo::CanActivate` refuses a `MouseClicked` while any ImGui item is hovered/active. The viewport covered the RT with `InvisibleButton("##viewport_area")`, so every LMB made that button active and Manipulate never entered `IsUsing`. Separate earlier issue: Animation dropped panel aspect tracking (window aspect vs panel `SetRect`).
- Resolution: (1) `Dummy` instead of full-rect `InvisibleButton` ? Viewports windows are `NoMove` so the button is not needed to stop window drag. (2) Animation tracks `scene_viewport_min/max` like Scene. (3) `ImGuizmo::BeginFrame`; studio hand-socket seeding for weld.
- Verification: rebuild `engine`; Animation ? outrider_shortbow ? Enable Weld ? drag G/R/T axes; Scene placement gizmo still moves selections.
- Remaining risk: if a future viewport window drops `NoMove`, content-area window drag can return ? gate that with title-bar-only move or a non-activating hover probe.

## 2026-08-04 ? Inventory slot drag never moved items

- Reproduction: F5 play-test, open inventory (`I`), press-drag a bag or hotbar item onto another slot. Ghost never appears; item stays put. Click-select on slots also feels dead.
- Impact: Act 0 loot slice unusable for bag?hotbar equip besides Lua/MCP set_hotbar; desktop QA reports ?drag broken.?
- Cause: `UiCanvasStack::handle_modal_input` starts inventory drag on `mouse_down` and completes on `mouse_held`/`mouse_released`, but `process_test_session_ui_input` only set `mouse_clicked` + one-shot `mouse_pos` when the Game viewport was hovered. `mouse_down`/`mouse_held`/`mouse_released` stayed false, so the drag path never started.
- Resolution: feed continuous press flags from ImGui (`IsMouseClicked` ? down, `IsMouseDown` ? held, `IsMouseReleased` ? released) and always update `mouse_pos`. Hold/release still apply if the cursor leaves the viewport mid-drag. Suite regression on sample inventory canvas: bag.0 ? hotbar.0 emit `drag_from_bind` / `drag_to_bind`.
- Verification: `engine_suite_tests --suite hud`; play-test inventory drag bag?hotbar.
- Remaining risk: drops onto non-slot chrome still no-op by design; `inventory.bagEquip.*` stubs remain no-moves.

## 2026-08-02 ? `LNK1168` on `engine.exe` with no editor open (respawned MCP server)

- Reproduction: close the editor, then `MSBuild /t:engine` ? `LINK : fatal error LNK1168: cannot open build/windows-msvc-debug/dev-next/engine.exe for writing`, even though no editor window exists.
- Impact: repeated link failures and wasted rebuild cycles; easy to misread as an antivirus hold and start guessing.
- Cause: the project MCP server is `engine.exe mcp --project ...`, it has no window, and the IDE respawns it on the next MCP call. So after killing the editor there is still a windowless `engine.exe` holding the image, and killing it once is not enough because a later MCP tool call brings a new one back.
- Resolution: before MSBuild, check `Get-Process engine` (do not filter on `MainWindowTitle`) and stop every match. If the lock persists, `Rename-Item` the locked `engine.exe` aside ? the linker then does a full link into a fresh file, and the stale image can be deleted once its holder exits.
- Verification: reproduced twice during TICKET-0247; both times a windowless `engine.exe` was present and the rebuild succeeded immediately after the rename or the kill.
- Remaining risk: after killing the MCP server the IDE's MCP client reports "Not connected" until it reconnects, so finish MCP-driven verification before the rebuild, or fall back to the equivalent `engine` CLI commands (for example `engine build-coordination --action release`).

## 2026-08-02 ? Stale `%TEMP%/ai-rpg-engine-test` fails three foundation checks

- Reproduction: run `engine_tests.exe` twice in a row. The second run reports `FAIL` for "Project validation exposes structured asset and entity metrics", "Missing asset dependency is detected", and "Asset database rebuild is incremental".
- Impact: looks like a regression from whatever you just changed; the three failures are unrelated to the working tree.
- Cause: `tests/foundation_tests.cpp` builds its sandbox at `temp_directory_path()/"ai-rpg-engine-test"` without clearing it first, and the pre-existing crash after "Asset database rebuild is incremental" (see the 2026-07-28 `regression_all` entry) means the `remove_all` at the end of `main` never runs. The leftover `assets/missing.txt` and `out/assets/registry.json` then break the empty-project and missing-dependency assertions.
- Resolution: delete `%TEMP%/ai-rpg-engine-test` before running `engine_tests.exe`. Also note the suite split: `engine_tests.exe` is the foundation suite and ignores `--suite`; the per-suite tests live in `engine_suite_tests.exe --suite <name>`.
- Remaining risk: the underlying crash and the missing sandbox reset are still open; a proper fix is `remove_all` at the *start* of `main`.

## 2026-08-02 ? Play camera "overpowered" the Scene view (single-camera frame globals)

- Reproduction: F5 play-test, switch to the Scene tab, fly the free-cam. The camera moves but the world reads as the play view ? empty ground away from the player, particles facing the wrong way, and a stretched image at fixed play resolutions.
- Impact: Scene was not usable for inspection during play even after the free-cam was unlocked.
- Cause: only the camera *pose* was duplicated (`DebugCamera` vs `OrbitCamera`). Everything else derived from "the camera" was one frame-global picked by the active tab: streaming foci, stream view bias, particle update camera, audio listener, and the projection aspect. Both viewport targets were window-sized, so a fixed play resolution letterboxed a window image instead of rendering at that size.
- Resolution: `RenderView` (camera, matrices, target pixel size, aspect, stream focus) is built per frame and is the single source for streaming, view bias, particles, world passes, and target sizing. Streaming unions visible view foci with the play avatars. The renderer separates the offscreen chain size (`render_width_`/`render_height_`, `set_render_resolution`) from the window/backbuffer, so Game renders at the authored play resolution. Only the visible view is drawn per frame.
- Verification: Rebuild `engine`; play-test ? Scene free-cam loads world under the lens; Game at 1920?1080 is a real target; captures still read the game RT.
- Remaining risk: changing play resolution or switching tabs recreates the offscreen chain (one `wait_for_gpu` stall). Panel-splitter drags do not, because Scene renders at window size.

## 2026-08-02 ? Game tab drew editor overlays; Scene stream stuck on player

- Reproduction: F5 play-test with maximized/1080p Game view; selection shows yellow AABB / event zones over the Game image. Scene tab while play runs: freecam far from the player sees empty land / little foliage because stream foci only followed the orbit pivot.
- Impact: Game view polluted with edit chrome; Scene during play felt wireframe-empty.
- Cause: Selection/collider/event-zone ImGui overlays ran for any 3D viewport (including Game). Terrain/foliage/water stream used player foci only during `test_session_active`.
- Resolution: Gate viewport overlays to Scene/Sculpt. While play runs and Scene/Sculpt is selected, stream freecam as an additional focus (first, so foliage follows Scene). Water + view-bias follow freecam on those tabs. Game billboards use the play orbit VP.
- Verification: Rebuild `engine`; F5 ? Game without selection boxes; Scene freecam away from the player loads terrain/foliage.

## 2026-08-02 ? Scene during play looked broken (box player + frozen free-cam)

- Reproduction: F5, switch to Scene while testing. Free-cam felt wrong (stretched); player often read as a solid unit cube, not the skinned mesh; gizmo/select/place disabled so world could not be examined while play ran.
- Impact: Live inspection during play-test was unusable.
- Cause: (1) `draw_physics_body` drew the unit box into the Scene RT whenever a test session was active. (2) Free-cam `set_perspective` only ran when no orbit camera existed, so play left Scene on a stale projection matrix. (3) `edit_mode = !test_session_active` blocked all Scene tools. (4) Maximized play chrome stayed collapsed on Scene, hiding Inspector.
- Resolution: never draw the editor unit body box; always update free-cam aspect from Scene viewport rect; allow Scene edit tools + shortcuts during play; collapse max chrome only while Game is active; keep Sculpt/UI/World Forge gated.
- Verification: rebuild `engine`; F5 ? Scene ? skinned player, free-cam (RMB), select/gizmo; inspector panels visible when not maximized Game.
- Remaining risk: editing the live player spawn transform fights visual-follow each frame ? inspect/edit other props or settings instead.

## 2026-08-02 ? Scene view janky after play-test; no fixed-res play surface

- Reproduction: F5 Game play-test, walk/push dynamic props; End Test (Shift+F5); return to Scene. Spawn may restore but other Rigidbody placements stay where physics left them; editor often remains on the Game tab with a grey ?Start a test session? plate; relative-mouse / orbit leftovers make free-cam feel broken; Game tab only fits the panel so UI cannot be checked at 1080p fullscreen.
- Impact: Scene editing after play was unreliable (moved objects, wrong tab/camera presentation); HUD layouts could not be reviewed at design resolution.
- Cause: restore only snapped player spawn + edit camera; Game tab was not left; only the panel-sized viewport existed for play.
- Resolution: snapshot every entity transform at Start and restore on End (bump collision revision); End forces Scene tab + layout restore, clears look/fullscreen; Play display modes Embedded / Maximized / Fullscreen with letterboxed resolution presets (Fit, 720p?1080p, HUD design, Custom).
- Verification: rebuild `engine`; Start Maximized 1080p ? play ? End ? Scene free-cam, gizmos, and pre-play placement poses.
- Remaining risk: Fullscreen uses the host editor window (not a second OS process); fixed-res letterboxes the Game RT rather than allocating a dedicated 1080p swap target.

## 2026-07-31 ? Held weapon drifted off the hand; grip gizmo snapped back on release

- Reproduction: Play-test with the Ashfell sword on hotbar 0. The sword floated at roughly double character scale near the hip instead of in the hand, and it kept the hand's animated pose only loosely. In Inspector ? Held Weapon Attach, dragging the rotate gizmo moved the weapon, but the weapon jumped to a different orientation the moment the mouse released. Pausing with F6 dropped the character to bind pose while the weapon stayed at the last animated hand position.
- Impact: Hand attach was not authorable ? every gizmo edit landed somewhere other than where it was dragged, and the saved `handAttach` did not match what had been on screen.
- Cause: four independent defects. (1) `transform_from_column_major` rebuilt the matrix with `XMMatrixSet` in transposed order, so `XMMatrixDecompose` read translation out of the bottom row of a matrix whose translation was in the right column ? joint welds lost all translation and got the inverse rotation. (2) The socket chain was `entity world ? joint global` and skipped the skinned mesh's prefab part transform, so the player prefab's `0.655` scale on `Body` never reached the weapon. (3) The gizmo write-back went through `ImGuizmo::DecomposeMatrixToComponents`, which returns Euler in X?Y?Z, and fed those angles to `XMQuaternionRotationRollPitchYaw`, which composes Z?X?Y; any rotation with two non-zero components came back wrong. (4) The skinning pass was gated on `simulating`, so pausing skipped it and `begin_bone_slot_frame` cleared the palette to identity.
- Resolution: new `engine::BoneWeld` / `BoneSocketChain` module (`include/engine/animation/bone_attachment.h`, `src/animation/bone_attachment.cpp`) owns weld math as a Motor6D-style C0: `bone_socket_world` composes owner ? visual part ? joint, `weld_world_transform` applies the weld, and `weld_from_world_transform` is its exact inverse for manipulator write-back. `transform_from_column_major` now uses `XMLoadFloat4x4` directly. Gizmo results decompose straight to a quaternion (`transform_from_gizmo_matrix`) instead of round-tripping Euler, which also fixed the entity gizmo. The skinning pass runs whenever a test session is active, paused or not. The socket is captured once at drag start so an animating joint cannot pull the handles away mid-drag.
- Verification: `animator` suite covers the Euler round-trip (including gimbal lock), socket composition with a scaled character, the weld solve round-trip, and `transform_from_column_major` translation. In-editor, the sword sits in the right hand at character scale and tracks the idle clip across frames.
- Remaining risk: `weld_from_world_transform` assumes a uniformly scaled socket; a non-uniform joint scale would bake shear into the solved weld. The joint dropdown lists the live skin's joints only while a test session is active ? with no session it falls back to the authored string.

## 2026-07-31 ? Attack looked left-handed; NPC poses shared player palette

- Reproduction: Play-test Attack with Ashfell sword; swing read as screen-left arm vs Blockbench right. Placed `npc_test` / second `player.gltf` mirrored player Run. Dropping the ? yaw offset made the player face the camera while moving (backwards art).
- Impact: Combat read mirrored; all skinned scene copies shared one pose.
- Cause: (1) The Blockbench glTF is right-handed (front ?Z, `Right*` joints at +X) and is imported verbatim into the left-handed runtime, so the drawn character is the mirror of the authored one: the joint named `Right*` is the visual **left** limb, and every clip reads mirrored. (2) Play-test only attached the spawn animator and uploaded one bone palette for every skinned draw. (3) `npc_test` animator lived on the prefab until `propagate_prefab_components` ran.
- Resolution: Keep yaw ? for facing (mesh ?Z front vs loco +Z). Mirror the **clip sample** instead of the geometry: `sample_clip_pose_for_joint` reads the sagittal counterpart channel (`RightUpperArm` <-> `LeftUpperArm`) and reflects the local pose through the YZ plane (`t.x ? ?t.x`, `q ? (x, ?y, ?z, w)`), seeding from the reflected rest so unkeyed channels round-trip to their own rest. F5 propagates prefab components then attaches every entity with an `animator`; multi-slot bone CB keyed by entity id.
- Two mirrors that do **not** work: negative entity `scale.x` (`XMMatrixDecompose` in prefab expand drops mirrored scales, and a mirrored model matrix inverts winding against the back-face-culling prop pipeline), and ReflectX conjugation of the palette (`S M S`) ? that skins the *mirrored* bind vertex with unmirrored joint assignments and tears the rig apart.
- Verification: Rebuild `engine`; from behind, Attack winds up and swings on the screen-right arm (`out/article-captures/play_20260731-130856.mp4`) with the rig intact; Run keeps opposed arm/leg swing facing the movement direction (`play_20260731-131052.mp4`); NPC Idle stays while player Runs.
- Remaining risk: sagittal reflection assumes identity bind rotations and world-aligned joint axes ? true for this Blockbench rig, wrong for a rig with rotated bind frames. Root motion in `AnimatorRuntime` is not mirrored, so X-axis root translation (DodgeLeft/Right) would travel opposite the visual once root motion is wired. >15 unique skinned entities/frame fall back to identity slot 0; co-op guest still lacks its own animator wiring.
- **Path forward when this recurs:** Mirror at the clip sampler, not the transform or the palette. The permanent fix is a real RH?LH conversion at import (negate X on positions/normals/bind translations/IBMs plus flipped winding), which would let the sampler mirror be deleted. Do not remove yaw ? without a play-test walk check; ensure `upload_entity_bone_slot` + `skin_entity_id` on expand ? not a single `pending_skin_matrices` path.

## 2026-07-31 ? Prop instancing drew skinned player as T-pose (shadows still moved)

- Reproduction: Play-test Idle/Run; character mesh T-posed while the cast shadow silhouette animated.
- Impact: Locomotion looked broken even when CPU skin + animator succeeded.
- Cause: Hardware-instanced prop VS ignored `BLENDWEIGHT`/`BLENDINDICES` and had no Bones CBV; shadow VS skinned correctly from the same palette.
- Resolution: Prop root adds Bones CBV (b2, root param 6); prop VS LBS before model (zero-weight early-out for static props); `draw_prop_instances` binds the palette.
- Verification: Rebuild `engine`; F5 ? Idle arms off T-pose; shadow matches mesh.
- Remaining risk: Mitigated by multi-slot bone CB + per-entity attach (see entry above).
- **Path forward when this recurs:** Check prop VS for LBS + `bind_bone_constants(6)` on the prop root; compare shadow vs lit silhouette.

- Reproduction: After GoodPlayerModel asset-bake, play-test showed missing palm/digit faces and often T-pose despite clips present in `player.bake.json`.
- Impact: Hands look hollow; locomotion appears broken until editor restart.
- Cause: (1) Hand/digit cubes majority-inward winding ? D3D backface cull. (2) Mesh hot-reload updated skin while `AnimationClipLibrary` kept pre-bake clips (`reload_changed` never polled).
- Resolution: Player baker flips inward hand tris + normals; gates `ASSET-BAKE-WINDING-HAND` / `ASSET-BAKE-CLIP-LOCO-THIN`; editor reloads clip sources when mesh keys reload.
- Verification: Bake `hand_tris=288 outward=284 inward=4`; MSBuild `engine` ok; verify tests ok.
- Remaining risk: Non-hand inward prims elsewhere still unflipped; Jump/Attack still unwired in animator.
- **Path forward when this recurs:** [`recurring-asset-failures.md`](recurring-asset-failures.md) ? Player ? missing hand / finger faces; ? Player ? T-pose after Assets bake.

## 2026-07-30 ? Foliage bake full-inpaint muddied `bush_tall`

- Reproduction: Tall bush in sandbox / open-world looked glitchy (camo streaks / speckles) after foliage atlas rebakes.
- Impact: Scene dressing and GPU `bush_tall` foliage layer read as broken paint.
- Cause: `bake_tier1_props_gltf.py` foliage path cleared non-olive Blockbench chrome then **inpainted the entire atlas** from nearest opaque, collapsing ~18 source shades into ~6 muddy fills with edge streaks. Source `Tall_Bush.png` is also a sparse Blockbench swatch atlas (faces/neon islands) ? bake cannot invent leaf detail.
- Resolution: `bush_tall` PROPS ? soft clean (`foliage_strict_colors: false`) + small `foliage_inpaint_radius` (2); generator `v6-foliage-soft-inpaint`. Named rebake only.
- Verification: Rebake `bush_tall`; atlas opaque unique colors ~16 (was ~6 after full inpaint); PNG write may need `engine.exe` killed if file locked.
- Remaining risk: Source atlas still needs a foliage-only Blockbench retexture for real leaf read; soft clean can leave neon chrome if UV hits those islands.
- **Path forward when this recurs:** [`recurring-asset-failures.md`](recurring-asset-failures.md) ? Foliage ? glitchy / muddy `bush_tall`.


## 2026-07-30 ? Play-test `ANIM-CLIP-MISSING: Clip 'Run'` on player.gltf

- Reproduction: Game play-test; console `ANIM-CLIP-MISSING: Clip 'Run' not found in assets/models/player.gltf` (Fall similarly). Idle may still resolve.
- Impact: Locomotion transitions fail closed; Run/Fall never play.
- Cause: Baked `player.gltf` had only Idle/HandGrip while `player.animator.json` expects Idle/Run/Fall. Clips exist on `Player_V2_rigged.gltf` but were dropped or never re-baked into the sample mesh.
- Resolution: `python tools/bake_player_v2_gltf.py` from the rigged V2 source; confirm animation names include Run + Fall.
- Verification: `player.gltf` animations `['Idle', 'HandGrip', 'Run', 'Fall']`; play-test Idle?Run?Fall after mesh reload.
- Remaining risk: Future clip-stripped exports overwrite the sample again; atlas clean can still reduce `player.png` color count ? visual check after bake.
- **Path forward when this recurs:** [`recurring-asset-failures.md`](recurring-asset-failures.md) ? Player ? `ANIM-CLIP-MISSING`.


## 2026-07-28 ? Per-entity skinning T-posed everyone (bone CB clobber)

- Reproduction: After per-entity NPC animator work; play-test sandbox with player + npc_test ? both stuck in T-pose despite ~1 ms CPU skin.
- Impact: Player and NPC locomotion/Idle invisible.
- Cause: Single UPLOAD bone CB rewritten between DrawInstanced calls (identity for props, then other entities). GPU still reading prior draws saw the last write.
- Resolution: 16 aligned bone-palette slots per frame ring; bind CBV at slot offset; reuse slot per entity across shadow cascades; non-skinned draws bind identity slot 0 without rewriting skinned slots.
- Verification: MCP screenshot before (T-pose); rebuild; play-test Idle/Run on player and Idle on NPC.
- Remaining risk: >15 unique skinned entities in one frame reuse last slot (fail-closed).



## 2026-07-28 ? NPC copies of player.gltf mirrored play-test locomotion

- Reproduction: Place pc_test\ (or any prefab using \player.gltf\); start Game play-test; walk/jump ? NPCs deform with the same Idle/Run/Fall as the player.
- Impact: Input-driven animation appeared shared across all skinned stand-ins.
- Cause: GPU LBS used one bone palette per frame for every instance with blend weights.
- Resolution: Per-entity animator attach on play-test; pose cache keyed by entity id; prop/shadow draws upload that entity's matrices. pc_test\ ships with \player.animator.json\ (default Idle). Player locomotion params still drive only the spawn entity.
- Verification: Rebuild \engine\; play-test with npc_test ? player Runs on WASD; NPCs stay on Idle independently.
- Remaining risk: Many unique poses = more bone CB uploads per frame; co-op guest still lacks its own animator wiring.

## 2026-07-28 ? Play-test player stuck in T-pose after prop GPU batching

- Reproduction: Game play-test with Player_V2 / `player.gltf`; Idle/Run animator attached; character remains bind/T-pose.
- Impact: Locomotion clips never visible in play-test despite CPU skin matrices (~0.5?1 ms) and animator status succeeding.
- Cause: Hardware-instanced prop pipeline drew all placed meshes (including the skinned player). Prop VS eventually declared a Bones CB, but the prop root signature had no bone CBV slot and `draw_prop_instances` never bound the palette ? so GPU LBS never ran on the path that actually draws the player.
- Resolution: Add Bones CBV (register b2) as prop root parameter 6; bind `bind_bone_constants(6)` for prop draws; prop VS LBS early-outs on zero blend weights so static props stay unchanged.
- Verification: Rebuild `engine`; reload MCP; start play-test ? Idle arms should leave T-pose; Run when moving. Console one-shot `Skin pose applied` with non-zero `armOffIdentity`.
- Remaining risk: Mitigated for NPCs by per-instance skinEnable (see entry above); guest co-op and catalog-wide NPC animators remain follow-on.

## 2026-07-27 ? MCP GPU screenshots swapped R/B (fire looked blue)

- Reproduction: Live editor shows orange/yellow campfire particles; `engine_editor_screenshot` PNGs showed blue discs and an orange sky (R?B of blue sky + orange fire).
- Impact: Agents misread particle/lighting color from MCP evidence.
- Cause: `capture_presented_backbuffer_png` copied swap-chain bytes as RGBA; D3D12 RT readback is BGRA (same as the existing frame-capture / PPM path, which already swapped).
- Resolution: BGRA?RGBA in the MCP GPU screenshot readback (match CLI capture).
- Verification: Rebuild editor, MCP screenshot of campfire ? fire channels should be warm (R>B), sky not inverted.
- Remaining risk: None for R8G8B8A8_UNORM swap-chain; revisit if format changes.

## 2026-07-26 ? Remaining `foliage_sync` dips were ground-cover rejection storms

- Reproduction: sandbox play-test walk through dense meadow; dips showed `foliage_sync` ~7?21 ms after height-grid sampling landed.
- Impact: Soft look/walk cliffs after water per-cell upload fix.
- Cause: Ground-cover path still used rejection sampling (`target * 16` attempts with matrix builds) on dense painted cells.
- Resolution: Texel-driven ground-cover spawn with proportional budget scaling (covers the cell under `k_max_instances_per_cell`); empty-density early-out; defer water mesh/upload on frames that already paid terrain GPU or foliage scatter.
- Verification: `foliage` + `water` suites; sandbox MCP look/walk (`perfDipFoliageMs` should stay small).
- Remaining risk: Full 33?33 height sample still runs for painted cells; async foliage would hide residual cost.

## 2026-07-26 ? Play-test `water_stream` dips were full-buffer water GPU re-uploads

- Reproduction: sandbox/vertical-slice play-test look/walk; dips labeled `water_stream` with `waterMs` ~30?50 while `StreamedWaterField` mesh build alone is ~0.3 ms/cell in suite timing.
- Impact: Walk/look still hitching after mesh-build amortization; foliage dips already quiet.
- Cause: Main loop concatenated every resident water cell and re-created one monolithic upload buffer whenever streaming dirtied water ? same cost class as pre-per-cell terrain.
- Resolution: Per-cell water CPU dirty tracking (`take_render_dirty_cells` / removed) + per-cell GPU buffers (`upload_water_cell` / draw loop), capped to 1 upload/frame. Height-grid mesh build + empty-wet early-out remain. Foliage scatter uses a per-cell height grid (no more ~45 ms `foliage_sync` storms).
- Verification: `water` + `foliage` suites; rebuild + sandbox MCP look/walk (`perfDipWaterMs` should stay small / not dominate).
- Remaining risk: Terrain-height revision still rebuilds all resident water CPU meshes synchronously (uploads still amortized); eastern sea cells still allocate large per-cell buffers.

## 2026-07-26 ? Play-test `simulation_other` dips were synchronous water mesh rebuilds

- Reproduction: play-test walk near sea/shore in `samples/open-world-rpg` (or sandbox with water). MCP dips showed ~4 FPS / ~230 ms wall with dominant `simulation_other`, while terrain/foliage/collision/physics were ~0.
- Impact: Look/walk hitch attribution looked like an unknown Simulation hole; crossing water stream cells spiked frames hard.
- Cause: `StreamedWaterField::update` rebuilt every newly wanted cell mesh in one frame (`build_cell_mesh` does dense terrain height sampling), then the main loop immediately concatenated + uploaded the full resident water buffer ? all untimed inside Simulation.
- Resolution: Cap new water cell mesh builds to 1/update (catch-up while `stream_pending`), expose `water_ms` / `water_stream` in Performance dips + MCP; superseded full-buffer upload by per-cell GPU path (see entry above).
- Verification: `water` suite amortized catch-up case; rebuild + MCP look/walk retest (`perfDipWaterMs` / dominant should not be a silent `simulation_other` cliff).
- Remaining risk: See per-cell water upload finding above.

## 2026-07-26 ? Dip wall time used previous frame delta (misattributed dominant phase)

- Reproduction: play-test look/walk in `samples/open-world-rpg`; MCP `perfDipsJson` showed `wallMs` 200?300 with phase totals ~20 ms and a nonsense dominant (often `simulation_other`).
- Impact: hitch attribution lied; play-start cliffs and foliage prep spikes were hard to confirm from dip logs.
- Cause: instant FPS / dip `wallMs` used `raw_frame_delta` (time since previous frame start) while phase timers measured the current frame. Present/GPU wait from the prior frame inflated wall without matching phases.
- Resolution: dip/instant wall is elapsed from `cpu_profile_started` to frame end; smoothed FPS still uses inter-frame delta. Dips also record `gpuWaitMs` / `foliageUploadMs`. Placement collision dynamics flips budget rebuilds (priority spawn first). Foliage instance packing runs asynchronously; upload applies when ready.
- Verification: `collision` suite (budgeted sync + priority). Rebuild + MCP look/walk retest for aligned `wallMs` vs phases.
- Remaining risk: async foliage can show one stream-settling frame of stale blades; content revision prevents clearing dirty on stale uploads.

## 2026-07-26 ? Play-test player visual follow re-scanned placement collision every frame

- Reproduction: start play-test in `samples/open-world-rpg`, look/walk while watching Diagnostics ? Performance. `Placement collision` sat at ~5?10 ms every frame with terrain/foliage/physics near zero; wall frame time spiked into the high teens / low 20s.
- Impact: Simulation dominated remaining look/walk hitch after the static-cache and terrain amortizing fixes.
- Cause: play-test copies locomotion/character feet into the spawn entity via `Scene::set_transform` with the default `bump_edit_revision=true`. That invalidated `PlacementCollisionTracker`'s revision early-out every frame, so sync rescanned every world placement even though physics bodies were unchanged.
- Resolution: play-test visual follow uses `set_transform(..., bump_edit_revision=false)` (same contract as physics write-back). Editor/gizmo/MCP transforms still bump so collision stays correct on authored edits.
- Verification: `collision` suite 1847/1847 (includes visual-follow revision stability + O(1) sync skip). Live look/walk samples: `perfCollisionMs` max ? 0.003 ms (was ~5?10 ms); wall avg ? 10.2 ms / max ? 16 ms (was avg ? 18.5 / max ? 34).
- Remaining risk: any other gameplay path that updates placement transforms every frame with the default bump will reintroduce this; prefer `bump=false` when collision identity is unchanged.

## 2026-07-26 ? game-look visual regression: orbit soft-follow buried the probe under slopes

- Reproduction: `engine visual-regression --project samples/open-world-rpg` after the look-jitter soft-follow / collision-smoothing change. `game-look` (180/40 look at frame 20) failed threshold with the camera framing the underside of a hillside.
- Impact: Capture / QA gate red; play-test look into slopes could leave the eye inside terrain for frames.
- Cause: Soft-follow lagged feet Y when physics pushed the capsule up a slope, so the collision sweep originated inside the heightfield. Separately, soft pull-in after an instant look into geometry left `resolved_distance` inside the blocker for several frames.
- Resolution: Soft-follow XZ at rate 22, Y at 80; snap the whole pivot on jumps >2 m; snap collision pull-in / shoulder when the new target is clearly shorter (`< 0.85 ? current`).
- Verification: `camera` suite 85/85; visual-regression pass (worstMeanAbsRgb ? 8.6 < 12).

## 2026-07-26 ? Static prop cache rebuilt every frame (Inspector re-seeded component-less prefabs)

- Reproduction: open the editor, select any entity whose prefab authors no collision/script/animator/rigidbody/audio, and watch Diagnostics ? Performance. `Cache rebuild` holds near its full cost (~12 ms here) instead of decaying to 0, and `sceneDirty` never clears.
- Impact: the ~12 ms static prop expansion ran on nearly every frame instead of only on scene edits, dominating Render preparation and holding the editor near 33 FPS. Also produced a permanently dirty scene, so "unsaved changes" was always true.
- Cause: `Scene::ensure_authored_components_seeded` skipped work only when the entity already had a **non-empty** `AuthoredComponentsComponent`. Seeding a prefab with nothing to seed leaves an **empty** component, so the guard stayed false forever. The Inspector polls this every frame for the selected entity, so each frame re-seeded, called `bump_edit_revision()`, and re-dirtied the static render cache.
- Resolution: treat "component present but prefab has nothing to seed" as unchanged. A prefab that later gains components still seeds onto the existing empty set, so the original intent is preserved.
- Verification: new `world` suite case asserts repeated seeding of a component-less prefab reports `false` and leaves `edit_revision()` untouched, and that a prefab which gains a collider still seeds. All 21 named suites pass.
- Remaining risk: any other per-frame caller of a `Result<bool>`-style "ensure" helper can reintroduce this. Prefer asserting revision stability in tests for helpers the editor polls each frame.

## 2026-07-26 ? Static prop expansion ran inline on the render thread

- Reproduction: place/move an object or start a play-test in `samples/open-world-rpg`; the frame that rebuilds `static_render_instances` spikes.
- Impact: prefab expansion is O(placements ? parts) (~10k instances here) and landed entirely inside one frame.
- Resolution: the rebuild now snapshots its inputs (prefab catalog, mesh bounds, material cache, placements) and expands on a worker via `build_static_render_cache`; props keep drawing from the previous cache until the job lands. A live gizmo drag still expands inline because it must see its own transform in the same frame. Diagnostics ? Performance separates main-thread `Cache rebuild` from `Prop expand (worker)` and shows a rebuild count so a re-dirty loop is visible immediately.
- Verification: `engine benchmark --project samples/open-world-rpg` on the identical harness went from 23.85 ms / 41.9 FPS to 13.33 ms / 75.0 FPS average CPU. `engine visual-regression` `game-default` renders terrain, character, and the campfire prop as before.
- Remaining risk: an edit landing while a job is in flight is picked up by a follow-up pass, so the cache can trail authored state by a frame or two outside gizmo drags.







## 2026-07-26 ? Orbit look-around jitter from collision distance pop + pivot micro-shake







- Reproduction: Game play-test; right-drag look in place near trees/terrain. Camera distance and framing stutter even when Simulation ms is healthy.



- Impact: Look feel remains jittery after streaming/placement CPU fixes; easy to blame mouse input or frame timing.



- Cause: `OrbitCamera::update` snapped `resolved_distance` to the raw sphere-sweep hit each frame, so grazing static geometry made distance oscillate. Rigidbody feet Y also fed the pivot unfiltered. On the deferred Rigidbody update path, `apply_look` changed yaw/pitch without refreshing the eye until the late collision update.



- Resolution: Smooth collision distance / shoulder scale (fast pull-in, slow recover) with frame dt; soft-follow the pivot; `apply_look` recomputes the eye from the current resolved pose immediately.



- Verification: `camera` suite 74/74 (ease-out recovery assert); rebuilt `engine`; editor relaunched; build lease released.



- Remaining risk: Very tight interiors may still pull in aggressively (by design). Extreme hitch frames clamp smoothing dt via a 1/60s fallback.







## 2026-07-26 ? Placement collision resynced the whole scene every play-test frame







- Reproduction: Start a Game play-test on a dense sandbox world (hundreds of forest placements). Diagnostics ? Performance shows Simulation ~10?12 ms even while standing still or walking over already-loaded terrain.



- Impact: Movement feels hitchy/low-FPS in Debug editor play-test because the CPU budget is dominated by simulation work unrelated to locomotion.



- Cause: `PlacementCollisionTracker::sync` walked every entity each frame, re-resolving prefab paths and rebuilding effective collision volume lists. `write_back_transforms` then linear-scanned `entity_ids()` per physics body and called `set_transform`, which would have invalidated any revision-based skip.



- Resolution: Scene gains `edit_revision` bumped on authoring mutations; physics write-back uses `set_transform(..., bump_edit_revision=false)`. Sync skips entirely when revision and `simulate_dynamics` are unchanged. Write-back stores/uses `EntityId` directly. `entity_ids()` reads `IdComponent` instead of re-parsing UUID strings.



- Verification: Rebuilt `engine`; collision suite 1514/1514 including write-back revision + dense-forest skip timing (`<50 ms` for 200 no-op syncs over 240 static placements). Live play-test after editor reset: Simulation dropped from ~10?12 ms / ~43 FPS to **~6.3 ms Simulation / ~66 FPS / ~15 ms wall** (Diagnostics capture `out/jitter-fix-after-sync-skip-*.png`). Build lease released.



- Remaining risk: First sync after a large edit still pays full cost; sustained Simulation time will still include Jolt step and terrain streaming commits. Editor UI still ~5?6 ms in Debug.







## 2026-07-26 ? Foliage full rebuild + upload ran once per streamed terrain cell







- Reproduction: Walk across a terrain-cell boundary with resident painted foliage; every committed streaming cell caused a frame spike even after terrain commits were budgeted to one per frame.



- Impact: Crossing a boundary loads several cells over consecutive frames, so movement stuttered for the whole streaming burst instead of one frame.



- Cause: Two per-commit full-cost paths. `StreamedFoliageField` rebuilt its merged batch map (copying every resident instance) on each changed cell, even though the renderer consumes `cell_instances()` directly and the batch map is only read by diagnostics/tests. The runtime also re-uploaded the full foliage instance buffer (`set_foliage_cell_instances`) on every dirty frame. A secondary waste: the Rigidbody play path ran `OrbitCamera::update` (with its `sweep_sphere`) twice per frame; only the post-physics call feeds the rendered game pass.



- Resolution: Batch map is now rebuilt lazily on `batches()` access, so streaming commits skip the full instance copy. The instance-buffer upload is debounced while `stream_pending()` is true (bounded at 8 frames so long walks still refresh), collapsing a streaming burst into one upload. The pre-physics orbit-camera update is skipped when the authoritative post-physics follow update will run.



- Verification: Rebuilt `engine` and `engine_suite_tests`; foliage suite (46/46) adds regression checks that sync defers the batch merge and that access caches it; terrain suite passes (237/237). Live editor play-test not run: the editor MCP server was disconnected and could not be respawned from the agent session.



- Remaining risk: Foliage scatter for a newly committed cell is still synchronous on the main thread; a very dense single cell can still cost a visible slice. Debounced uploads delay new foliage visibility by up to 8 frames during streaming.







## 2026-07-26 ? Async terrain completions could still bunch into one movement hitch







- Reproduction: Walk while several queued terrain-generation workers finish in the same frame; the player hit a visible stutter despite the generation itself running off the render thread.



- Impact: Open-world movement could hitch at or shortly after terrain-cell transitions, especially when a support fringe and outer-ring jobs completed together.



- Cause: `StreamedTerrainField::commit_ready_generations` committed every ready future in one update. Each commit still performs main-thread heightfield creation, terrain render conversion/upload, foliage scatter, and foliage-buffer rebuild, bypassing the existing job-scheduling budgets.



- Resolution: Add `TerrainStreamParams::max_ready_commits`; the runtime sets it to one completed terrain cell per frame. Pending completed jobs remain queued for later frames, preserving collision support while smoothing work across frames.



- Verification: Rebuilt `engine`; terrain suite adds a one-ready-commit-per-update regression assertion. The focused suite build succeeds, but its runtime process did not terminate on this workstation and was stopped after a bounded wait.



- Remaining risk: A single cell can still be expensive because foliage scatter and full foliage-buffer upload remain synchronous; further work should move/scatter or upload foliage incrementally.







## 2026-07-24 ? Grass striped in straight lines + wrong olive color







- Reproduction: Dense grass paint under sandbox forest shows bright olive/yellow clumps locked into straight horizontal bands instead of a natural dark meadow carpet.



- Impact: Ground cover reads as broken brush artifacts; color mismatches stylized terrain greens.



- Cause: (1) Sample grass layer used `grass_clump` with `densityMultiplier: 0.9` and bright olive color ? docs/tests expect `grass_blade`. (2) `scatter_foliage_cell` walked the density grid row-major and `return`ed at `k_max_instances_per_cell` (2048), striping the first z-rows. (3) Even after thinning, spawning from density-sample centers (1.25 m lattice) still read as straight grass lines.



- Resolution: Ground-cover layers rejection-sample random XZ points inside the cell against painted density (discrete bushes stay sample-anchored). Restore brighter `grass_blade` authoring.



- Verification: Rebuild `engine`; `engine_suite_tests --suite foliage`; sandbox Scene grass should look scattered meadow green, not a grid.



- Remaining risk: Rejection sampling can under-fill very sparse paint if max attempts are exhausted; raise attempts or brush strength if meadows look thin.







## 2026-07-24 ? Foliage batches sampled the wrong instance rows (invisible grass)







- Reproduction: Paint dense grass under trees; Diagnostics shows tens of thousands of instances, but Scene/Sculpt shows little or no grass carpet (sparse wrong-place clumps at best).



- Impact: Ground-cover foliage looked broken / missing despite valid density paint and scatter.



- Cause: `draw_foliage_instances` passed `draw.instance_offset` as D3D12 `StartInstanceLocation`, while the foliage VS indexed `StructuredBuffer` with `SV_InstanceID * 4`. `SV_InstanceID` is 0-based per draw and ignores `StartInstanceLocation`, so every mesh batch (except the first) read transforms from the start of the instance buffer.



- Resolution: Pass instance base in root constants (`layerBladeTime.z`) and draw with `StartInstanceLocation = 0`; lift scattered instances slightly to reduce terrain z-fight.



- Verification: Rebuild `engine`; sand?box Scene should show grass clumps under forest stands matching density paint.



- Remaining risk: Discrete bush batches were also mis-indexed when not first in the buffer ? same fix. Reintroduce frustum cull only with tests that cover multi-batch instance bases.







## 2026-07-24 ? Foliage tool bugs (ghost forest / vanish / broken undo)







- Reproduction: (1) Sandbox loads a dense green carpet not listed in Scene Hierarchy. (2) Play or some mesh-sync interactions can blank foliage. (3) Sculpt foliage Undo does not restore pre-stroke density.



- Impact: Density paint looks like unselectable ?ghost? scene content; play-test/select paths can clear the carpet; Undo cannot recover paint mistakes.



- Cause: (1) Expected split ? `foliage-density.json` GPU instances are not scene entities (Hierarchy only lists prefabs). Sandbox has ~38 painted cells / bush+grass layers that look like a mini forest. (2) TICKET-0220 cell AABB frustum cull false-rejected draws under Game orbit (disabled). (3) Scatter `distance_falloff` used 3D distance including camera Y, so high Scene/Sculpt orbits baked near-zero counts into newly streamed cells. (4) Editor foliage brush snapshotted cells *after* paint for Undo. (5) `sync_imported_meshes` rebuilt `mesh_ranges_` without rebinding foliage draws; empty foliage upload `Reset()` the instance buffer while the GPU could still sample it.



- Resolution: Keep foliage uncullled for now; horizontal-only scatter falloff; snapshot brush footprint before mutate; re-ensure foliage meshes + re-upload instances after prefab mesh sync; fence-retire empty instance buffers.



- Verification: Rebuild `engine`; `engine_suite_tests --suite foliage` (includes high-camera falloff assert); Scene Sculpt paint ? Undo restores prior density; Play near spawn keeps resident carpet.



- Remaining risk: Terrain/paint/water brushes still snapshot after mutate (same Undo class). Hierarchy still omits density instances ? needs a Foliage summary UX if authors keep confusing them with prefabs. Safe frustum cull needs instance-derived AABBs + orbit VP tests.







## 2026-07-24 ? Play-test foliage forest stayed gone (frustum cull)







- Reproduction: Load sandbox; dense bush carpet visible in Scene (not in Hierarchy). Start Test ? carpet vanishes and stays gone while looking around. Prefab oaks remain.



- Impact: Sandbox play-test looked stripped of authored ground cover.



- Cause: Foliage batch frustum cull (TICKET-0220) rejected resident bush draws under the Game orbit camera; Hierarchy omission is expected (density instances, not entities). Play-start resscatter also risked thinning via distance falloff.



- Resolution: Stop frustum-culling foliage draws (streaming + 120?220 m scatter falloff already bound cost); remove play-start foliage rebuild. Centered cell AABB kept for future use.



- Verification: Rebuild `engine`; Scene ? Play ? bush carpet near spawn/river should remain visible in Game.



- Remaining risk: Slightly higher foliage draw cost vs cull-on; reintroduce cull only with orbit-VP unit tests + instance-derived AABBs.







## 2026-07-24 ? Foliage vanished in Game play-test (false frustum cull)







- Reproduction: Load sandbox; grass/flowers visible in Scene viewport but not listed in Scene Hierarchy; Start Test / Game tab ? foliage near spawn disappears.



- Impact: Play-test ground cover looked empty; easy to blame streaming or Hierarchy.



- Cause: (1) Hierarchy only lists scene entities ? density-painted foliage is GPU-instanced, not entities (expected). (2) TICKET-0220 foliage cull AABB used **corner** cell origin (`cell*40`) while scatter/terrain cells are **centered** (`cell*40 - 20`), so the spawn cell `(1,0)` was false-culled under the tighter Game orbit camera.



- Resolution: Center foliage draw bounds in `Renderer::set_foliage_cell_instances` to match `foliage_scatter.cpp`.



- Verification: Rebuild `engine`; foliage suite cell-origin assert; Scene ? Play near spawn should keep grass instances.



- Remaining risk: Distant painted cells still unload when stream focus jumps to spawn (expected streaming).







## 2026-07-24 ? Skin matrices timer re-imported glTF every frame







- Reproduction: Game play-test after GPU LBS; Diagnostics ?Skin matrices? ~19 ms with GPU frame ~1.5 ms.



- Impact: Play-test FPS dropped back to ~35 despite GPU skinning; looked like skinning was still CPU-bound.



- Cause: `sample_skinned_local_poses` ? `find_clip` called `AnimationClipLibrary::load` every frame, which re-parses the clip glTF.



- Resolution: Prefer `get()` (cached) and only `load()` on miss.



- Verification: Rebuild `engine`; Game play-test Skin matrices should be sub-ms; Idle still deforms.



- Remaining risk: First frame still pays import cost; hot-reload still uses `reload()`.







## 2026-07-24 ? Present drain removed via 2-slot upload CB ring







- Reproduction: Steady Game play-test; Diagnostics showed large GPU-fence wait after Present even when GPU timestamps were ~1?3 ms.



- Impact: Wall-clock FPS stayed CPU-serialized; CPU could not overlap prep with the prior GPU frame.



- Cause: Permanently mapped single-slot frame/water/shadow/SSAO/composite upload CBs forced `wait_for_fence` after every Present.



- Resolution: Ring those CBs (plus bone palette) with `frame_count = 2` keyed to swapchain `frame_index_`; drop the successful-Present drain ([DEC-0047](../decisions/index.md#dec-0047-frame-upload-ring-and-gpu-lbs-skinning), TICKET-0226). Allocator reuse still waits at frame start.



- Verification: Rebuild `engine`; Diagnostics fence wait ? 0 in steady play; multi-minute walk without green screen.



- Remaining risk: In-place foliage instance Map on dirty cells is still single-buffered (retire-on-grow only).







## 2026-07-24 ? Green-screen crash after static render-instance cache







- Reproduction: Enter Game play-test after the static prefab instance cache landed; the editor could freeze or exit with a solid green/corrupt frame.



- Impact: Play-test validation of the render-prep optimization was blocked.



- Cause: Dynamic player/preview instances were appended into the cached `static_render_instances` vector and trimmed with `resize` after submit. A failed or early-exit frame could leave the durable cache polluted; fence waits that failed to arm `SetEventOnCompletion` also skipped draining, allowing upload-heap overwrites while the GPU still referenced them (classic DXGI device-removed / green screen).



- Resolution: Keep the static cache immutable for the frame ? draw from `static_render_instances` by pointer plus a frame-local dynamic list (no append/resize on the durable cache); invalidate on authored edits (`mark_scene_dirty` / mesh sync), not sticky unsaved `scene_dirty` every frame; harden fence waits with a GetCompletedValue spin fallback; drain on Present failure.



- Verification: Rebuild `engine`; start a Game play-test and walk for several minutes while watching Diagnostics ? Performance (Render prep, CPU skinning, Cache rebuild).



- Remaining risk: Foliage dirty-cell in-place Map remains single-buffered. Catalog-wide GPU skinning is follow-on (player path uses GPU LBS).







## 2026-07-24 ? Normal terrain stream generation ran on the render thread







- Reproduction: Walk across a terrain-cell boundary; generating the incoming support fringe or view-biased outer ring consumed the render frame before collision and GPU upload could proceed.



- Impact: Even with per-frame generation budgets, procedural mesh construction created visible traversal stutters.



- Cause: `StreamedTerrainField::update` called `generate_stylized_terrain` synchronously for every budgeted normal stream cell. Terrain/paint stores could not safely be read by an arbitrary worker without a stable snapshot.



- Resolution: Normal walk-fringe and outer-ring jobs snapshot terrain edits, paint, and referenced material values, generate the `TerrainMesh` asynchronously, then commit the Jolt heightfield on the main thread when ready. Bootstrap/teleport support remains synchronous (or waits for a queued support result) to preserve floor safety.



- Verification: `engine_suite_tests --suite terrain` includes an async walk-fringe regression; rebuild `engine`.



- Remaining risk: Foliage scatter, terrain render-vertex conversion/normals, collision-body creation, and GPU upload still execute on the main thread. Fast travel retains its intentional synchronous support load.







## 2026-07-24 ? Water-buffer replacement drained the GPU during streaming







- Reproduction: Change a streamed water neighborhood or edit water/terrain while water is resident; the next water mesh upload can hitch.



- Impact: Water authoring and traversal near water could stall a frame even when the rendering workload itself was small.



- Cause: `Renderer::upload_water_vertices` called `wait_for_gpu()` before replacing the full water vertex buffer.



- Resolution: Retire the replaced buffer behind the renderer fence and release it after GPU completion. A blocking wait remains only as a device-signal failure fallback.



- Verification: Rebuild `engine`; run the `water` suite and an interactive water-stream/editor-edit smoke test.



- Remaining risk: Water mesh building and the full-resident buffer upload remain synchronous; water still needs per-cell buffers and amortized generation to eliminate CPU-side spikes.







## 2026-07-24 ? Foliage instance-buffer growth forced a GPU stall during streaming







- Reproduction: Enter a denser streamed foliage cell or grow the resident foliage set beyond the current GPU instance-buffer capacity; the frame that reallocates the buffer can visibly hitch.



- Impact: Traversal still stuttered even when terrain generation was amortized, especially on the first dense-cell transition.



- Cause: `Renderer::set_foliage_cell_instances` called `wait_for_gpu()` before replacing its upload buffer, so each capacity increase blocked the main/render thread for all work submitted by the prior frame.



- Resolution: Retire replaced foliage buffers behind the renderer fence and release them only after GPU completion. The new buffer and SRV are used immediately; a blocking wait remains only as a device-signal failure fallback.



- Verification: Rebuild `engine`; run the benchmark and cross a dense foliage stream boundary in a Game play-test.



- Remaining risk: Foliage scattering, batch-map rebuild, and CPU upload remain synchronous. They can still consume a frame on exceptionally dense cells, but no longer wait for the GPU before proceeding.







## 2026-07-24 ? Walk cell-edge hitch from synchronous support strip







- Reproduction: Game play-test walk across a 40 m terrain cell boundary; brief freeze / lag spike even after outer-ring amortize + look-gate.



- Impact: Open-world traversal still felt laggy every ~40 m; easy to blame physics or input.



- Cause: Missing **support** cells always generated on the main thread in one update (up to a full strip of 3 for radius 1), before any unload ? correct for fall-through safety, but one-frame gen+heightfield cost.



- Resolution: Amortize support fringe when the focus cell is already resident (`max_new_support_cells`, default 1). Hold prior cells until the new support disc is complete (no floor hole). Bootstrap / teleport (focus cell missing) still loads full support same frame. Outer ring waits until support catches up. Play path also enables **async cell generation** (worker thread gen, main-thread collision commit) for walk fringe / outer ring.



- Verification: `terrain` suite recenter fringe + teleport asserts; rebuild `engine`; MCP walk across cell edges.



- Remaining risk: Teleport / FT still sync-loads support; foliage scatter on newly dirty cells can add a smaller hitch; async commit still touches the main thread for Jolt heightfield add.







## 2026-07-24 ? Event look_at still framed the player (cine pivot overwritten)







- Reproduction: Sandbox Game play-test; walk into `event_zone_sandbox` ? `evt_sandbox_zone_pan`. Camera yaw/distance changed slightly but stayed glued to the player instead of filming NPC/campfire/oak.



- Impact: Cinematic leave-player focus appeared broken even though timeline `look_at` directives were active.



- Cause: Rigidbody play path updated the player spawn visual each frame and called `orbit_camera->update(player_feet)` **after** the cinematic pivot blend. Game pass then read `orbit_camera->view_projection()`, so the late player-follow won.



- Resolution: Skip late orbit `update` (and facing-from-camera) while `camera_directive().active` or held `event_cine_pivot` under control lock.



- Verification: Rebuild `engine`; re-walk the sandbox blue pad ? camera should leave the player for each subject.



- Remaining risk: CharacterVirtual-only play path did not hit this overwrite; keep both paths in mind for future camera drivers.







## 2026-07-23 ? Stutter / lag while looking around 360? (Game camera)







- Reproduction: Game play-test (or editor Game tab); orbit / mouse-look a full 360. Motion is not smooth ? hitchy stutter while turning in place (not only when walking across cell boundaries).



- Impact: Look-around QA and combat readability feel poor; easy to confuse with SSAO/shadow flicker (those were visual; this is frame-time).



- Cause: (1) **View-biased terrain/foliage streaming** keyed off live orbit forward ? a 360? spin thrash-adds/removes outer-ring cells on the main thread (gen + heightfield + GPU + foliage). (2) Play-test stream focus used **orbit eye**, so near cell edges look-only could also flip the focus cell. (3) The original look-hot threshold was `0.025` rad/frame (~8 px at default sensitivity), so ordinary slow turns bypassed the gate and still churned the stream neighborhood. (4) Debug CPU-bound frames amplify the hitch.



- Resolution: `apply_stream_view_bias_look_gate` freezes view-bias forward and sets `max_new_cells=0` while yaw is hot (settle ~12 frames). Its default threshold is now `0.0005` rad/frame so sub-pixel/slow turns are gated too. Play-test stream focus prefers **feet/pivot** over eye. Support disc still follows XZ for walking. **Bugfix (same day):** streamer treated `max_new_cells==0` as *unlimited* outer loads ? look-gate was dumping the whole pending outer ring in one frame; 0 now correctly means block. **Follow-up (2026-07-26):** look-gate also sets `max_ready_commits=0` ? workers finishing mid-turn were still committing one cell (heightfield + foliage/GPU), which showed as `terrainCells` bumping during a fast look even with outer queue frozen.



- Verification: `terrain` suite look-gate + zero-budget asserts; rebuild `engine`; MCP look-around after restart.



- Remaining risk: Post-settle catch-up still lands one collision commit per frame, then GPU upload and foliage scatter on following frames (split 2026-07-26). Look-hot also pauses support fringe queueing; walking while looking delays fringe until yaw settles. Teleport still full-loads. Debug dual Scene+Game + CSM cost unchanged.



## 2026-07-22 ? Lag spikes made collision ?stop working? (player fell through / tunneled)







- Reproduction: walk in Game until a hitch (often terrain cell cross); during/right after the freeze, player falls through the floor or passes through thin static geometry. Feels like ?collision breaks when the game lags.?



- Impact: Play-test unsafe after any multi-hundred-ms hitch; easy to mis-diagnose as missing colliders or broken Rigidbody layers.



- Cause: (1) Frame dt is variable and clamped to **0.25 s** (`render_app.cpp`); physics used **one** Jolt collision step with default **discrete** motion quality, so a fat post-hitch step only tested overlap at the end of a large move and could skip clean over floors. (2) Terrain support-cell generation on the main thread remains the common hitch source (see stream-hitch finding below) ? it freezes the frame; the *next* frame then advances physics with the clamped fat dt.



- Resolution: `CollisionWorld::step` now substeps toward ~1/60 s (capped at 16 collision steps per Update). Dynamic bodies use Jolt `EMotionQuality::LinearCast` (CCD). Regression in `collision` suite: 0.25 s step at ?20 m/s must not tunnel a sphere through a thin floor.



- Verification: `engine_suite_tests --suite collision` (includes lag-spike tunnel case); rebuild `engine`.



- Remaining risk: Terrain support loads can still hitch (feel laggy); substeps/CCD stop tunneling but do not remove the hitch itself. Async terrain gen still deferred. CharacterVirtual path already swept ? less affected.







## 2026-07-22 ? Pre-existing test-suite instability found while verifying frustum culling (unrelated to that change)







- Reproduction: (1) `ctest -R foliage` (`engine_suite_tests --suite foliage`) fails at "sample ground-cover.layers.json loads" and pops a blocking Debug-CRT dialog ("Microsoft Visual C++ Runtime Library" window title, `tasklist /V`), hanging the process until killed. (2) `ctest -R regression_all` (`engine_tests.exe`) prints its asset-registry checks then exits with an access violation (`-1073741819` / SegFault) after "Asset database rebuild is incremental". (3) `engine editor --project samples/open-world-rpg --frames 2 --hidden true` (the `editor_smoke`/`editor_responsive_smoke` ctest targets) took minutes instead of seconds on this machine.



- Impact: `ctest` cannot currently be run unattended end-to-end; a stuck CRT dialog silently blocks a CI-style run rather than failing fast.



- Cause: Not root-caused. Confirmed **not** caused by the frustum-culling change below ? reproduces identically with `include/engine/rendering/viewport_picking.h`, `src/rendering/viewport_picking.cpp`, `src/rendering/render_app.cpp`, and `tests/suite_tests.cpp` stashed back to `HEAD` (i.e. on top of whatever else was already uncommitted in the tree, minus this change). `foliage`/`regression_all` exercise `engine/world/foliage_field.*` and `engine/assets/asset_registry.*` ? neither is touched by the rendering change. The `editor_smoke` slowness reproduced on the same reverted baseline, so it is at minimum not worse; whether it is a real regression from other in-flight uncommitted work or just this machine under heavy concurrent build/test load was not isolated.



- Resolution: None yet ? out of scope for the rendering change that surfaced this. Needs a dedicated debugging session (attach a debugger to the foliage suite process before dismissing/killing the CRT dialog to get the assertion message; get a crash dump for `regression_all`).



- Verification: Reproduced twice each for `foliage` and `regression_all`, with and without the culling change present.



- Remaining risk: Any automated/CI run of these suites will hang or fail until root-caused; `foliage`'s CRT dialog in particular can silently stall a batch run indefinitely instead of erroring out.







## 2026-07-22 ? Rendering pipeline had no frustum culling (whole-world draw every frame)







- Reproduction: Any scene with off-camera terrain cells or placed props ? `draw_world_pass` issued a `DrawInstanced` for every resident terrain cell and every placed-object instance regardless of whether the camera could see it.



- Impact: CPU draw-call submission cost scaled with total resident geometry, not visible geometry; called out as a deferred item in this file's terrain-stream entry above.



- Cause: No view-frustum test existed anywhere in the render loop; terrain cells and placed objects had no tracked world-space bounds available at draw time.



- Resolution: Added `Frustum`/`FrustumPlane`, `frustum_from_view_projection`, and `frustum_intersects_aabb` (conservative AABB test) to `viewport_picking.h/.cpp`. `Renderer` now tracks a world AABB per resident terrain cell (`terrain_cell_bounds_`, computed from baked cell vertices at upload) and per `RenderInstance` (via the existing mesh-bounds catalog through `expand_prefab_render_instances`). `draw_world_pass` builds one frustum per pass from `params.view_projection` and skips the draw call for anything fully outside it.



- Verification: New `world` suite assertions (hand-built orthographic view-projection, boxes inside/straddling/outside each plane) ? `engine_suite_tests --suite world` 59/59 passed; `renderer_smoke` and `debug_world_smoke` ctest targets pass (exercise the culled draw paths); rebuilt `engine`.



- Remaining risk: Foliage instances and the legacy single-buffer terrain path are not culled yet (foliage already batches via instancing; legacy terrain path is one draw call for the whole mesh). No spatial index ? culling is a linear scan per pass, fine at current placed-object counts but revisit if that grows much further.







## 2026-07-22 ? Terrain stream hitch (1?2s freeze) when crossing cells







- Reproduction: walk in Game/debug world until the focus cell changes; frame freezes for ~1?2s, then smooth until the next boundary. After amortize v1, residual stutter still dropped the player through the world.



- Impact: Open-world traversal felt unplayable; fall-through made play-test unsafe.



- Cause: (1) `StreamedTerrainField::update` generated every missing cell on the main thread in one call, then rebuilt/uploaded the **entire** resident terrain vertex buffer; foliage rescattered every loaded cell on each sync. (2) Amortize v1 still budgeted **support** cells and unloaded the old neighborhood *before* new support existed, so the floor under the player could vanish for several frames. (3) Per-cell GPU replace/remove called `wait_for_gpu()` synchronously.



- Resolution: Amortize outer-ring generation only; load the full support disc immediately **before** unload; view-biased wanted set; per-cell GPU buffers with fence-deferred retirement; incremental foliage cell scatter.



- Verification: `terrain` suite ? support-before-unload recenter keeps focus neighbors; rebuild `engine`; MCP walk play-test.



- Remaining risk: Teleport / FT still hitches once for full support; async gen still deferred (frustum culling of terrain cells / placed objects shipped 2026-07-22, see entry below and `context/features/streaming-lod-budgets.md`). Walk fringe support amortize shipped 2026-07-24.







## 2026-07-22 ? Co-op host fell through map while testing multiplayer







- Reproduction: local co-op play-test; control/possess guest and move away (or otherwise recenter terrain stream off the host); host drops through the world.



- Impact: Colliders looked ?broken in multiplayer? even though Jolt layers were fine.



- Cause: (1) `StreamedTerrainField` called `CollisionWorld::unload_cell` using 40?m terrain keys that collide with 128?m placement keys, destroying the host Rigidbody when origin terrain unloaded. (2) Stream focus followed only the possessed camera, so idle host lost heightfield support under gravity.



- Resolution: Terrain stream removes only its heightfield body ids; multi-focus update keeps cells under host + guest during `coop_local`.



- Verification: `terrain` suite ? Dynamic body survives stream recenter; multi-focus keeps distant cells. Rebuild `engine`; co-op F7 walk guest away ? host stays on ground.



- Remaining risk: World-partition `unload_cell` for true placement streaming still nukes a whole cell bucket by design; CharacterVirtual guest is not cell-owned.







## 2026-07-22 ? Co-op F7 moved guest but Game camera stayed on host







- Reproduction: co-op local with Rigidbody host; F7 possess guest; WASD moves guest; Game view still frames host.



- Impact: Guest QA looked broken even after possess input worked.



- Cause: After the early orbit pivot update to the guest, the spawn-entity sync path always called `orbit_camera->update(host_feet)` for `player_locomotion`, clobbering the guest pivot every frame.



- Resolution: Spawn sync respects `coop_focus_slot`; guest possess updates orbit from guest feet and leaves host mesh at host feet.



- Verification: Rebuild `engine`; F7 ? Game camera jumps to guest; look/WASD stay on guest until F7 again.



- Remaining risk: Still one shared OrbitCamera instance (fine for local prove-out); per-slot camera prefs can wait for real net co-op.







## 2026-07-22 ? F7 co-op camera did nothing / WASD stayed on host







- Reproduction: co-op local play-test; press F7 expecting other player camera + control.



- Impact: Could not QA the guest avatar with look/WASD.



- Cause: (1) F7 lived only in ImGui shortcuts and was easy to miss. (2) Auto ?camera follows driver? snapped focus back to host on any WASD, undoing F7. (3) WASD always moved host; arrows-only guest control was easy to miss.



- Resolution: F7 via SDL edge-detect in the play loop; sticky possess slot; WASD+look+jump drive the focused avatar only; camera/facing follow that slot.



- Verification: Rebuild `engine`; Start co-op local ? F7 ? status ?possess: guest? ? WASD moves guest and camera stays on them; F7 again returns to host.



- Remaining risk: Guest still CharacterController-only (no Rigidbody loco path); F7 requires Game tab + `coop_local` guest spawn.







## 2026-07-23 ? Residual SSAO sparkle under look-around (smooth quantized noise)







- Reproduction: Game play-test; look around ? contact darkening still sparkled slightly on hills (prior continuous `sin` hash amplified depth-reconstruct FP jitter).



- Impact: Easy to read as light/texture flicker even after face-normal + world-noise fixes.



- Cause: Continuous `frac(sin(dot(worldPos,?)))` kernel rotation is unstable under tiny reconstructed-position deltas; no temporal AO history yet.



- Resolution: Smooth hermite blend across 25 cm world-hash cells; SSAO intensity 0.30; 7?7 composite blur. MCP `editor_input` `action=look` (dx/dy[/frames]) for look-around QA without SDL relative mouse.



- Verification: Rebuild `engine`; kill/restart editor + MCP; play-test look burst ? left/right terrain still-frame diffs 0.000% meanAbs; look pans change view as expected.



- Remaining risk: True temporal AO still deferred (editor dual-viewport history path); silhouette shimmer without MSAA; flat face normals still read as hard facet shading (not flicker).







## 2026-07-22 ? Lighting flicker when looking around (face normals + SSAO)







- Reproduction: F5 play-test / Game tab; look around ? hillside lighting and contact darkening crawl or pop (?shadow flicker?).



- Impact: World looked choppy; easy to mistake for missing cascaded shadow maps.



- Cause: (1) Opaque/foliage PBR derived normals from `ddx`/`ddy` of world position, which flips at triangle edges under camera motion. (2) Residual SSAO kernel crawl (screen-space AO; no sun shadow maps).



- Resolution: Store stable triangle face normals on the GPU vertex stream and light from those; SSAO uses continuous world-space noise, depth-edge rejection, and softer defaults. Full temporal AO still deferred.



- Verification: Rebuild `engine`; look around in Game ? terrain/object shading holds; AO much quieter.



- Remaining risk: Soft AO only (no temporal history); foliage bend does not re-orthogonalize normals; glTF authored normals still replaced by flat face normals.







## 2026-07-22 ? SSAO flicker when looking around (world-stable noise)







- Reproduction: look around in Game/Scene; contact darkening crawls/flickers (no directional shadow maps ? this is SSAO).



- Impact: World looked ?choppy?; users mistook AO for shadow-map instability.



- Cause: Screen-space interleaved noise retargeted the AO kernel under camera motion.



- Resolution: World-stable kernel hash from quantized world position + wider 5x5 composite blur. Full temporal history deferred (editor dual-viewport copy path was unstable in a first attempt).



- Verification: Rebuild `engine`; look around ? AO should hold much steadier.



- Remaining risk: Some residual crawl remains without temporal accumulation; track as a follow-on if needed.







## 2026-07-22 ? Esc pause teleported play-test player / camera







- Reproduction: F5 play-test, walk away from spawn, press Esc (pause). Camera/body jumps; terrain around player may also look wrong.



- Impact: Pause/menu flow felt broken; co-op lobby QA path looked like a soft reset.



- Cause: (1) `simulate_dynamics` was tied to `test_session_running()`, so Esc rebuilt motion bodies and orphaned `player_locomotion` handles. (2) Terrain stream focus used frozen edit `camera.position()` instead of gameplay orbit camera during test.



- Resolution: Keep `simulate_dynamics` true for entire active test session; only gate step/input on Running. Stream focus uses `camera_position` (orbit) while test is active.



- Verification: Rebuild `engine`; walk far, Esc ? stay in place with pause overlay; Resume continues.



- Remaining risk: Co-op lobby **Start** still intentionally restarts the play-test at the spawn entity (different from Esc pause).







## 2026-07-22 ? Local co-op guest CharacterController had no mesh







- Reproduction: editor play-test with lobby Start / `--coop-local`; look for second player; arrow keys may move an invisible guest.



- Impact: Dual-slot session appeared broken (only host visible) even though guest physics/camera midpoint ran.



- Cause: Render path only called `append_character_render_instances` for host `debug_character` (or moved the scene spawn entity). Guest had no scene entity and was never drawn.



- Resolution: Draw guest with the same visual prefab + `guest_facing_yaw` when `coop_local` + `guest_character` are active.



- Verification: Rebuild `engine`; Start co-op local ? second mesh ~2m offset; F7 possess moves it with WASD.



- Remaining risk: Guest reuses host visual prefab (no distinct guest appearance); Rigidbody host + CharacterController guest still intentional for local prove-out.







## 2026-07-21 ? Imported glTF prefab meshes did not render until editor restart







- Reproduction: while the editor is running, add a new `.gltf` (or overwrite one via MCP/`engine_asset_apply` / bake), create or update a prefab that references it, place instances; keep the session open.



- Impact: placements resolved in the catalog/bounds but drew nothing in Scene/Game until restart. Affected every new mesh-backed prefab, which blocks MCP art iteration.



- Cause: `prefab_meshes_dirty` only ran `ensure_prefab_primitive_meshes`, which skipped non-`__primitive/` keys. New glTF paths never entered `imported_meshes`, so `mesh_ranges_` had no draw range (silent skip). Overwrites also kept the old GPU copy because existing keys were not invalidated.



- Resolution: `ensure_prefab_catalog_meshes` imports missing glTF/glb required by the prefab catalog and re-imports paths queued in `pending_mesh_reloads`. MCP asset writes for `.gltf`/`.glb` queue that reload and set `prefab_meshes_dirty`; the render loop still calls `Renderer::sync_imported_meshes()`.



- Verification: `assets` suite checks missing-key import + reload-queue reimport; rebuild `engine`, MCP-apply a mesh+prefab, place without restart.



- Remaining risk: rewriting only a sidecar albedo PNG (without rewriting the glTF or queuing reload) will not refresh GPU albedo until the mesh is invalidated; full file-watch hot reload is still out of scope.







## 2026-07-20 ? MSVC Debug `std::clamp` assert in World Forge Hierarchy Graph







- Reproduction: World Forge ? Hierarchy ? Graph (or Relationships ? Graph) with pane width ? 520px.



- Impact: Debug Assertion / abort (`std::clamp` invalid bounds; dialog may say ?invalid iterator range?).



- Cause: `list_w = clamp(avail.x * 0.70f, 320.0f, avail.x - 200.0f)` inverts when `avail.x < 520`. Crash dump stack hit `draw_hierarchy_factions_page`. Related footguns: cartography label clamps and graph node canvas clamps with unordered lo/hi.



- Resolution: `hierarchy_graph_list_width()` orders bounds; same ordering for map labels, graph node canvas, HUD slider thumb, orbit distance.



- Verification: Rebuild `engine`; open Hierarchy Graph in a narrow pane ? no assert.



- Remaining risk: Narrow panes still squeeze layout; clamp only prevents the CRT abort.







## 2026-07-19 ? Water refraction read the live lit RT (shoreline fuzz / highlight feedback)







- Reproduction: Game/editor view with authored water; look at shorelines and bright specular areas (sun).



- Impact: Grainy shimmering edges, ghosting around trunks, and blocky yellow/white blotches on the water surface.



- Cause: `draw_water_pass` wrote into `lit_color_` while the water pixel shader sampled that same resource as `sceneColor` for refraction (read-while-write / RT+SRV hazard). Wave UV offsets made the feedback move every frame.



- Resolution: Copy `lit_color_` into `water_scene_color_` before the water draw; bind that stable copy as `t0`. Remove unused depth SRV sampling so depth can stay `DEPTH_WRITE` for the water DSV test.



- Verification: Rebuild `engine`, relaunch editor; water edges and specular areas should no longer feedback-smear.



- Remaining risk: Soft depth-based shoreline foam still needs a separate depth copy (or reconstructed depth) before sampling depth in the water PS.







## 2026-07-16 ? Dialogue New Node forced a full graph relayout







- Reproduction: Dialogues ? Graph, open a laid-out tree, click **New Node** (or Duplicate).



- Impact: All nodes jumped into a single vertical BFS column stack (orphans share one depth), wiping any dragged layout ? looked ?squished into one line.?



- Cause: `dialogue_graph_needs_layout` always called `layout_dialogue_graph(..., preserve_existing=false)`, clearing positions. New unlinked nodes all land at `max_depth+1`, so they stack in one column.



- Resolution: Rename to `dialogue_graph_full_relayout` (tree switch / import / Auto Layout only). New/Duplicate place the node beside the selection via incremental layout; Delete/Undo no longer full-relayout.



- Verification: Rebuild `engine`; New Node keeps existing layout and offsets the new card to the right of the selection.



- Remaining risk: Auto Layout still packs orphans into one column by design; use links or Auto Layout intentionally.







## 2026-07-15 ? New toolbar icon macros silently render blank without a matching font glyph range







- Reproduction: add an `ICON_FA_*` macro to `include/engine/editor/editor_icons.h` and use it in a tab/button label without also adding its codepoint to `icon_ranges` in `src/ui/game_fonts.cpp`.



- Impact: the merged Font Awesome font only rasterizes the explicit codepoint pairs listed in `icon_ranges`; any icon macro whose codepoint is missing from that allowlist renders as a blank/tofu glyph instead of the intended icon, with no build or runtime error.



- Cause: `ICON_FA_MOUNTAIN` (`0xf6fc`, Sculpt tab) and `ICON_FA_DESKTOP` (`0xf108`, UI tab) were added to `editor_icons.h` after the `icon_ranges` allowlist was last updated, and neither codepoint was added to the range list.



- Resolution: when adding a new `ICON_FA_*` macro for editor UI (e.g. `ICON_FA_GLOBE` for the World Forge tab, TICKET-0015), also add its codepoint as a `{cp, cp}` pair to `icon_ranges` in `game_fonts.cpp`.



- Verification: `ICON_FA_GLOBE` (`0xf0ac`) renders correctly on the World Forge tab after adding it to `icon_ranges`; `ICON_FA_MOUNTAIN`/`ICON_FA_DESKTOP` were left as pre-existing, out-of-scope gaps for this change.



- Remaining risk: `ICON_FA_MOUNTAIN` (Sculpt tab) and `ICON_FA_DESKTOP` (UI tab) still render blank until their codepoints are added to `icon_ranges` in a follow-up fix.







## 2026-07-03 ? MCP-created prefab primitives did not render until editor restart







- Reproduction: create a new compositional prefab through `engine_prefab_apply` while the editor is running, place instances, keep the session open without restarting.



- Impact: placed props used catalog/bounds data but drew nothing; older prefabs from launch (tree, campfire) still rendered.



- Cause: `prefab_meshes_dirty` regenerated CPU primitive meshes in `imported_meshes` but never re-uploaded them into the renderer `mesh_ranges_` map built only at `initialize()`.



- Resolution: `Renderer::sync_imported_meshes()` rebuilds the prop vertex buffer when the catalog changes; render loop calls it after `ensure_prefab_primitive_meshes()`.



- Verification: create `bush.prefab.json` via MCP, place bushes live, confirm green shrub meshes appear without restarting the editor.



- Remaining risk: very large mesh uploads may hitch one frame while the GPU buffer is rebuilt.







## 2026-07-03 ? MCP could not reach editor: relative vs absolute project path pipe mismatch







- Reproduction: launch editor with `--project samples/open-world-rpg`, enable MCP connection; Cursor MCP uses absolute `--project` from `.cursor/mcp.json`.



- Impact: `engine_editor_status` reported disconnected while the editor had MCP enabled; live `engine_scene_apply` could not reach the bridge.



- Cause: named-pipe identity hashed `lexically_normal()` path strings only, so `samples/open-world-rpg` and `C:/.../samples/open-world-rpg` produced different pipe names.



- Resolution: normalize project roots with `absolute` + `weakly_canonical` before hashing in `editor_bridge.cpp`.



- Verification: toggle MCP connection off/on after rebuild (or restart editor); `engine_editor_status` should report `editorRunning: true`.



- Remaining risk: editor and MCP must point at the same project directory on disk, not two different copies.







## 2026-07-03 ? Cursor MCP timeout: NDJSON stdio vs Content-Length framing







- Reproduction: enable `ai-rpg-engine` in Cursor; MCP logs show `createClient` then `MCP IPC timeout` after 30s; `mcp-trace.jsonl` shows `frame_skip` with raw `{"method":"initialize"...` lines at the same timestamps.



- Impact: Cursor stayed yellow; `createClient` timed out; no tools loaded.



- Cause: Cursor sends newline-delimited JSON on stdio, not `Content-Length` framed messages. The server skipped those lines as unknown framing.



- Resolution: accept line-delimited JSON when a line starts with `{`; mirror the same format on stdout for responses when that mode is detected. Keep Content-Length support for scripted tests.



- Verification: reload MCP in Cursor; trace should show `frame_ndjson`, `request`/`initialize`, and `tools/list` within seconds.



- Remaining risk: very large single-line JSON payloads are uncommon for MCP but would need framed reads if introduced.







## 2026-07-03 ? Cursor MCP yellow: case-sensitive Content-Length header







- Reproduction: enable `ai-rpg-engine` in Cursor MCP; `mcp-trace.jsonl` shows repeated `server_start` / `server_stop` with no `request` events.



- Impact: Cursor stayed yellow; tools never loaded; editor bridge saw no `client_connected` because MCP never reached tool calls.



- Cause: `read_message()` only accepted `Content-Length:` (exact case). Cursor sends lowercase `content-length:` per HTTP-style headers.



- Resolution: case-insensitive `content-length` parsing in `mcp_server.cpp`; log skipped framing lines as `frame_skip` in the trace.



- Verification: reload MCP in Cursor; `mcp-trace.jsonl` should show `request` for `initialize` and `tools/list` within seconds.



- Remaining risk: none known for header casing; other Cursor UI yellow states may still appear briefly during reconnect.







## 2026-07-03 ? Cursor MCP hung on ?loading tools? before the editor starts







- Reproduction: enable `ai-rpg-engine` in Cursor MCP without launching the editor.



- Impact: Cursor stayed on step 2 (?loading tools?) for a long time or showed a yellow/warning state.



- Cause: Windows text-mode stdio could break MCP `Content-Length` framing; Cursor also needs reliable process launch paths and handlers for `ping` / empty `resources/list` / `prompts/list` probes during connect.



- Resolution: binary stdin/stdout in `run_mcp_server`, robust framed reads/writes, extra MCP handlers, and `tools/mcp-server.cmd` wired from `.cursor/mcp.json`.



- Verification: reload MCP in Cursor; tools should appear within a few seconds even when the editor is not running.



- Remaining risk: first `engine.exe` cold start can still take a couple seconds while Windows loads linked DLLs.







## 2026-07-03 ? Live bridge pipe I/O blocked the editor render thread







- Reproduction: enable MCP connection in the editor while Cursor MCP (`ai-rpg-engine`) is active.



- Impact: Cursor MCP showed yellow/warning; the editor became not responding and sometimes crashed.



- Cause: accept/read/write on the named pipe ran on the render thread and could deadlock with the MCP client waiting for a response.



- Resolution: move all pipe accept/read/write to a dedicated bridge worker thread; the render thread only dispatches one non-blocking `poll_pending()` handler call per frame.



- Verification: enable MCP in Diagnostics with Cursor connected; editor UI should stay responsive and `engine_editor_status` should return within about one second.



- Remaining risk: very slow scene commands still stall the UI while they execute on the render thread.







## 2026-07-03 ? MCP status probes blocked for five seconds without live automation







- Reproduction: open Cursor MCP while the editor is running but **Enable MCP connection** is off.



- Impact: Cursor showed tools loading for several seconds; `engine_editor_status` appeared hung.



- Cause: `forward_to_editor()` called `WaitNamedPipeW` with a 5000 ms timeout even when no bridge was listening.



- Resolution: probe with `is_editor_running()` first (50 ms cap), return unavailable immediately, and lower live request timeouts.



- Verification: reload MCP in Cursor without enabling the editor bridge; tools should list quickly and status should report disconnected.



- Remaining risk: the first `engine.exe mcp` cold start can still take a second while Windows loads the binary.







## 2026-07-03 ? Disabling live automation could freeze the editor







- Reproduction: enable MCP connection in Diagnostics, let Cursor connect, then uncheck the box.



- Impact: the editor became not responding and sometimes crashed.



- Cause: pipe teardown closed the listening handle while a client could still be connected; response `WriteFile` could block the render thread.



- Resolution: poll before teardown each frame, call `DisconnectNamedPipe` before `CloseHandle`, and use timed overlapped writes with shorter read/write budgets.



- Verification: toggle MCP connection on/off repeatedly while Cursor MCP is enabled.



- Remaining risk: heavy concurrent MCP traffic while disabling may still drop one in-flight request.







## 2026-07-03 ? Live automation bridge is opt-in inside the editor







- Reproduction: launch `engine editor` with Cursor MCP configured; the window stayed black or not responding while the bridge listened on every frame.



- Impact: normal editing was blocked by pipe probes and render-thread bridge I/O even when MCP was not intended.



- Cause: the named pipe started automatically at editor launch; Cursor MCP and probe clients could connect before the user was ready.



- Resolution: add **Enable MCP connection** in the Diagnostics panel (off by default); start/stop the bridge only while enabled; remove blocking `FlushFileBuffers` on disconnect; cap bridge reads with `PeekNamedPipe` timeouts.



- Verification: rebuild `engine`, launch editor without enabling MCP (UI should respond), then enable MCP and validate tools from Cursor.



- Remaining risk: a misbehaving client can still stall the render thread briefly while live automation is enabled.







## 2026-07-03 ? Per-frame full asset scan froze the editor







- Reproduction: launch `engine editor` after the MCP/Lua slice landed.



- Impact: Windows reported the editor as not responding; the viewport stayed black or updated only intermittently.



- Cause: `reload_changed_lua_scripts()` called `AssetMonitor::poll()` every frame, which recursively scanned and content-hashed every file under `assets/`.



- Resolution: replace script hot reload with a lightweight `ScriptFileMonitor` that watches only `assets/scripts/*.lua` write times every 30 frames; throttle MCP bridge polling and remove blocking sleeps on the render thread.



- Verification: rebuild `engine`, relaunch the editor, and confirm the UI renders while MCP remains available.



- Remaining risk: very large script trees still add periodic directory walk cost, but it is no longer proportional to total project asset size.







## 2026-07-03 ? MCP editor-status probes froze the live editor bridge







- Reproduction: launch `engine editor` while the Cursor MCP server is enabled (`.cursor/mcp.json`) or while `is_editor_running()` probes the named pipe.



- Impact: the editor window stayed black or appeared frozen because the render thread blocked inside the bridge on every pipe connection.



- Cause: `is_editor_running()` opened and immediately closed the pipe without sending a framed request, and `poll_pending()` blocked up to 2 seconds waiting for `Content-Length` data on the render thread.



- Resolution: `is_editor_running()` now uses `WaitNamedPipeW` only; `poll_pending()` rejects empty probe connections quickly and caps request reads at 100 ms.



- Verification: `automation` suite passes; rebuild `engine` and relaunch the editor.



- Remaining risk: a misbehaving external client that connects without sending a request can still stall the editor for ~20 ms per connection.







## 2026-07-02 ? Parallel MSVC rebuild collided on the program database







- Reproduction: trigger a full multi-source rebuild after changing the vcpkg dependency graph.



- Impact: `cl.exe` intermittently failed with C1041 while multiple compiler processes wrote `engine_core.pdb`.



- Cause: the Visual Studio 2019 toolchain did not serialize shared PDB writes for the target.



- Resolution: add MSVC `/FS` to the engine target.



- Verification: the subsequent full fastgltf rebuild completed successfully.



- Remaining risk: none known for the current generator; compile throughput may be slightly lower during PDB writes.







## 2026-07-02 ? Gizmo rotation committed but proxy rendering ignored it







- Reproduction: rotate the selected placement and release the gizmo.



- Impact: scene data changed, but the box proxy appeared unchanged, making rotation look nonfunctional.



- Cause: placement proxy model matrices applied translation and scale only; gizmo initialization also assumed identity rotation.



- Resolution: proxy and gizmo matrices now compose scale, the stored quaternion, and translation using DirectXMath.



- Verification: rotation survives scene commands and feeds the rendered proxy model matrix; editor GPU smoke and world command tests pass.



- Remaining risk: proxy boxes are only placeholders and cannot demonstrate the orientation of visually symmetric assets as clearly as imported meshes.







## 2026-07-02 ? Editor camera and gizmo interaction escaped viewport scope







- Reproduction: move the mouse outside the viewport after right-click interaction, or drag a gizmo while watching the placement proxy.



- Impact: camera rotation could continue outside the viewport and the gizmo moved without visibly moving the rendered proxy until commit.



- Cause: raw SDL relative motion was accumulated globally, and rendering read only committed scene transforms rather than the active gizmo preview.



- Resolution: camera capture starts only from a right press over the viewport image and ends on release; keyboard movement is gated with it. Active gizmo preview transforms now feed proxy rendering and commit once on release. Terrain clicks establish the next asset placement position.



- Verification: the responsive 900?600 capture preserves the locked panel geometry and visible proxy/gizmo; GPU editor smoke suites pass at two resolutions.



- Remaining risk: native pointer drag semantics still require manual testing until Windows UI automation is added.







## 2026-07-02 ? ImGuizmo matrix convention obscured the editor viewport







- Reproduction: pass transposed DirectX camera matrices to `ImGuizmo::Manipulate` while drawing over the offscreen viewport image.



- Impact: the gizmo disappeared; an earlier inconsistent combination also produced a black viewport/panel capture.



- Cause: the renderer's DirectXMath row-major storage already matches the interpretation required by the integrated ImGuizmo build. Applying another transpose was incorrect.



- Resolution: pass the camera view/projection storage directly and keep gizmo model matrices in ImGuizmo's native layout.



- Verification: `out/captures/editor-gizmo.png` shows terrain, physics body, placement proxy, panels, and translation gizmo together; `editor_smoke` covers initialization and rendering.



- Remaining risk: interactive drag and click behavior still require user-input automation beyond the current hidden lifecycle smoke test.







## 2026-07-02 ? Editor windows collapsed in the initial docking layout







- Reproduction: capture the first hidden editor frame after creating an unconstrained root dockspace.



- Impact: only the menu bar was visible; hierarchy, viewport, inspector, assets, and diagnostics appeared missing despite being submitted.



- Cause: ImGui persisted 32?35 first-use window sizes while the host dockspace occupied the viewport.



- Resolution: hidden runs disable layout persistence, visible runs store layout under generated output, and every core panel receives a deterministic usable first-run position and size.



- Verification: `out/captures/editor-mvp.png` shows all five panels; `editor_smoke` validates lifecycle behavior.



- Remaining risk: the central viewport is currently a transparent runtime overlay rather than a dedicated render texture.







## 2026-07-02 ? Terrain source duplication removed







- Reproduction: the visual debug world rendered a hardcoded flat plane while physics independently used a box floor.



- Impact: future terrain could appear correct while collision remained flat or misaligned.



- Cause: the early renderer and collision walking skeletons had separate placeholder geometry.



- Resolution: one deterministic terrain generator now supplies both render triangles and Jolt heightfield samples. Adjacent cells sample world-space coordinates to share exact borders.



- Verification: terrain topology, deterministic output, border equality, heightfield ray casts, cell unloading, and the D3D12 debug-world smoke path are automated.



- Remaining risk: only one cell is rendered at runtime; streamed multi-cell ownership and LOD seams remain future work.







## 2026-07-02 ? Diagnostics test deleted an actively open Windows log







- Reproduction: Remove the JSONL file before the process-owned logger closes its stream.



- Impact: The diagnostics suite fails even though the runtime event is flushed correctly.



- Cause: Windows prevents deletion of an open file handle.



- Resolution: Use a unique temporary log per test process and retain it as a diagnostic artifact until normal temporary cleanup.



- Verification: Named diagnostics suite completes and validates severity/priority JSON.



- Regression coverage: `diagnostics`.







## 2026-07-02 ? Initial 3D model translation used the wrong matrix convention







- Reproduction: Upload translation in row-vector slots while HLSL evaluates `mul(matrix, vector)`.



- Impact: The first perspective capture flattened and misplaced cube geometry near the horizon.



- Cause: CPU model layout did not match the column-vector shader convention already used by the camera matrix.



- Resolution: Store translation in the final matrix column and visually recapture the scene.



- Verification: Depth-buffered ground and cube remain visible under perspective after correction.



- Regression coverage: `debug_world_smoke` plus captured-image review.







The same review found and removed a redundant CPU camera transpose: DirectXMath row-major data uploaded to HLSL column-major constants already produces the required column-vector interpretation.







## 2026-07-02 ? Streaming suite used scheduler-dependent iteration waits







- Reproduction: Run the focused `streaming` suite when its async worker is not scheduled during 100 immediate polling iterations.



- Impact: The suite reports `cell committed` as failed even though the loader completes normally afterward.



- Cause: Iteration counts do not represent elapsed time and `yield` does not guarantee another thread runs.



- Resolution: Poll against a two-second steady-clock deadline with short sleeps; keep the production result-generation checks unchanged.



- Verification: Repeated focused streaming runs and the complete CTest suite pass.



- Regression coverage: Focused streaming and broad regression suites use bounded deadline waits.







## 2026-07-02 ? MSVC 19.27 future requires default-constructible payload







- Reproduction: Instantiate `std::future<Result<CellData>>` where `Result` intentionally has no default state.



- Impact: Asynchronous streaming would not compile on the installed toolchain.



- Cause: The older standard-library future implementation default-constructs internal payload storage.



- Resolution: Transport an owned `Result<CellData>` pointer through the future instead of adding an invalid default result state.



- Verification: Streaming compilation and cancellation/validation tests pass.



- Remaining risk: Reevaluate this compatibility layer after upgrading MSVC.







## 2026-07-02 ? Hot reload must validate before replacing the accepted snapshot







- Reproduction: Poll assets after introducing a missing or circular dependency.



- Impact: Applying the changed snapshot could leave runtime content partially updated and internally inconsistent.



- Cause: File changes are observable before the entire dependency graph is known to be valid.



- Resolution: Build and validate a candidate registry, emit no changes on failure, and replace the accepted snapshot only after validation succeeds.



- Verification: Monitor tests reject a cyclic snapshot, then recover and emit exactly one modified asset after correction.



- Regression coverage: Foundation asset-monitor tests.







## 2026-07-02 ? EnTT registry ownership blocked scene result transport







- Reproduction: Return a `Scene` containing an inline EnTT registry through `Result<Scene>` on MSVC 19.27.



- Impact: Scene loading could not compile because the registry did not satisfy the required move path.



- Cause: The registry ownership model and older standard-library variant implementation made implicit scene transport fragile.



- Resolution: Give `Scene` unique ownership of a stable registry allocation, define explicit move operations, prohibit copies, and construct result variants in place.



- Verification: Deterministic scene round-trip and CLI world loading pass.



- Regression coverage: Foundation tests load, move, serialize, and compare scenes.







## 2026-07-02 ? vcpkg release tag object is not a valid builtin baseline







- Reproduction: Use the annotated tag object hash for release `2026.01.16` as `builtin-baseline`.



- Impact: Manifest resolution is not guaranteed to address the intended versions commit.



- Cause: Annotated Git tags reference a tag object, which then references the commit required by vcpkg.



- Resolution: Pin underlying commit `66c0373dc7fca549e5803087b9487edfe3aca0a1`.



- Verification: Clean manifest configure resolved and built SDL 3.4.0.



- Regression coverage: CMake preset configuration invokes manifest resolution.







## 2026-07-02 ? CLI JSON help initially contained raw newlines







- Reproduction: Request `engine help --json` before JSON escaping was centralized.



- Impact: Automation consumers could not parse valid JSON reliably.



- Cause: The initial string encoder escaped quotes and slashes but not control characters.



- Resolution: Escape newline, carriage return, tab, and other control characters.



- Verification: Foundation test checks escaped help output; PowerShell JSON parsing passes.



- Regression coverage: `foundation_tests.cpp`.







## 2026-07-02 ? Current Windows SDK lacks modern DRED interfaces







- Reproduction: Inspect Windows SDK 10.0.18362.0 `d3d12.h` for current DRED interfaces.



- Impact: Device-removal diagnostics provide HRESULT causes but not modern breadcrumb/page-fault detail.



- Cause: Installed SDK predates those interfaces.



- Resolution: Preserve base device-removal reporting; schedule richer DRED data after a toolchain upgrade.



- Verification: Device error paths query `GetDeviceRemovedReason`.



- Remaining risk: Complex GPU hangs will be harder to diagnose until the SDK is upgraded.




## 2026-08-05 ? Main menu preview showed the gameplay HUD and no camera framing

- Reproduction: Boot `engine.exe editor --project samples/open-world-rpg` (default world `worlds/main-menu.world.json`) and press Play. Before the fix the Game view either stayed on the "Start a test session (F5)" placeholder or drew the player HUD (health, stamina, hotbar, compass) behind `main_menu`.
- Impact: The front-end could not be reviewed without a player play-test, so menu framing and layout were judged against gameplay chrome that will never ship on the title screen.
- Cause: Game-view rendering, canvas input, and HUD drawing were all gated on `test_session_active()`. A menu-only world had no session state to occupy, and `UiCanvasStack::draw_overlay` always drew the HUD layer whenever it had widgets.
- Resolution: Added `EditorState::menu_preview_active` plus `game_preview_active()`, so Play on `main-menu.world.json` enters a menu preview (Game tab, `main_menu` pushed, locked establishing camera) with no player spawn or gameplay simulation. `UiCanvasStack::set_hud_visible(false)` hides the gameplay HUD for menu-only presentation and is restored when the preview ends or a real play-test starts. Esc no longer pops `main_menu` while it is the preview root.
- Verification: `engine` + `engine_suite_tests` rebuilt; `--suite hud` 136/136 and `--suite automation` 190/190 pass. Screenshots confirmed the menu over the graybox backdrop with no HUD, Shift+F5 / `engine_editor_session` `end` returning to the empty Game view, and `vertical-slice.world.json` still spawning the player with the HUD (`sessionState: playing`, `testSession: running`).
- Remaining risk: The preview is selected by world filename (`main-menu.world.json`). Additional front-end worlds will need an authored marker or project field instead of the name check.

## 2026-08-06 - Editor hung the GPU ~36 s after load from an unbound foliage root argument

- Reproduction: Launch `engine.exe editor --project samples/open-world-rpg` (default `worlds/main-menu.world.json`) and leave it idle. The window disappears after 36-40 s while the process lingers; `out/logs/engine.jsonl` records `Device removed; reason HRESULT 0x887A0006` (`DXGI_ERROR_DEVICE_HUNG`) and `Could not present frame`. Reproduced on every run, Debug and RelWithDebInfo, with and without the D3D12 debug layer.
- Impact: The editor was unusable for any session longer than half a minute, which read as "the editor crashes on load". Frame rate also degraded to roughly 8 FPS before the hang; it now holds ~45 FPS on the same world.
- Cause: The foliage root signature gained parameter 5 (`t2`, the compacted visible-instance index buffer) for GPU instance culling in TICKET-0276, but only the `use_gpu_cull` + compacted branch bound it. With `k_enable_gpu_cull_draw` disabled, every foliage draw executed with root parameter 5 uninitialized. The foliage vertex shader can still reach `t2`, so the driver dereferenced a garbage descriptor and the GPU hung instead of reading zeros. `wait_for_gpu`/`wait_for_fence` used `INFINITE`, so the process deadlocked silently rather than reporting the fault.
- Resolution: `bind_foliage_graphics` now always binds root parameter 5, pointing it at the foliage instance buffer when no compacted index buffer is in play, so every path leaves the argument initialized. Supporting changes: retire (rather than free) `shadow_instance_buffer_`, `prop_instance_buffer_`, `particle_vertex_buffer_`, and the legacy terrain/water vertex buffers when they grow mid-frame; bounded GPU waits with device-removal checks; DRED auto-breadcrumbs and page-fault capture always on; run-length-encoded breadcrumb history plus named resources, command list, and queue; `BeginEvent`/`EndEvent` pass markers so a hung op maps to a pass; guards that skip and log out-of-range or non-finite draws; and `ENGINE_GPU_VALIDATION` / `ENGINE_GPU_TRACE` opt-in switches for GPU-based validation and residency tracing.
- Verification: GPU-based validation named the defect directly ("Uninitialized root argument accessed. Shader Stage: VERTEX, Root Parameter Index: [5]"). After the fix a 180 s idle soak ran 7,600+ presents with no errors and flat video-memory use (119 MB), versus a hang at present ~290 on every prior run. Live editor confirmed via `engine_editor_status` (`editorRunning: true`, 46 FPS) and a screenshot showing terrain, props, and grass foliage.
- Remaining risk: Other conditionally bound root parameters follow the same pattern (`if (mesh_shadow_srv_gpu_.ptr)`, `if (mesh_albedo_heap_)`, `if (foliage_shadow_srv_gpu_.ptr)`) and would hang the same way if the resource were ever missing while a shader reached it. Re-enabling `k_enable_gpu_cull_draw` still needs its own soak. Separately, the prop pass batches poorly - 323 single-instance draws of the same `bush_tall.gltf` - which is a standing performance item, not a correctness one.

## 2026-08-10 - Terrain tool stalls were hidden by throttled frame-dip persistence

- Reproduction: Apply a height-tool stroke in the Scene/Sculpt viewport. One observed session recorded a 347.52 ms frame (41.33 ms terrain plus 255.31 ms viewport/tool UI) followed by a 243.84 ms frame (71.18 ms terrain and 144.90 ms placement collision sync).
- Impact: The owner experiences severe terrain authoring hitches, but the throttled `performance_dip` event can omit a brief later stroke and make the issue appear unreproducible in log review.
- Cause: Frame-dip persistence intentionally records only the first, materially worse, or heartbeat samples. Terrain strokes commit and later finish asynchronous terrain/Jolt work outside that sampling contract.
- Resolution: The editor now writes unthrottled `terrain_sculpt_click_observed`, `terrain_sculpt_click_started`, `terrain_sculpt_click_rejected`, `terrain_sculpt_stroke_committed`, and `terrain_sculpt_reload_committed` events. They distinguish raw input from a rejected/non-hovered press and record click position/settings, whole-stroke duration, sample count and cost distribution, touched/reloaded cell counts, terrain generation, and heightfield-collision cost. Height-tool reloads queue only one background mesh job at a time; remaining touched cells drain after each completion instead of saturating the CPU with parallel `std::async` work.
- Verification: Debug rebuild and a new multi-cell height-tool stroke are pending. The next log must contain both events even if no `performance_dip` is persisted, and show at most one pending terrain reload.
- Remaining risk: The observed placement-collision sync was substantially more expensive than heightfield collision; its relationship to terrain editing still needs a captured operation trace.

## 2026-08-11 - Terrain height edits synchronously rebuilt the full water neighborhood

- Reproduction: Height-sculpt terrain with resident water. A recorded accepted terrain click was followed by a `water_stream` frame at 230.0 ms / 4.3 FPS.
- Impact: A small terrain edit freezes the editor while every resident water mesh is rebuilt before the per-cell GPU upload budget can amortize the result.
- Cause: Each terrain-height revision called `reload_loaded_water_cells`, which immediately rebuilt every loaded water cell and marked them all dirty.
- Resolution: Terrain-triggered water rebuilds now enter a pending cell queue. The frame loop rebuilds one water cell per frame before the existing one-cell upload budget.
- Verification: Debug rebuild and a terrain edit near water are pending. The next log must not show a full-neighborhood `water_stream` stall; water updates may visibly settle over several frames.
- Remaining risk: Direct Water-tool edits still use their small affected-cell reload path immediately; a large direct water brush may need a separate queue if it proves expensive.

## 2026-08-10 - Sculpt viewport retained unrelated Scene overlays during terrain authoring

- Reproduction: Open Sculpt on `main-menu`, execute a large height stroke, and observe the viewport while Scene-only map labels/collider overlays remain enabled. A captured desktop frame showed 2.4 FPS and a 225-sample sculpt status; the nearest persisted dip was 96.4 ms, including 70.4 ms of viewport UI.
- Impact: Terrain authoring pays projection and ImDrawList work for collision, event-zone, map, and selection overlays that do not help place the brush.
- Cause: The Scene/Sculpt viewport tail unconditionally drew all Scene debug overlays for both tabs, even though selection bounds were separately suppressed for Sculpt.
- Resolution: Sculpt now draws only terrain-relevant viewport markers and brush feedback. Scene retains selection, collision, authored-collider, event-zone, and World Forge map overlays.
- Verification: Debug rebuild and a repeat large Sculpt stroke are pending; the next viewport timing should exclude Scene overlay work while normal Scene overlays remain available.
- Remaining risk: If a large Sculpt stroke still reports a high viewport slice, subdivide the remaining Sculpt UI timing before changing brush behavior.

## 2026-08-10 - Idle Scene hover performed full mesh picking every frame

- Reproduction: Leave the mouse over the Scene viewport in the dense `main-menu.world.json`. Per-session `performance_dip` events reported 7-10 FPS with 81-112 ms in `uiViewportMs`; hierarchy, inspector, asset browser, diagnostics, simulation, and GPU were all small.
- Impact: Simply hovering the Scene viewport created sustained editor hitches, obscuring normal authoring and making the performance panel itself insufficiently actionable.
- Cause: Hover highlighting called `pick_placement` every frame. Its exact ray/AABB path iterated every placement and rebuilt bounds for every compositional prefab before finding the highlighted entity.
- Resolution: Hover highlighting caches the projected bounds of all visible placements for the current Scene camera, plus the camera-independent prefab bounds and transforms used to project them. Pointer moves now only compare cursor coordinates with cached rectangles; a camera or viewport change reprojects the cached sources rather than re-resolving each placement through the prefab catalog. Matrix and viewport comparisons tolerate sub-pixel floating-point drift so a visually stationary camera cannot invalidate the cache. Camera-capture frames do not refresh hover. Exact ray/AABB picking remains on click, where its precision and one-off cost are appropriate. The persisted log now breaks editor UI time into panels to guard this path.
- Verification: The camera-to-terrain cell broad phase was rejected after live logs still showed 53 ms and 127 ms pointer-move viewport work on the dense main-menu view. The editor persists a `hover_screen_bounds_rebuilt` event with its invalidation reason, source/visible placement counts, and rebuild duration. In the current Debug session, the initial scene-source build took 70.29 ms for 407 placements, while later viewport and camera refreshes reprojected those cached sources in 1.03-1.41 ms; normal hover stayed below 0.6 ms.
- Remaining risk: A scene mutation or world load still rebuilds the camera-independent source cache linearly, and exact click selection still scans a dense scene once. If either is perceptibly slow, amortize that source-cache refresh and add a bounded broad phase to exact ray/AABB picking with a correctness fallback.

## 2026-08-10 - Main-menu establishing shot pinned the Scene camera

- Reproduction: Open the editor on the default `main-menu.world.json`, switch to Scene, hold right mouse, and use WASD/mouse-look. The camera accepts input but snaps back to `menu_cam_establishing` every frame.
- Impact: The default project scene cannot be freely inspected or authored after startup.
- Cause: Startup automatically entered menu preview, and the front-end camera update reapplied its fixed pose to the shared debug camera without checking which viewport was active.
- Resolution: Main-menu startup now remains an ordinary Scene authoring session. Play/F5 explicitly enters menu preview and locks the establishing shot only while Game is active; Scene camera updates are never overwritten by the preview shot.
- Verification: Debug `engine` target rebuilt successfully. Desktop viewport QA remains pending.
- Remaining risk: Scene and front-end Game still share one `DebugCamera`, so switching from a moved Scene view back to Game intentionally reposes it to the authored establishing marker rather than retaining two independent poses.

## 2026-08-10 - Player bake rejected valid override-only animation clips

- Reproduction: Re-import `GoodPlayerModel` while `player.animator.json` references Animation Studio clips such as `Attack2`, `BowAim`, and `MagicCast` that exist as `player.<Clip>.anim.json` sidecars but not in glTF `animations[]`.
- Impact: Mesh, rig, atlas, and source clips baked successfully, but `ASSET-BAKE-CLIP-MISSING` marked the player bake failed even though runtime merges the DEC-0052 overrides and project validation succeeds.
- Cause: `asset_bake_verify.py` compared animator references only against glTF clip names and did not inspect valid override sidecars beside the baked mesh.
- Resolution: The verifier now accepts schema-v1 `animationClipOverride` sidecars whose `clipSource`, non-empty channels, and positive duration match the baked mesh; malformed or foreign-source sidecars remain fail-closed.
- Verification: `tools/test_asset_bake_verify.py` regression passes; named player bake reports `ok` with 17 glTF clips and eight preserved override clips; project validation, animator suite, and player-controller animation preview pass.

## 2026-08-10 - A single melee click could queue Attack2

- Reproduction: With a one-handed melee weapon equipped, press and hold LMB for the duration of Attack's cancel window. The input handler buffered `attack2` from the held state, even though the initial press had already started Attack.
- Impact: A single click/press could advance the light string to its second animation.
- Cause: Combo input treated `lmb_down` as an independent request for the next attack rather than accepting only a new press edge.
- Resolution: Removed held-button combo buffering. A distinct LMB press still immediately advances during a cancel window or buffers an early next strike.
- Verification: Debug `engine` rebuilt successfully. `ctest -R animator` remains red on the existing cancel-pose continuity assertion (436/437), which concerns the authored attack clips rather than this input path.
- Remaining risk: Desktop play-test verification is still needed to confirm click timing with actual mouse hardware.
