# Bow draw / aim / release (ranged combat)

Status: **active** (TICKET-0260) — first playable vertical slice.

## Goal

Outrider shortbow combat is a **three-state** controller graph driven by hold-to-draw input. Gameplay owns the nocked/flying arrow; animation only supplies poses and timeline events.

## State machine

Bow combat lives on the **`upperBody` overlay** (same mask as Block / Attack): spine, arms, and head. Base idle/locomotion keep the legs, so you can walk and run while drawing.

| State | Layer | Clip | Loop | Player input | Arrow |
| --- | --- | --- | --- | --- | --- |
| `bowDraw` | `upperBody` | `BowDraw` | no | LMB held (`bowDrawn=true`) | `nockArrow` event → instantiate / weld nocked prop |
| `bowAim` | `upperBody` | `BowAim` | yes | LMB still held after draw finishes | Nocked mesh follows string hand; walk/run continue on base |
| `bowRelease` | `upperBody` | `BowRelease` | no | LMB released (`bowDrawn=false`) while drawn | `releaseArrow` event → free projectile + hide nocked |
| `empty` | `upperBody` | none | — | dodge / hit / jump / unequip cancel | nock cleared **without** firing |

Parameters:

| Name | Type | Role |
| --- | --- | --- |
| `bowDrawn` | bool | True while primary is held for a ranged hotbar weapon |
| `shoot` | trigger | Legacy one-shot into `bowDraw` (compat) |

Transitions (`upperBody` layer, `player.animator.json`):

- `empty` / `block` + `bowDrawn==true` + grounded → `bowDraw`
- `bowDraw` + exitTime ≈ 0.92 + `bowDrawn` → `bowAim`
- `bowDraw` + `bowDrawn==false` (early loose) → `bowRelease`
- `bowAim` + `bowDrawn==false` → `bowRelease`
- `bowRelease` + exitTime → `empty`
- `bowDraw` / `bowAim` / `bowRelease` + `grounded==false` → `empty` (cancel, no shot)

Dodge / hit / unequip: C++ crossfades overlay to `empty` and clears `bowDrawn` **before** the animator tick so a held-LMB drop does not fire `releaseArrow`.

Timeline events:

- `nockArrow` on `upperBody` `bowDraw` @ early pull (~0.12s of `BowDraw`)
- `releaseArrow` on `upperBody` `bowRelease` @ string loose (~0.05s)

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
4. **Flying**: on `releaseArrow`, velocity is **nock → reticle look point with gravity lead** (look point = camera-ray hit distance, else ~45 m) × speed + gravity + dual trail (ImGui polyline + particle wake).
5. **Impact**: ray-cast each step against `CollisionWorld` (terrain heightfields + placement colliders when streamed); else snap when Y ≤ terrain sample + slack; else lifetime expiry. Before world impact, each step also samples `query_combat_hits_along_segment` against hurt volumes (dummy `dummy_body`, NPCs), **ignoring the shooter's placement** so the nock/muzzle start inside the player `body` hurt does not self-hit. A combat contact dispatches Lua like melee (`recent_combat_events`), plays the impact burst, and despawns. Misses still spark on ground/geometry with no damage.

### Arrow VFX assets

| Asset | Role |
| --- | --- |
| `assets/vfx/arrow_trail.particle.json` | Faint cool-white wind-rush streaks; `spawn_burst` ~1 wisp every ~0.22 m of flight, emission reverse of velocity |
| `assets/vfx/arrow_impact_flash.particle.json` | Brief additive gold flash pop on impact |
| `assets/vfx/arrow_impact.particle.json` | Bright gold/cyan spark spray on impact (+ companion `footstep_dust` puff) |

Also keeps a thin near-white translucent ImGui polyline + hollow ring as a secondary flight read. Recipes: `arrow_trail` / `arrow_impact` in `assets/vfx/recipes/vfx_recipes.json`.

Mesh: `assets/models/outrider_arrow.gltf` (local **+Z = tip**). Flight / nock rotations use `quat_look_along_direction` so +Z aligns with velocity (or look when nocked).

## How to aim

- **Mouse look** on Game viewport sets orbit **yaw/pitch** — that orientation is the aim direction into the world.
- Hold **LMB** on a `ranged` weapon to draw/aim — gold arc previews the ballistic path toward the **screen-center** reticle (look point sits on that ray at the surface under the reticle when one is hit).
- Release **LMB** to fire from the nock toward that look point, with a small gravity lead so the arc meets the reticle aim.

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
| look point | `eye + forward * hitDist` | hitDist = nearest **StaticWorld** or hurt-volume hit on the ray (min 1.75 m), else `projectile_aim_range`. Dynamic/Character capsules are ignored so OTS never pins aim on the player back (that fired bolts backwards). |
| fire dir | nock → look point (+ gravity lead) | parallax-corrected from hands onto the **reticle ray** at target distance; if look point is behind the muzzle along camera forward, push aim forward along the camera |

`tan(half_hfov)` is still updated each frame for optional off-center rays (hooks kept at zero). Left-shoulder swap deferred.

Fixed far-only convergence (always 45 m) made mid-range impacts sit left of the reticle under OTS; distance now tracks what the reticle is actually on.

Default numbers (play-test): `projectile_speed` **42**, `projectile_gravity` 4.0, `projectile_aim_range` 45 (max / miss fallback), trajectory preview ~1.5 s. Gravity lead uses the same speed so the gold arc still meets the look point mid-range.

## Authoring clips

| Override | Intent |
| --- | --- |
| `player.BowDraw.anim.json` | Nock at chest (~0.0–0.28s hold) then pull to full draw ≈ 0.95s. Bow arm (clip `Right*`) extends toward aim; string arm (clip `Left*`) wraps **outside** the ribs to the cheek — do not collapse the elbow through the torso. |
| `player.BowAim.anim.json` | Full-draw hold loop (~0.4s): string elbow beside the body (same side as the drawing hand), hand at jaw/cheek |
| `player.BowRelease.anim.json` | Same outside-ribs full-draw start → snappy string-loose along the draw path → nock/ready settle ≈ 0.35s |

## Out of scope (v1)

- Full IK string hand / nock refinement
- Undo / separate cancel-undraw clip
- Layer-filtered projectile ray (player self-hit filter); continuous attached emitter (bursts only)
- Piercing projectiles (one hurt placement per shot)

## Ammo (play-test vertical slice)

Outrider play-test / `apply_starter` grants **20** `crude_arrow`. Draw (`bowDrawn`) is blocked when ammo count is 0 (a nock already in progress may still release). `releaseArrow` / `bowRelease` consumes **1** arrow before spawning the visual projectile; if consume fails, the nock clears with no shot.

## Related

- Animation Studio held flex: [`animation-studio.md`](animation-studio.md)
- Animator format: [`../formats/animator-controller-assets.md`](../formats/animator-controller-assets.md)
- Gearing combat baseline: [`gearing-system.md`](gearing-system.md)
- Particles / bursts: [`particles.md`](particles.md)
- Ticket: [`../planning/tickets/TICKET-0260.md`](../planning/tickets/TICKET-0260.md)
