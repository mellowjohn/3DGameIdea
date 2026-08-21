# Animation Craft (Vocabulary + Intent)

Status: **active** — craft guidance for agents authoring clips in Animation Studio / MCP.

This is how to **reason** about poses and timing, not the runtime format. For tools and sidecar workflow see [`../features/animation-studio.md`](../features/animation-studio.md). For clip/controller schemas see [`../formats/animation-clip-assets.md`](../formats/animation-clip-assets.md) and [`../formats/animator-controller-assets.md`](../formats/animator-controller-assets.md).

**Engine fit:** stylized fantasy / low-poly readability, in-place locomotion + authored root motion where decided, combat windows, Animation Studio dual-edit. Prefer pose-to-pose key authoring with clear extremes; verify silhouette via `seek` + screenshot.

## Higher-level intent (reason before bones)

Before placing keys, answer:

| Question | Why it matters |
| --- | --- |
| **Animation intent** | What the clip communicates (threat, cast, fatigue, celebration) |
| **Character emotion** | Drives posture, speed, and secondary motion |
| **Weight distribution** | Which foot/hip carries load; affects plant and lean |
| **Balance / center of gravity** | Prevents floaty or tipping silhouettes |
| **Momentum / inertia** | Startup lag and recovery after fast moves |
| **Anticipation + follow-through** | Readability and weight without extra frames of noise |
| **Readability at distance** | Mid-camera / fog; big shapes over finger detail |
| **Gameplay responsiveness** | Input → visible commit; cancel / recovery windows |
| **Silhouette clarity** | Limb separation, weapon arc, cast hand vs torso |
| **Transition quality** | Idle↔locomotion, attack↔idle; no pops |
| **Root motion vs in-place** | Who owns translation; foot sliding vs control |
| **Layering** | Upper/lower body, additive hits, override aim |
| **Procedural + authored** | IK, look-at, physics on top of keys — do not fight them |

## AI-friendly descriptors (prompt / critique language)

Use these when describing or reviewing a clip:

| Axis | Vocabulary |
| --- | --- |
| **Speed** | Slow, relaxed, casual, brisk, fast, explosive, snappy, heavy |
| **Weight** | Light, heavy, floaty, grounded, stiff, loose |
| **Energy** | Confident, nervous, tired, angry, happy, scared, aggressive, defensive, heroic |
| **Style** | Realistic, stylized, cartoon, anime, Pixar-like, low-poly, pixel-inspired, Souls-like, JRPG, medieval, fantasy |
| **Quality** | Smooth, fluid, responsive, snappy, natural, mechanical, organic, loopable, seamless |

Default project look: **stylized / fantasy / low-poly**, **grounded**, **readable**, **loopable** locomotion, **snappy** combat commits with clear recovery.

## Core clip roles

Idle · Walk · Run · Sprint · Jump · Fall · Land · Roll · Dodge · Attack · Block · Hit Reaction · Death · Revival · Climb · Swim · Crouch · Crawl · Sit · Emote · Victory · Taunt

Name states/clips after the gameplay role; keep one clear intent per clip when possible.

## Timeline & keyframe terms

| Term | Meaning |
| --- | --- |
| **Keyframe** | Authored pose sample at a time |
| **In-between / tween** | Interpolated frames between keys |
| **Frame / FPS** | Discrete time base; author timing in seconds in Studio |
| **Timeline** | Time axis for keys and events |
| **Animation clip / sequence** | Named reusable TRS tracks |
| **Pose** | Full skeleton configuration at a time |
| **Hold frame** | Sustained pose (aim hold, charge peak) |
| **Breakdown** | Mid pose that shapes spacing between extremes |
| **Extreme** | Peak of an arc (highest swing, deepest squat) |
| **Anticipation pose** | Wind-up opposite the main action |
| **Contact pose** | Foot/hand/weapon hits ground or target |
| **Passing pose** | Mid-stride opposite limbs pass |

Prefer **pose-to-pose**: block extremes → breakdowns → polish spacing. Use straight-ahead only for continuous secondary flourishes.

## Motion principles (Disney 12 + related)

| Principle | Use when authoring |
| --- | --- |
| **Squash and stretch** | Impact, land, soft props — keep volume; subtle on armored characters |
| **Anticipation** | Attacks, jumps, casts — telegraph before commit |
| **Follow-through** | Hair, cloth, weapon tip, free hand after stop |
| **Overlapping action** | Body parts arrive at different times (hips → spine → arms) |
| **Ease in / ease out (slow in/out)** | Soft starts/stops; avoid robotic linear hits unless mechanical |
| **Arcs** | Limbs and weapons travel on curves, not chords |
| **Secondary motion** | Cloak, pouch, bow limbs after primary body |
| **Timing / spacing** | Same pose path; close keys = slow, wide keys = fast |
| **Exaggeration** | Readable silhouettes over micro-realism |
| **Appeal / staging** | Clear shape; camera-facing read in Studio screenshots |
| **Straight ahead vs pose-to-pose** | Pose-to-pose for combat/locomotion; straight-ahead for trails |

## Curves & interpolation

Linear · Bezier · Constant (step) · Ease in · Ease out · Ease in-out · Tangent · Animation curve · Interpolation · Extrapolation

Studio/glTF path today is primarily **LINEAR/STEP TRS** keys — shape motion with **key placement and spacing**, not dense curve editing. Holds = repeated keys or flat spacing; snaps = close extremes.

## Rigging terms

Skeleton · Armature · Bone · Joint · Root bone · IK · FK · Controller · Constraint · Weight painting · Skinning · Bind pose · Rest pose

Studio edits **joints/channels** on the skinned subject (and held skins). See sagittal RH→LH channel notes in Animation Studio docs.

## Character movement

| Term | Meaning |
| --- | --- |
| **Root motion** | Clip drives character translation/rotation |
| **In-place** | Feet cycle; gameplay/code moves the entity |
| **Locomotion** | Walk/run/strafe cycles |
| **Strafing / turn-in-place** | Lateral or yaw without full travel cycle |
| **Blend space / directional blend** | Parameterized direction mixes |
| **Walk strafe (project)** | In-place left/right walk cycles on `blendTree2D` (`moveX`/`moveZ`); capsule still code-driven — no extra root X in the clip |
| **Run strafe (project)** | Same tree at run radius: `RunStrafeLeft`/`RunStrafeRight` at `(-1,0)`/`(1,0)` with stronger hip CoM (±0.12) + mild Spine roll lean into travel |
| **Motion matching** | Pose search (not primary workflow here) |
| **Pivot** | Weight shift / direction change |
| **Foot plant** | Locked contact; reduces sliding |
| **Foot sliding** | Contact foot drifts — fix plant or root |
| **Hip translation / CoM** | Vertical bob and mass shift for weight |

Gameplay drive: play-test sets facing-relative `moveX` / `moveZ` from horizontal velocity (TICKET-0282). Author walk strafe from Walk (hip ±0.08); run strafe from Run (hip ±0.12 + spine lean) via MCP `create_clip` → `edit_clip` → `upsert_keys` → `save_override`. Keep cycle phase with the cloned source.

## Animation states

Entry · Exit · Transition · Blend · Layer · Additive · Override layer · State machine · Transition duration · Interrupt · Crossfade

Gameplay drives parameters/triggers via Lua; C++ owns the graph ([DEC-0022](../decisions/index.md#dec-0022-c-animator-backend-with-lua-drive-api)).

## Combat animation

| Term | Meaning |
| --- | --- |
| **Wind-up / startup** | Pre-hit telegraph |
| **Active frames** | Hit volume live / damage window |
| **Recovery** | Post-hit vulnerability / settle |
| **Cancel window** | When another action may interrupt |
| **Hitstop / hitstun** | Freeze or stun on connect |
| **Knockback / launch** | Displacement of target |
| **Combo / finisher** | Chained attacks; ender |
| **Charge attack** | Hold → peak → release |

Align timeline **events** (VFX, sfx, hit enables) with active frames — not only with visual extremes.

## Physics & secondary motion

Jiggle / spring bone · Cloth · Hair · Ragdoll · Dynamic bone · Secondary animation · Procedural animation · Physics blend

Authored clips should leave room for secondary systems; do not key every cloak fold if physics will own it later.

## Facial animation

Blend shape / morph target · Viseme · Lip sync · Eye blink / saccade · Brow · Smile / frown · Expression · FACS

Use when face channels exist; body silhouette still carries most mid-distance read.

## Camera animation

Dolly · Pan · Tilt · Orbit · Zoom · Crane · Shake · Tracking · Follow · Cinematic

Character clips assume orbit/follow gameplay cameras — favor **silhouette clarity** over cinematic-only poses unless authoring a cutscene.

## Technical terms

Animation event / notify · Trigger · Parameter · State machine · Blend tree · Animation graph · Avatar mask · Retargeting · Baking · Sampling rate · Compression

Studio: timeline events on the controller; clip overrides on disk via **Save Override** / MCP `save_override`.

## Authoring checklist (Studio / MCP)

1. State **intent**, **weight**, **energy**, and **speed** in one line.
2. Block **anticipation → extreme → settle** (combat) or **contact → passing → contact** (locomotion).
3. Check **silhouette** and **limb overlap** at mid distance (`seek` + screenshot).
4. Fix **foot plant** / hip bob before polishing fingers.
5. Place **events** on gameplay windows (release, footstep, hit).
6. **`save_override`**; verify loop seams and transitions to Idle.

## Related

- Skill (author clips): [`../../skills/author-character-animation/SKILL.md`](../../skills/author-character-animation/SKILL.md)
- Animation Studio: [`../features/animation-studio.md`](../features/animation-studio.md)
- Animator runtime: [`../features/animator.md`](../features/animator.md)
- Live MCP skill: [`../../skills/live-editor-mcp/SKILL.md`](../../skills/live-editor-mcp/SKILL.md)
- MCP-first rule: [`.cursor/rules/animation-studio-mcp-first.mdc`](../../.cursor/rules/animation-studio-mcp-first.mdc)
