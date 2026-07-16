# Visual Direction

## Accepted baseline

The game targets an Unturned-inspired blocky stylized look with a dark-fantasy atmosphere. Terrain is smooth low-poly heightfield geometry rather than stepped voxel blocks. Characters, buildings, trees, rocks, weapons, and other props use simple modular forms, readable proportions, and strong silhouettes.

Player-character proportions, starter kits, and turnaround presentation are defined in [Character Art Direction](character-direction.md), anchored by the starting Squire concept reference.

## Palette and atmosphere

- Use muted earth colors: charcoal rock, desaturated forest green, cold gray soil, worn brown timber, and subdued iron.
- Keep broad daylight readable but overcast, cool, and slightly desaturated rather than uniformly black.
- Use warm fire, lanterns, windows, and settlements as navigation landmarks and signs of safety.
- Reserve saturated colors for supernatural forces, corruption, magic, status effects, and important gameplay feedback.
- Use layered distance fog and softened distant contrast to create scale while supporting terrain LOD transitions.
- Favor strong value separation between characters, enemies, interactable objects, and terrain.
- Preserve readable silhouettes during combat; atmosphere must never obscure hit timing, hazards, or traversal edges.

## Terrain implications

- Author terrain as streamed heightfield cells with seamless shared borders.
- Keep visible polygon density deliberate and relatively coarse without compromising collision or navigation.
- Use a small set of stylized material regions such as grass, dirt, rock, snow, and corrupted ground.
- Prefer broad color shapes and restrained texture detail over photorealistic surface scans.
- Use low-poly foliage clusters, distance impostors, fog, lighting, and shadows to establish atmosphere.
- Keep render geometry, collision geometry, and navigation inputs derived from the same authoritative height data.
- Do not require editable voxel terrain or cube-by-cube runtime meshing for v1.

## Production constraint

The pipeline must remain practical for a developer without advanced modeling or texturing experience. Modular kits, reusable palettes, procedural placement, validation tools, and AI-assisted asset workflows are first-class requirements.

## Typography

Typography should reinforce dark fantasy without reducing usability. Ornamental lettering is reserved for short display text; gameplay information uses highly readable faces.

### Font roles

- **Engine / editor chrome:** Roboto (SIL OFL 1.1) — `assets/ui/fonts/roboto/`. Transparent tooling UI.
- **In-scene game UI (HUD, menus, inventory, dialogue, titles):** Cinzel (SIL OFL 1.1) — `assets/ui/fonts/cinzel/`. Player-facing game asset typography.
- **Developer console, diagnostics, IDs, and profiling:** JetBrains Mono (SIL OFL 1.1) — `assets/ui/fonts/jetbrains-mono/`.
- **In-world inscriptions:** may still use a separate stylized face or authored glyph set, but must provide a readable translation where gameplay meaning matters.

### Usage rules

- Maintain strong contrast against dark scenes and provide a subtle opaque panel, shadow, or outline where backgrounds vary.
- Do not communicate warnings, rarity, allegiance, or status through color alone; pair color with an icon or text label.
- Support UI scaling and test at 1440p from the start. Body and dialogue text must remain readable at the smallest supported scale.
- Use tabular numerals where changing values must not shift layout.
- Preserve Unicode-capable shaping and localization-ready string layouts; do not bake ordinary UI text into textures.
- Define fallback chains for missing glyphs and surface a diagnostic instead of rendering silent replacement boxes.
- Package only fonts whose recorded licenses permit commercial use, modification, and redistribution. Preserve required notices and attribution. Roboto (engine) / Cinzel (scene) / JetBrains Mono are approved and listed in [`context/resources/index.md`](../resources/index.md).

### Planned validation

- Screenshot tests cover normal, hover, disabled, warning, error, and selected states over representative dark and bright backgrounds.
- Layout tests cover long quest text, dialogue choices, large inventory counts, missing glyphs, and UI scale changes.
- The diagnostics console distinguishes severity and priority through labels in addition to color.

## Open visual decisions

- Flat shading versus softened normals on terrain and props.
- Degree of texture use versus palette-driven materials.
- Archer and Acolyte starting-kit concept references.
- Final licensed font files and fallback coverage — **approved 2026-07-15** (TICKET-0144): Cinzel for in-scene game UI; Roboto for engine chrome; JetBrains Mono for diagnostics; ImGui fallback if files missing.
