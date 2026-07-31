---
name: blockbench-mesh-authoring
description: >-
  Author and refine low-poly meshes in Blockbench (characters, props, weapons)
  from concept orthographics. Covers primitive assembly, proportions, symmetry,
  primary→tertiary form, topology/loop cuts, UV awareness, procedural bbmodel
  scripts, and iterative screenshot feedback. Use when modeling or reshaping
  in Blockbench MCP, building Player_V3 / kit meshes, moving vertices to match
  turnaround art, or writing tools/art/*/build_*.py generators.
---

# Blockbench Mesh Authoring

Build and refine **geometry in the live Blockbench session via MCP tools** so silhouettes match concept art **before** bake/import.

**MCP-first (required):** `place_cube` / `place_mesh` / `create_cylinder`, `extrude_mesh`, `subdivide_mesh`, `knife_tool`, `select_mesh_elements`, `move_mesh_vertices`, ortho screenshots. See [`.cursor/rules/blockbench-mcp-model-first.mdc`](../../.cursor/rules/blockbench-mcp-model-first.mdc).

**Read with:** [`context/art/character-direction.md`](../../context/art/character-direction.md), [`context/art/visual-direction.md`](../../context/art/visual-direction.md), [`.cursor/rules/blockbench-prefer-meshes.mdc`](../../.cursor/rules/blockbench-prefer-meshes.mdc).

**Not this skill:** runtime bake/prefab wiring — use [`import-blockbench-models`](../import-blockbench-models/SKILL.md) or [`import-player-character`](../import-player-character/SKILL.md) after the mesh is approved.

## Checklist

```
Author / refine (MCP):
- [ ] Lock reference orthos (front / side / back) — match front first, then side depth
- [ ] Block primary volumes in Blockbench MCP before face detail
- [ ] Symmetry across character mid-plane; both limbs present and mirrored
- [ ] Add loop cuts (subdivide / knife / extrude) where silhouette needs control verts
- [ ] Move vertices to fit concept (side profile especially)
- [ ] Screenshot ortho front + side vs concept; iterate in MCP
- [ ] Save live project to tools/art/... ; fold into a generator only after owner-approved shape
- [ ] No textures until owner asks (unless painting is the task)
```

## Scripting (secondary only)

Procedural `tools/art/*/build_*.py` generators are **optional after** MCP modeling to record an approved mesh — not a substitute for live extrude/move/subdivide passes. Do not regenerate-and-reload over MCP polish unless the owner asks.

## Foundational geometry and shapes

### Primitive assembly

Combine **cubes, cylinders (faceted), spheres/lofts, and extruded meshes** into complex forms. Prefer:

| Need | Prefer |
| --- | --- |
| Boxy / planar kit pieces (belt, pouch, boot cuff) | Cube or box mesh |
| Limbs, necks, tapered shafts | Faceted cylinder / ellipse cylinder (6–8 sides) |
| Head, torso silhouette from orthos | Lofted rings (asymmetric front/back Z) |
| Curves (bows, organic props) | Continuous **mesh** + extrude / move verts — see mesh preference rule |

Do not invent a second body when kit layers (tunic, wraps, boots) can sit on a shared base.

### Scale and proportions

- Keep a single unit space; for humanoids match existing player BB scale (~40 units tall, feet at **y=0**) unless the owner sets another target.
- Measure concept ratios: head∶torso∶leg, shoulder width, arm length, foot length from **side** and **front**.
- Stay inside sensible bounds; avoid tiny floating islands and huge detached pieces.
- Record target height / feet origin in the generator script header when using procedural builds.

### Symmetry and alignment

- Humanoids: mirror **+X / −X** (left/right). Build one side, mirror, or run a paired `build_arm(±1)` / `build_leg(±1)`.
- Align joints on shared axes (shoulders same Y; wrists same Y for T-pose; ankle/foot soles on y=0).
- T-pose: arms along ±X at shoulder height; **palms down** (hand thin in Y, fingers spread in Z) unless the owner overrides.
- Check both arms/legs exist after every regenerate — never ship a one-armed blockout.

## Technical topology and form

### Primary → tertiary forms

1. **Primary:** head capsule, torso mass, limb shafts, feet — readable silhouette only.
2. **Secondary:** tunic flare, sleeves, forearm wraps, belt, boot shafts.
3. **Tertiary:** brows, nose, pouch, belt knot, finger separation, cuff trim.

Do not start with fingers or face micro-boxes before the side silhouette reads.

### Mesh optimization (this project)

- Style is **low-poly faceted** ([DEC-0006](../../context/decisions/index.md#dec-0006-smooth-low-poly-art-direction)): visible planar faces, flat shading.
- Prefer enough rings/sides for the concept silhouette — not subdivision-smooth high poly.
- Add density with **loop cuts / extra loft rings / `subdivide_mesh` on selected faces**, then **move vertices** — do not subdivide the whole character blindly.
- Clean exports: no interior junk faces, no flipped normals, groups named for future rig bones.

### Loop cuts and vertex moves

When the silhouette is close but wrong in profile:

1. Add control loops (extra loft stations, knife/subdivide on the problem band).
2. Select verts on the front (−Z) or back (+Z) of that ring.
3. Move them to match the concept **side** panel (chin inset, chest depth, heel, etc.).
4. Re-screenshot orthographic side + front.

### UV unwrapping and texturing

- Default authoring pass: **mesh only** unless the owner asks for paint/atlas.
- Face convention for player base (owner): **eyes + mouth painted** on atlas later; **eyebrows + nose modeled** as geo.
- When UVs matter: keep islands padded; avoid stretching on large flat tunic fronts; Blockbench auto-UV is fine for first kits — refine before final bake.
- Do not invent a second “eye cube” mesh if eyes are meant to be painted.

## Scripting and tool automation

### Parametric / code generation

Prefer **live MCP modeling** in Blockbench. Optional regenerable scripts under `tools/art/<slug>/` may **record** an owner-approved mesh afterward — they must not be the primary sculpting loop.

- Keep named meshes + outliner groups ready for rigging.
- Do not regenerate-and-reload over MCP vertex polish unless the owner asks.
- Small `risky_eval` helpers (list verts by band, save) are fine; whole-character generation is not.

### Iterative refinement

```
concept orthos → block primary → screenshot front/side →
compare → loop cuts / move verts / regen script → screenshot again →
owner feedback → next pass
```

- Always compare **LEFT/RIGHT side** turnaround panels for depth (front alone lies about Z).
- Accept visual or textual feedback (lumpy meshes, missing limb, wrong palm axis) and fix in the same session.
- Stop and ask if the concept might intentionally diverge from an existing baked player (V2 vs V3) rather than silently forking proportions.

## Project anchors

| Topic | Where |
| --- | --- |
| Player / kit look | `context/art/character-direction.md`, `context/art/reference/*turnaround*.png` |
| Mesh preference | `.cursor/rules/blockbench-prefer-meshes.mdc` |
| Asset backlog | `context/art/blockbench-asset-list.md` |
| Player V3 generator | `tools/art/player/build_player_v3_ashfell.py` |
| Bake after approval | `skills/import-player-character/SKILL.md` |

## Done bar

Orthographic front + side read against concept; symmetry OK; primary→secondary forms clear; no accidental eye meshes when eyes are paint-only; script or bbmodel saved under `tools/art/`; owner can iterate without a bake.
