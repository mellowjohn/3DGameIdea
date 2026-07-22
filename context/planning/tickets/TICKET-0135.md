# TICKET-0135: Animation tools panel (Diagnostics-adjacent)

- Epic: EPIC-0009
- Status: ready
- Agent: unassigned
- Priority: P2
- Notion: (see Notion Tickets DB by Ticket ID)

## Goal

Give authors an **Animation** editor surface in the existing bottom-center panel (shared slot with **Diagnostics**) to browse animator controllers, inspect clip references and rig metadata, and run a **text/headless preview** of controller playback — without adding a new viewport tab or replacing Diagnostics.

## Context links

- `context/planning/epics.md` (EPIC-0009 / EPIC-0008)
- [`context/features/animator.md`](../../features/animator.md) — shipped runtime + M5 exit CLI; this ticket adds editor panel
- [`context/formats/animator-controller-assets.md`](../../formats/animator-controller-assets.md)
- [`context/formats/animation-clip-assets.md`](../../formats/animation-clip-assets.md)
- [`context/formats/rig-assets.md`](../../formats/rig-assets.md) (TICKET-0106 metadata)
- [`context/architecture/content-vs-engine-workflows.md`](../../architecture/content-vs-engine-workflows.md) — C++ editor UI
- Implementation hooks: `draw_editor()` Diagnostics slot (`src/rendering/render_app.cpp` ~7965–8048); reuse `collect_asset_paths`, `draw_asset_path_combo`, `AnimatorControllerAsset::load`, `RigAsset::load`, `run_animation_preview` / `AnimatorRuntime`

## Acceptance criteria

- [ ] **Panel chrome:** Bottom-center editor window (same geometry slot as Diagnostics today) uses an `ImGui` tab bar with **Diagnostics** and **Animation** tabs; selecting **Animation** shows the new tools; **Diagnostics** tab preserves today's content unchanged.
- [ ] **Controller browser:** Dropdown (or selectable list) of all project `*.animator.json` paths (via existing asset scan / `collect_asset_paths` pattern); empty project shows explicit empty state, not a crash.
- [ ] **Controller inspect (read-only):** Selecting a controller loads `AnimatorControllerAsset` and displays at minimum: `id`, layer names, state names per layer, parameter names/types, and `timelineEvents` count. Invalid/missing file shows a visible error string (fail-closed).
- [ ] **Clip references:** For the selected controller, list each motion's `clipSource` + clip name (or blend-tree child clips). Missing clip file or unresolved clip name surfaces diagnostics text (reuse animator fail-closed behavior where applicable).
- [ ] **Rig inspect (read-only):** Dropdown of project `*.rig.json` assets; loading via `RigAsset::load` displays `ik_hooks` (id + joint fields) and `bone_roles` (role + joint). Invalid rig shows structured error text.
- [ ] **Preview controls (no GPU skinning required):** Animation tab provides Play / Pause / Step (or equivalent) that advances an `AnimatorRuntime` instance (or calls `run_animation_preview` for the selected controller) and shows **current state name** + active clip weights as text. If viewport mesh pose is unavailable, UI labels preview as headless/text-only (no proxy cube requirement).
- [ ] **Lookup UX:** Controller and rig pickers use dropdown/combos per lookup-fields rule (not free-text id boxes); include `(none)` when optional.
- [ ] **Docs:** Update [`context/features/animator.md`](../../features/animator.md) — move panel from Out of scope to shipped/ active editor path; note relationship to `engine animation-preview` CLI.
- [ ] **Build / validate:** Rebuild `engine`; `engine validate --project samples/open-world-rpg` passes.

## Out of scope

- GPU skinning or viewport mesh pose playback (deferred until skinning lands)
- IK solver authoring or editing rig/controller assets on disk (read-only v1)
- Removing or relocating the Diagnostics feed (errors, interaction/combat traces, Project Sync)
- New Scene/Game/World Forge viewport tab
- World Forge, water, rigidbody, or UI canvas work
- Replacing Scene/Prefab Inspector animator fields (they remain; this panel is additive)

## Dependencies

- **Soft gate (pickup):** Owner approval of **TICKET-0110** (M5 exit) on branch `cursor/miniaudio-ticket-0107-2b3b` / PR #8 — implementation should not start until 0110 is `done` or owner explicitly overrides.
- **Parallel OK:** TICKET-0107 audio (needs-approval on same PR).
- **Schema:** TICKET-0106 rig metadata (`*.rig.json`) — read-only display only; no dependency on 0106 owner approval for read path.

## Verification

**Headless / CI (agent must run before `needs-approval`):**

```powershell
# Rebuild engine target after C++ changes
engine validate --project samples/open-world-rpg --json
```

**Owner-desktop (required for approval):**

```powershell
engine editor --project samples/open-world-rpg
```

1. Open bottom-center panel → **Animation** tab visible beside **Diagnostics**.
2. Select `assets/animators/example.animator.json` → states/parameters/timeline event count visible.
3. Clip list shows references into `assets/models/player_clips.gltf` (or sample controller paths).
4. Select `assets/characters/player.rig.json` (or sample rig) → IK hooks / bone roles visible read-only.
5. Preview Play/Step advances state text (e.g. toward `attack` when trigger fired) without editor crash.
6. **Diagnostics** tab still shows error feed / interaction traces as before.

Optional smoke: `engine editor --project samples/open-world-rpg --frames 2 --hidden true --json` (GPU integration label).

Fill **What changed** on repo stub + Notion before `needs-approval`.

## Agent notes

Promoted **`proposed` → `ready`** 2026-07-22 via `write-engine-ticket` while owner deferred desktop QA. Acceptance written for C++ editor work in `render_app.cpp`; reuse existing asset/animator APIs — do not invent parallel loaders.

**PR #8 evaluate (read-only, 2026-07-22):**

| Ticket | Automated evidence on branch | Desktop still needed |
| --- | --- | --- |
| TICKET-0110 | `m5-exit`, `animation-preview` CLI + sample assets documented | Editor inspector smoke |
| TICKET-0107 | `audio` suite + sample WAV + Lua trigger | Live device / editor play SFX |

Verdict: keep both at **`needs-approval`** until owner runs desktop QA; do not start **0135** implementation until **0110** is `done` unless owner overrides.
