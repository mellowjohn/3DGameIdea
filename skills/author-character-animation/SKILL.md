---
name: author-character-animation
description: >-
  Author and polish character animation clips in Animation Studio via MCP:
  intent, weight, anticipation, pose-to-pose keys, combat windows, locomotion
  plants, silhouette checks, timeline events, and save_override. Use when
  creating or refining Idle/Walk/Run/Attack/BowShoot/MagicCast/Hit/Death clips,
  upserting joint keys, welding held gear, or reviewing animation quality.
---

# Author Character Animation

Reason about **why** a clip moves before placing bones. Edit live through **`engine_animation_call`** — never Python/`*.anim.json` disk rewrites as the primary path.

**Read first:** [`context/art/animation-craft.md`](../../context/art/animation-craft.md) (full vocabulary), [`context/features/animation-studio.md`](../../context/features/animation-studio.md), [`.cursor/rules/animation-studio-mcp-first.mdc`](../../.cursor/rules/animation-studio-mcp-first.mdc).

**MCP workflow skill:** [`live-editor-mcp`](../live-editor-mcp/SKILL.md) for editor/bridge/rebuild. This skill owns **craft quality**.

**Not this skill:** new animator C++ features (ticket + rebuild); Blockbench mesh shape ([`blockbench-mesh-authoring`](../blockbench-mesh-authoring/SKILL.md)); particle hits ([`author-particle-vfx`](../author-particle-vfx/SKILL.md)).

## Checklist

```
Clip authoring:
- [ ] One-line brief: intent + weight + energy + speed (+ style if not default)
- [ ] Live editor + Animation Studio subject / held gear set
- [ ] edit_clip → block poses (anticipation → extreme → settle, or contact → passing → contact)
- [ ] upsert_keys on major joints; shape timing with spacing (LINEAR/STEP)
- [ ] seek + screenshot: silhouette, foot plant, weapon/cast arc, no limb collapse
- [ ] Timeline events on gameplay windows (hit, release, footstep) — not only visual peaks
- [ ] save_override → seek loop seam + Idle transition
```

## 1. Intent brief (before keys)

State in one line:

`[role] — [intent]; [weight]; [energy]; [speed]`

Examples:

- `Attack — diagonal cut threat; heavy; aggressive; snappy commit, clear recovery`
- `Walk — travel; grounded; casual; loopable`
- `BowShoot — aim hold then release; grounded; focused; slow pull, snappy loose`

**Default project look:** stylized / fantasy / low-poly · grounded · readable at distance · loopable locomotion · snappy combat with clear recovery.

### Higher-level checks

| Ask | Fail look |
| --- | --- |
| Intent clear? | Generic flail |
| Weight / CoM? | Floaty or tippy |
| Anticipation + follow-through? | Instant pop / hard stop |
| Silhouette at mid distance? | Arms through torso; unreadable swing |
| Gameplay windows? | Pretty swing, useless hit timing |
| Root vs in-place? | Foot slide or double translation |
| Layering room? | Keys fight future aim IK / additive hits |

## 2. Pose-to-pose workflow

1. **Block extremes** (and contact / passing for locomotion).
2. Add **anticipation** opposite the commit; **settle / recovery** after.
3. Add **breakdowns** only where spacing reads wrong.
4. Prefer **overlapping action** (hips → spine → arms → weapon tip).
5. Shape speed with **key spacing** (close = slow, wide = fast) — Studio path is LINEAR/STEP, not dense Bezier polish.
6. **Hold frames** for aim / charge peaks; do not over-key fingers before silhouette works.

Combat shape: **wind-up / startup → active → recovery** (+ cancel if designed).  
Locomotion shape: **contact → passing → contact**; fix **foot plant** before polish.

## 3. MCP edit path

```
engine_animation_call:
  open / set_subject / set_held
  set_state → edit_clip
  optional set_duration
  upsert_key / upsert_keys / delete_key  (pass joint; eulerDeg OK)
  add_event / save_events when needed
  save_override
  seek / play + engine_editor_screenshot
```

- Prefer **`seek_times`** contact sheet for multi-extreme silhouette review; use **`list_keys` / `sample_pose` / `diff_pose`** before guessing from screenshots; prefer **`offset_keys` / `shift_keys` / `set_pose` / `ease_segment`** over absolute Euler fights; **`loop_report`** for locomotion seams.
- Pass **`joint`** on each key (overrides Studio selection).
- Player clips use sagittal RH→LH remap — see Animation Studio docs (bow hold on visual-right / clip `Right*`).
- If MCP is wrong/missing: **fix engine path** + rebuild lease; do not permanently script around it.

## 4. Critique language

When reviewing or prompting a pass, use descriptor axes:

| Axis | Words |
| --- | --- |
| Speed | slow · relaxed · casual · brisk · fast · explosive · snappy · heavy |
| Weight | light · heavy · floaty · grounded · stiff · loose |
| Energy | confident · nervous · tired · angry · happy · scared · aggressive · defensive · heroic |
| Quality | smooth · fluid · responsive · snappy · natural · mechanical · organic · loopable · seamless |

Principles to name in feedback: anticipation, follow-through, arcs, overlapping action, timing/spacing, exaggeration, staging.

## 5. Verify

| Check | How |
| --- | --- |
| Silhouette | `seek` at extremes + screenshot |
| Loop | First/last locomotion contact match |
| Combat | Active frames align with hit/VFX events |
| Transition | Crossfade to Idle without pop |
| Held gear | Weld + draw clip scrub with character |

## Related

- Full glossary: [`context/art/animation-craft.md`](../../context/art/animation-craft.md)
- Studio feature: [`context/features/animation-studio.md`](../../context/features/animation-studio.md)
- Clip format: [`context/formats/animation-clip-assets.md`](../../context/formats/animation-clip-assets.md)
- Controller / events: [`context/formats/animator-controller-assets.md`](../../context/formats/animator-controller-assets.md)
