# Animation Studio

Status: **active** (EPIC-0019) — sandbox through dual-edit keyframes (0248–0253) landed (needs-approval). Supersedes TICKET-0135.

## Goal

An isolated **Animation** editor viewport (peer of Scene / Game / UI) for previewing and polishing character animation **without** using the open-world scene.

## Viewport contract

| Surface | Role |
| --- | --- |
| **Animation** viewport tab | Sandbox stage: void clear color, base-plate ground quad, dedicated free-cam (`animation_camera`). No terrain/foliage/water/world entities. Disabled during play-test (same as Sculpt/UI). |
| Bottom strip | Diagnostics **Animation** tab: transport, **clip duration** drag+Apply, skeleton + bone list, **key toolbar** (Insert key / Key joint TRS / Delete key / **Save Override** / Sync), timeline, advanced key values + disk override under headers. |
| Preview play | **Solo state** — graph transitions disabled while editing. Playhead loops inside the preview state's clip duration. **Space** play/pauses. Selecting a timeline event pins that event's state and seeks to its time. |
| MCP | `engine_animation_call` — open/preview/play, **list_joints** / **set_skeleton**, **create_clip** / **create_state**, **set_duration**, key upsert, **undo** / **redo**, **list_keys** / **sample_pose** / **diff_pose** / **sample_series** / **held_tip_series**, **onion_skin**, **seek_times** contact sheet (event labels + tipTrail), **camera_orbit** / **camera_set** (`slash_review`), **offset_keys** / **shift_keys** / **set_pose**, **ease_segment** / **loop_report**, timeline events, held weld get/set/save / **inspect_weld**. Live editor required. |
| Subject | Dropdown of project characters / NPC prefabs (or skinned catalog entries); spawn into sandbox only — **TICKET-0249**. |

## Armature (TICKET-0259)

| Control | Behavior |
| --- | --- |
| Skeleton toggle | Cyan lines/joints for the subject skin; orange for skinned held items (e.g. shortbow limbs / `StringMid`). |
| Labels | Visual left/right joint names for the subject (sagittal of skin names). Selected status also shows skin + clip channel names when they differ. |
| Viewport pick | Left-click a joint sphere when weld gizmo and ImGuizmo are not capturing the cursor. |
| Bone list | Scrollable, indent-by-hierarchy list under Diagnostics: **Held** then **Subject**. Filter matches skin or display names. |
| Selection sources | `subject` writes keys on the active character clip using **clip channel names + pose reflect** (RH→LH); `held` opens the item draw clip (`handAttach.drawClip` / `bow_draw`), keeps that clip open while Diagnostics is open (must not force the character's active clip), timeline scrub drives the draw sample, and the bone gizmo writes limb keys there. |
| Key clipboard | **Ctrl+C** / **Copy TRS** — pos+rot+scale at the selected keyframe time (or scrub). **Ctrl+Shift+C** / **Copy track** — every T/R/S key on that joint (keeps spacing). **Ctrl+V** / **Paste** — write onto the **current** joint at the playhead (retarget by selecting another bone after copy). |
| Undo / redo | **Ctrl+Z** / **Ctrl+Y** (also **Ctrl+Shift+Z**) and toolbar **Undo** / **Redo**. Snapshot stack for the open edit-clip buffer (keys, duration, paste, bone-gizmo / timeline-key drags coalesce to one step). Cleared when switching clips. MCP: `undo` / `redo`; status `undoSize` / `redoSize`. Does not cover weld or timeline-event edits. |
| MCP | `list_joints` (`name` skin, `displayName` / `channelName`), `set_joint` (optional `path`), `delete_key` / `delete_keys` (`joint`+`path`+`time` / `times[]` / `keys[]`), `copy_keys` (`tracks`), `paste_keys` (optional `time`), `undo` / `redo`, `set_skeleton` (`visible`, `labels`), `list_keys` / `get_keys`, `sample_pose`, `diff_pose`, `sample_series` / `tip_series` / `held_tip_series`, `inspect_weld`, `onion_skin`, `seek_times` (contact sheet + `labelEvents` + `tipTrail`), `camera_status` / `camera_set` / `camera_look_at` / `camera_orbit` (`slash_review`), `offset_key(s)`, `shift_keys`, `set_pose` / `copy_pose_at`, `ease_segment` / `auto_breakdowns`, `loop_report`. |

## Subject RH→LH channels

Player character clips sample with **sagittal handedness**: skin joint `LeftUpperArm` loads clip channel `RightUpperArm` and reflects the local pose (and the reverse). Animation Studio mirrors that on write:

1. Selected bone identity stays the **skin joint** (gizmo world track uses armature joint name).
2. Viewport / bone-list labels show the **visual / channel name** (`RightUpperArm` on the arm that looks right).
3. Key writes target that channel name and reflect TRS so the edited limb is the one you see.

Held weapon skins skip this path entirely.

## Capability roadmap (ordered tickets)

1. **TICKET-0248** — Viewport tab + sandbox stage chrome *(needs-approval)*
2. **TICKET-0249** — Subject picker, skinned Play/Pause/Step/scrub, bottom timeline *(needs-approval)*
3. **TICKET-0250** — Held gear swap on the sandbox subject *(needs-approval)* — catalog items with `worldMesh`; studio-session only (no inventory bag mutation). **Armor slots:** Diagnostics Animation → **Armor slots** (head / chest / legs) plus MCP `set_armor`. Shells inherit player bake scale (`matchPlayerBake`) and skin with their own inverse binds; fit in Blockbench then rebake.
4. **TICKET-0286** — Appearance sockets (hair / skin / eyes) *(active)* — Diagnostics Animation → **Appearance sockets**; MCP `set_appearance`. Hair is a skinned overlay (`test_hair_spikes`), not `head` armor. Skin/eye tints are authored on `.character.json` / option tables; body-atlas multiply is follow-on.
5. **TICKET-0251** — Bone / hand-attach (weld) authoring in this view *(needs-approval)* — preferred attach surface; Game Inspector remains a fallback
6. **TICKET-0252** — Controller `timelineEvents` + particle trigger preview/authoring *(needs-approval)* — markers on Diagnostics Animation; `footstep` → dust; optional `payload.particle`; Save writes `*.animator.json`
7. **TICKET-0253** — Clip keyframe dual-edit ([DEC-0052](../decisions/index.md#dec-0052-dual-edit-animation-clips)) *(needs-approval)* — joint/channel key edit; Save → sidecar `*.anim.json`; Sync → `.gltf`; Replace from source deletes override

## Agent inspect / polish ops (TICKET-0261–0267)

| Kind | Role |
| --- | --- |
| `list_keys` / `get_keys` | Dump open edit-clip channels (`times`, `values`, `eulerDeg`) with `channelName` + `skinName`. Filter: `joint` / `joints[]`, `path`. |
| `sample_pose` | Local TRS (+ world when skin loaded) at `time`; seeks Studio scrub. |
| `diff_pose` | Deltas between `timeA`/`timeB` or vs `referenceClip`+`refTime`. |
| `sample_series` / `tip_series` / `held_tip_series` | Joint world/local across `times[]` (or N samples). Optional `gripJoint` / `includeHeld` grip world. **`held_tip_series` / `tip_series`** also return weld-aware `heldTipSeries` (`gripWorld`, `heldWorld`, `tipWorld`, `tipDir`, `pathLength`) with optional `tipLocal` (else mesh AABB farthest corner). |
| `inspect_weld` / `get_weld` | Current weld + optional `heldTip` at scrub. |
| `onion_skin` | `times[]` + `enabled` / `trail` / `ghosts` / `tipLocal` / `clear` — viewport tip trail + translucent grip→tip blade ghosts. |
| `seek_times` | `times[]` (max 12) → contact sheet under `out/`. Default `labelEvents=true`. **`tipTrail=true`** stamps cumulative tip arc (ortho `tipTrailView` side/front/top) and returns `tipTrail` world series. |
| `camera_status` / `camera_set` / `camera_look_at` / `camera_orbit` | Frame Animation Studio camera (stay on Animation tab). Orbit presets: `front` / `back` / `side` / `right` / `threequarter` / **`slash_review`** (aliases `melee`/`slash`: yaw ~145°, default distance 2.8). Prefer `front`/`side`/`right`/`slash_review` for melee tip review — avoid `threequarter`/`back`. |
| `offset_key(s)` | Add euler/translation/scale deltas at time(s); creates key from sampled pose if missing. |
| `shift_keys` | Time-shift joint track(s) by `dt` (clamped to duration). |
| `set_pose` / `copy_pose_at` | Copy **authored channel** TRS from `fromTime` / `fromClip` onto `time` or `times[]`. Only existing source channels are written — does not invent bind/default translation or scale keys (those collapse the skeleton). `times[]` stamps the same pose onto a hold loop. Explicit `poses[]` still writes rotation; T/S only if that path already exists on the clip. |
| `ease_segment` / `auto_breakdowns` | Insert N LINEAR ease-in/out/in-out keys between `timeA`/`timeB` (no Bezier runtime). **Clears keys strictly inside the segment first** so eased endpoint samples do not zigzag against denser authored keys. Prefer sparse pose-to-pose tracks; do not `sync_gltf` polish experiments into shipping glTF. |
| `loop_report` | First-vs-last seam diff + hip/foot Y series. |

CUBICSPLINE/Bezier is a follow-on. Scene `engine_editor_camera` switches to Scene — use Animation camera kinds for Studio stills.

## Clip duration

| Control | Behavior |
| --- | --- |
| UI | Transport row: DragFloat + **Duration** apply — sets open edit clip `duration_seconds`. Disabled when no clip is open. |
| MCP | `engine_animation_call` kind `set_duration` (alias `set_clip_duration`); arg `duration` (float seconds, must be `> 0`). Requires `edit_clip` / `create_clip` first. Status meta `duration` updates after apply. |
| Lengthen | Keeps existing keys; extends the timeline / loop range. |
| Shorten | Removes keys with time **strictly after** the new end (sole remaining key is clamped onto the end). Scrub clamps to the new range. |
| Keys still expand | `upsert_key` / UI key moves continue to do `max(duration, keyTime)`. `set_duration` is the explicit override (including shortening). |
| Persist | Marks clip dirty; **Save Override** / MCP `save_override` writes the sidecar. Push-to-library so solo preview (e.g. `bowShoot`) honors the new length immediately. |

## Dual-edit (clips)

See [DEC-0052](../decisions/index.md#dec-0052-dual-edit-animation-clips). Live keyframe edits go to an engine override sidecar (`mesh.ClipName.anim.json`); **Sync to source** writes LINEAR/STEP TRS back into the `.gltf`. Re-import keeps the override by default; **Replace from source** is the explicit wipe.

**Override-only clips** (created in Studio before Sync, e.g. `player.BowShoot.anim.json`) are merged on library load/reload by scanning `mesh.*.anim.json` next to the glTF — not only names already present in the source file.

### Outrider BowShoot (draft blockout)

- Clip override: `samples/open-world-rpg/assets/models/player.BowShoot.anim.json` (~1.7s; nock → slow pull → aim hold → release → settle)
- Controller state: `bowShoot` + `shoot` trigger (`*` → `bowShoot` → idle); timeline event `bowRelease` @ ~1.5s
- Draw remap (`map_bow_shoot_time_to_draw_u`): raise through ~0.80s, hold through ~1.40s, release 1.40–1.70s (paired with `bow_draw` peak/hold)
- **Arm authoring:** Studio labels subject bones by visual/channel side. Rotating the arm labeled `RightUpperArm` edits clip channel `RightUpperArm` (which drives skin `Left*`). Do not rename skin joints; the remap is intentional. Held shortbow weld remains skin `LeftHand` (visual right).
- Pose intent: bow arm (`Right*`) **forward toward aim** (not side T-pose / hang); string arm (`Left*`) nocks early, then wraps **outside the ribs** with the drawing hand at the cheek/jaw on `BowAim` / end of `BowDraw`. Large upper-arm **Y+** collapses the elbow through the chest — keep elbow world-X on the string side. Runtime mouse-aim will drive upper body look later; clip is the base aim silhouette.
- Held mesh: skinned `outrider_shortbow.gltf` samples `bow_draw` (`handAttach.drawClip`) remapped to BowShoot scrub; weld remains the grip root
- MCP `edit_clip` reloads the clip library/sidecars from disk so disk polish lands without a cold restart
- **Edit buffer vs preview clip:** Animation Studio must not replace an open `edit_clip` buffer with the runtime active clip (often Idle) every frame. Dirty/MCP authoring buffers stay sticky; clean `set_state` / preview still pins the state's motion. Status may show `edit:RunStrafeLeft | play:Idle`.
- Preview: Animation tab → held `outrider_shortbow` → state `bowDraw` / `bowAim` / `bowRelease` or play-test LMB hold. Combat: LMB hold on `ranged` hotbar item drives `bowDrawn` on the `upperBody` overlay (TICKET-0260); legs stay on idle/locomotion.

### Ashfell Attack slash (draft blockout)

- Clip override: `samples/open-world-rpg/assets/models/player.Attack.anim.json` (~1.15s; overhead wind → diagonal cut → recover)
- Combo string (TICKET-0268): `Attack` → `Attack2` (reverse waist/chest-height horizontal backhand, ~1.0s) → `Attack3` (full-windup overhead centerline cleave, ~1.25s)
  - Overrides: `player.Attack2.anim.json`, `player.Attack3.anim.json`
  - States: `attack` / `attack2` / `attack3` on the `upperBody` overlay (same mask as Block); triggers `attack` / `attack2` / `attack3`
  - Entry into overlay `attack` from `empty` / `block` only (not `*`); recover is `empty` so locomotion stays on base
  - Play-test: click buffering or holding LMB advances through hits 2 and 3; holding through the finisher does not loop a new string until a fresh press; `hitFrame` on each overlay state arms a short combat probe
  - Trajectory escalation: `Attack` high-right → low-left diagonal; `Attack2` low-left → right horizontal with a flat blade; `Attack3` overhead apex → low centerline with the longest recovery
  - Exaggeration/readability: all three hits use larger hip/chest counter-rotation and vertical hip compression; chain handoffs remain pose-matched at the cancel windows
- Controller state: `attack` (existing); LMB with `one_handed`+`melee` hotbar gate
- Pose intent: **diagonal slash** (not overhead pickaxe/hack). Sword arm (clip `Right*` / skin `LeftHand` grip) coils high-right, then tip sweeps **laterally** high-right → low-left with torso yaw past centerline (~30–60° carry). Keep tip plane around torso height through follow-through (avoid tip diving to the ground). Blade lays more edge-on through the cut rather than tip-up vertical. Peak tip speed mid-arc; decelerate after contact. Free arm stays near counterpose.
- Held mesh: `ashfell_arming_sword` (`handAttach` on skin `LeftHand`)
- MCP note: `upsert_key(s)` resolve display/channel names (`RightHand`) before skin names so sagittal subjects do not write the free arm by mistake
- Preview: Animation tab → subject `player.prefab.json` → held `ashfell_arming_sword` → state `attack` / `attack2` / `attack3`

### Runecaster MagicCast (draft blockout)

- Clip override: `samples/open-world-rpg/assets/models/player.MagicCast.anim.json` (~1.0s; coil backswing → charge → forward thrust; holds hands out at the end, no settle)
- Controller state: `magicCast` on the `upperBody` overlay (same mask as Block / Attack / Bow); `cast` trigger from `empty` / `block` → `magicCast` → `empty`. Base idle/locomotion keep the legs.
- Timeline events: `castCharge` @ 0.28s (rune gather during backswing), `castRelease` @ 0.8s (forward thrust / bolt spawn) on layer `upperBody`
- Play-test VFX: charge stack at focus tip + ground swirl under feet; on release a particle-only bolt that samples combat hurt volumes like arrows. Default magenta `arcane_bolt_*` on `guild_rune_focus`; `magic_fire` / `magic_frost` / `magic_lightning` tags select `fire_bolt_*` / `frost_bolt_*` / `lightning_bolt_*`. Ref: `context/art/concepts/arcane-bolt-spiral-inspiration.png`. Keep spawn_burst sparse — each call allocates a full emitter pool.
- Pose intent: focus arm (clip `Right*` / skin `LeftHand` grip) **swings the rune focus back** for the charge, then **both hands out in front** for the cast. Clip ends on that extension; overlay `empty` is the recover.
- Held mesh: `guild_rune_focus` (`handAttach` on skin `LeftHand`)
- Preview: Animation tab → held `guild_rune_focus` → state `magicCast`. Play-test: LMB edge on `magic`-tagged hotbar item fires `cast` while walking/running.

## Out of epic scope

- Full Blockbench replacement (mesh/topology authoring)
- Live open-world entity pinning (may revisit later)
- IK solver authoring (rig metadata remains TICKET-0106 / DEC-0041)
- Replacing Scene/Prefab Inspector animator fields (additive studio)

## Related

- Craft skill: [`../../skills/author-character-animation/SKILL.md`](../../skills/author-character-animation/SKILL.md)
- Craft / vocabulary / intent: [`../art/animation-craft.md`](../art/animation-craft.md)
- Runtime animator: [`animator.md`](animator.md)
- Clip format: [`../formats/animation-clip-assets.md`](../formats/animation-clip-assets.md)
- Hand attach / welds: [`gearing-system.md`](gearing-system.md)
- Editor tabs: [`editor-mvp.md`](editor-mvp.md)
- Planning: [`../planning/epics.md`](../planning/epics.md) EPIC-0019
