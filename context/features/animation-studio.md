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
| MCP | `engine_animation_call` — open/preview/play, **list_joints** / **set_skeleton**, **create_clip** / **create_state**, **set_duration** (`duration` seconds on open edit clip), key upsert, timeline events, held weld get/set/save. Live editor required. |
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
| MCP | `list_joints` (`name` skin, `displayName` / `channelName`), `set_joint`, `copy_keys` (`tracks`), `paste_keys` (optional `time`), `set_skeleton` (`visible`, `labels`). |

## Subject RH→LH channels

Player character clips sample with **sagittal handedness**: skin joint `LeftUpperArm` loads clip channel `RightUpperArm` and reflects the local pose (and the reverse). Animation Studio mirrors that on write:

1. Selected bone identity stays the **skin joint** (gizmo world track uses armature joint name).
2. Viewport / bone-list labels show the **visual / channel name** (`RightUpperArm` on the arm that looks right).
3. Key writes target that channel name and reflect TRS so the edited limb is the one you see.

Held weapon skins skip this path entirely.

## Capability roadmap (ordered tickets)

1. **TICKET-0248** — Viewport tab + sandbox stage chrome *(needs-approval)*
2. **TICKET-0249** — Subject picker, skinned Play/Pause/Step/scrub, bottom timeline *(needs-approval)*
3. **TICKET-0250** — Held gear swap on the sandbox subject *(needs-approval)* — catalog items with `worldMesh`; studio-session only (no inventory bag mutation). Armor/equip-strip mesh swap deferred until modular body slots.
4. **TICKET-0251** — Bone / hand-attach (weld) authoring in this view *(needs-approval)* — preferred attach surface; Game Inspector remains a fallback
5. **TICKET-0252** — Controller `timelineEvents` + particle trigger preview/authoring *(needs-approval)* — markers on Diagnostics Animation; `footstep` → dust; optional `payload.particle`; Save writes `*.animator.json`
6. **TICKET-0253** — Clip keyframe dual-edit ([DEC-0052](../decisions/index.md#dec-0052-dual-edit-animation-clips)) *(needs-approval)* — joint/channel key edit; Save → sidecar `*.anim.json`; Sync → `.gltf`; Replace from source deletes override

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
- Pose intent: bow arm (`Right*`) **forward toward aim** (not side T-pose / hang); string arm (`Left*`) hand on nock path early, **deeper elbow pull** at full draw (perpendicular to the aiming arm). Runtime mouse-aim will drive upper body look later; clip is the base aim silhouette.
- Held mesh: skinned `outrider_shortbow.gltf` samples `bow_draw` (`handAttach.drawClip`) remapped to BowShoot scrub; weld remains the grip root
- MCP `edit_clip` reloads the clip library/sidecars from disk so disk polish lands without a cold restart
- Preview: Animation tab → held `outrider_shortbow` → state `bowDraw` / `bowAim` / `bowRelease` or play-test LMB hold. Combat: LMB hold on `ranged` hotbar item drives `bowDrawn` (TICKET-0260).

## Out of epic scope

- Full Blockbench replacement (mesh/topology authoring)
- Live open-world entity pinning (may revisit later)
- IK solver authoring (rig metadata remains TICKET-0106 / DEC-0041)
- Replacing Scene/Prefab Inspector animator fields (additive studio)

## Related

- Runtime animator: [`animator.md`](animator.md)
- Clip format: [`../formats/animation-clip-assets.md`](../formats/animation-clip-assets.md)
- Hand attach / welds: [`gearing-system.md`](gearing-system.md)
- Editor tabs: [`editor-mvp.md`](editor-mvp.md)
- Planning: [`../planning/epics.md`](../planning/epics.md) EPIC-0019
