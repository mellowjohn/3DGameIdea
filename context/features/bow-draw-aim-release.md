# Bow draw / aim / release (ranged combat)

Status: **active** (TICKET-0260) — first playable vertical slice.

## Goal

Outrider shortbow combat is a **three-state** controller graph driven by hold-to-draw input. Gameplay owns the nocked/flying arrow; animation only supplies poses and timeline events.

## State machine

| State | Clip | Loop | Player input | Arrow |
| --- | --- | --- | --- | --- |
| `bowDraw` | `BowDraw` | no | LMB held (`bowDrawn=true`) | `nockArrow` event → instantiate / weld nocked prop |
| `bowAim` | `BowAim` | yes | LMB still held after draw finishes | Nocked mesh follows string hand; can walk (body pose still aim) |
| `bowRelease` | `BowRelease` | no | LMB released (`bowDrawn=false`) while drawn | `releaseArrow` event → free projectile + hide nocked |

Parameters:

| Name | Type | Role |
| --- | --- | --- |
| `bowDrawn` | bool | True while primary is held for a ranged hotbar weapon |
| `shoot` | trigger | Legacy one-shot into `bowDraw` (compat) |

Transitions (base layer, `player.animator.json`):

- `idle` / `locomotion` + `bowDrawn==true` → `bowDraw`
- `bowDraw` + exitTime ≈ 0.92 + `bowDrawn` → `bowAim`
- `bowDraw` + `bowDrawn==false` (early cancel) → `bowRelease`
- `bowAim` + `bowDrawn==false` → `bowRelease`
- `bowRelease` + exitTime → `idle`

Timeline events:

- `nockArrow` on `bowDraw` @ early pull (~0.12s of `BowDraw`)
- `releaseArrow` on `bowRelease` @ string loose (~0.05s)

## Held bow flex

`handAttach.drawClip` (`bow_draw`) still drives limb flex:

- `bowDraw` — u progresses 0→peak with state time
- `bowAim` — u held at peak
- `bowRelease` — u from peak → rest with state time

## Play-test input

Game viewport, hotbar item tagged `ranged` (Outrider shortbow):

- LMB held → `bowDrawn=true`
- LMB released → `bowDrawn=false` (graph leaves aim into release)
- Melee one-hand still uses edge `attack` trigger (unchanged)

## Arrow lifecycle (v1)

1. **Nocked** (session-only mesh): prefer welding to held shortbow **`StringMid`**; shaft aims at the **camera look point** (center reticle ray × `projectile_aim_range`, no gravity lead).
2. **Vertical aim**: while drawing/aiming, orbit pitch range widens (look up/down farther). Spine/chest/upper arms get a **procedural elevation** so arms raise/lower with camera pitch.
3. **Aim overlay**: ballistic **trajectory arc** while nocked (same gravity lead as fire) + **screen-center reticle** in the Game viewport (true look axis).
4. **Flying**: on `releaseArrow`, velocity is **nock → look point with gravity lead** (same center look ray as the reticle) × speed + gravity + dual trail (ImGui polyline + particle wake).
5. **Impact VFX (visual only)**: ray-cast each step against `CollisionWorld` (terrain heightfields + placement colliders when streamed); else snap when Y ≤ terrain sample + slack; else lifetime expiry. On end: spark + dust burst, despawn projectile, stop trail emit. Still **no damage volumes**.

### Arrow VFX assets

| Asset | Role |
| --- | --- |
| `assets/vfx/arrow_trail.particle.json` | Faint cool-white wind-rush streaks; `spawn_burst` ~1 wisp every ~0.22 m of flight, emission reverse of velocity |
| `assets/vfx/arrow_impact.particle.json` | Short gold/warm spark burst on impact (+ companion `footstep_dust` puff) |

Also keeps a thin near-white translucent ImGui polyline + hollow ring as a secondary flight read. Recipes: `arrow_trail` / `arrow_impact` in `assets/vfx/recipes/vfx_recipes.json`.

Mesh: `assets/models/outrider_arrow.gltf` (local **+Z = tip**). Flight / nock rotations use `quat_look_along_direction` so +Z aligns with velocity (or look when nocked).

## How to aim

- **Mouse look** on Game viewport sets orbit **yaw/pitch** — that orientation is the aim direction into the world.
- Hold **LMB** on a `ranged` weapon to draw/aim — gold arc previews the ballistic path toward the **screen-center** reticle (open space along look, not the skull).
- Release **LMB** to fire from the nock toward the look point (≈45 m along the ray), with a small gravity lead so the arc meets that aim at mid-range.

### Aim camera (OTS)

While play-test **orbit** camera is active (Game tab — not Scene free-cam):

| Concern | Role |
| --- | --- |
| **Orientation** (yaw/pitch) | World look / aim. `OrbitCamera::forward()` and view matrix use this axis (`LookTo`), not `LookAt` pivot. |
| **Position** (distance + shoulder) | Framing only — right of character, body left of center without pinning reticle on the head. |

While a `ranged` hotbar weapon is drawn (`bowDrawn` / LMB held):

| Param | Hipfire (camera asset) | Aim (full blend) |
| --- | --- | --- |
| `shoulder_offset` | e.g. `0.45` | blends → **`1.12`** m (over right shoulder) |
| orbit desired distance | rest / user scroll (default **`5.25`**, max **`6`**) | blends → **`4.35`** m |
| vertical FOV | asset (e.g. ~65°) | ~**8°** tighter (`base − 0.140` rad) — focus kick |
| pitch clamp | asset defaults | still widens to ~−0.85…1.15 while drawn **or** nocked |

Blend is **exponential** (`u += (target−u)*(1−e^{−dt/τ})`, τ ≈ **0.15 s**) toward 0 or 1 — continuous enter/exit without re-trigger hard steps. Rest distance tracks scroll while fully hipfire; freezes at aim-enter and holds through blend-out (scroll during aim re-bases the frozen rest). Framing runs **before** `orbit.update` each frame so shoulder/desired/FOV land on the same eye recompute.

**Why it no longer snaps:** orbit collision used to **instant-snap** `resolved_distance` whenever target was &lt; 85% of resolved — intentional ADS pull-in hit that mid-blend. Snap now applies **only when the probe actually hit** geometry; clear-path zoom-in eases at ~14/s.

### Aim reticle (screen + ray)

`OrbitCamera::forward()` is **pure yaw/pitch**. Screen center is open space along look under OTS (shoulder no longer forces LookAt-head). While nocked (or projectile trails active):

| Constant | Value | Role |
| --- | --- | --- |
| `k_bow_aim_reticle_ndc_x` / viewport x | **`0`** | Center reticle = true aim (lateral offset bandaid removed) |
| look point | `eye + forward * range` | fire, gold arc, nock shaft share this ray |
| fire dir | nock → look point (+ gravity lead) | parallax-corrected from hands, not “from camera seat” |

`tan(half_hfov)` is still updated each frame for optional off-center rays (hooks kept at zero). Left-shoulder swap deferred.

Default numbers (play-test): `projectile_speed` **42**, `projectile_gravity` 4.0, `projectile_aim_range` 45, trajectory preview ~1.5 s. Gravity lead uses the same speed so the gold arc still meets the look point mid-range.

## Authoring clips

| Override | Source phase of polished `BowShoot` (~1.7s) |
| --- | --- |
| `player.BowDraw.anim.json` | ~0.0–1.2s, retimed to length ≈ 0.95s |
| `player.BowAim.anim.json` | Full-draw pose loop (~0.4s) |
| `player.BowRelease.anim.json` | ~1.4–1.7s release/settle ≈ 0.35s |

## Out of scope (v1)

- Full IK string hand / nock refinement
- Ammo inventory consume
- Combat damage volumes on projectile
- Upper-body aim blend while walking (body stays aim clip)
- Undo / separate cancel-undraw clip
- Layer-filtered projectile ray (player self-hit filter); continuous attached emitter (bursts only)

## Related

- Animation Studio held flex: [`animation-studio.md`](animation-studio.md)
- Animator format: [`../formats/animator-controller-assets.md`](../formats/animator-controller-assets.md)
- Gearing combat baseline: [`gearing-system.md`](gearing-system.md)
- Particles / bursts: [`particles.md`](particles.md)
- Ticket: [`../planning/tickets/TICKET-0260.md`](../planning/tickets/TICKET-0260.md)
