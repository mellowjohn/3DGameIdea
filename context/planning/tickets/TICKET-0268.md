# TICKET-0268: Ashfell 3-hit light attack chain

- Epic: EPIC-0011
- Status: active
- Agent: composer
- Priority: P2

## Goal

Ship a Souls-lite Ashfell one-handed light string: Attack → Attack2 → Attack3 with buffered LMB cancel windows, and gate play-test combat probes on each swing’s `hitFrame`.

## Context links

- `context/features/animation-studio.md`, `context/features/animator.md`, `context/features/gearing-system.md`, `context/features/combat-volumes.md`
- DEC-0048 (simple action combat; shared anims over per-weapon trees)
- Sample: `samples/open-world-rpg/assets/animators/player.animator.json`
- Clips: `player.Attack.anim.json`, `player.Attack2.anim.json`, `player.Attack3.anim.json`

## Acceptance criteria

- [x] `Attack2` / `Attack3` overrides + controller states `attack2` / `attack3`
- [x] Animator: narrowed entry into `attack`; `attack`→`attack2`→`attack3` triggers; exitTime recover; `hitFrame` on each
- [x] Play-test LMB click-buffer advances the combo while `one_handed`+`melee` hotbar gate
- [x] `hitFrame` arms ~0.12s forward sphere probe that dispatches combat contacts (hit-once per swing)
- [x] Animator suite covers combo transitions, pose-matched handoffs, distinct starts, and wrist snap-back regression
- [ ] Rebuild `engine`; desktop play-test LMB ×3 with Ashfell sword

## Out of scope

- Stamina cost on light swings, lock-on, heavy attacks, per-weapon unique trees

## What changed

- Re-authored all three player attacks via Animation Studio MCP with exaggerated hip/chest counter-rotation, vertical compression, distinct trajectories, and pose-matched handoffs: Attack diagonal → Attack2 horizontal backhand → Attack3 overhead cleave.
- Animator graph: triggers `attack2`/`attack3`; narrowed `attack` entry (idle/locomotion/land/block); combo transitions; `hitFrame` @ 0.52/0.22/0.58; fall exits for combo states.
- Play-test: buffered clicks advance through the current three-hit string; every advance requires a distinct press edge, so one click cannot queue Attack2. Combat probes remain `hitFrame`-gated and hit-once.
- Held-input transition triggers are one-shot per source state, preventing a duplicate Attack2 trigger from leaking across a crossfade and skipping Attack1 on the next string.
- Animator suite: combo transitions plus shipped override continuity/distinctness/finisher/wrist-snap regression.
- Docs: animation-studio, animator, gearing-system, combat-volumes; epics TICKET-0268.

## Agent notes

Editor + MCP reset after rebuild; lease released. Desktop: Game tab with Ashfell on hotbar — LMB ×3 during cancel windows to verify string.
