# UI Canvas Assets (`*.uicanvas.json`)

Status: active (schemaVersion 1) — [DEC-0025](../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp)

Author UI layouts in a fixed **design resolution** (default 1920×1080). At runtime the canvas **scales uniformly** into the Game viewport using `scaleMode`.

Default sample path: `assets/ui/player.uicanvas.json`.

## Shape

```json
{
  "schemaVersion": 1,
  "id": "player_hud",
  "designResolution": [1920, 1080],
  "scaleMode": "letterbox",
  "widgets": [
    {
      "id": "player_health",
      "type": "bar",
      "anchor": "top_left",
      "offset": [48, 56],
      "size": [420, 28],
      "bind": "player.health",
      "maxBind": "player.healthMax",
      "label": "Health"
    }
  ]
}
```

### Canvas fields

| Field | Meaning |
| --- | --- |
| `designResolution` | `[width, height]` design pixels (required positive) |
| `scaleMode` | `letterbox` (default) or `fill_edges` (cover; may crop via viewport clip) |

### Widget types

| Type | Bind | Notes |
| --- | --- | --- |
| `bar` | number + optional `maxBind` | Fill bar |
| `text` | text/number | Authored `text` seeds bind |
| `panel` | — | Solid fill; optional `image` |
| `button` | required | Activates → Lua `uiButtons` / `on_ui_button` |
| `toggle` | bool (required) | Checkbox + label; activate flips |
| `slider` | number (required) + optional `maxBind` | Track + thumb; arrows / click adjust |
| `image` | — | Decorative; uses `image` path (GPU texture via `UiTextureCache`) |

Coordinates are in **design pixels**.

Optional style / state / image fields:

| Field | Meaning |
| --- | --- |
| `color` | `[r,g,b,a]` 0–255 fill (panels/buttons) or text (labels). Wins over tokens |
| `themeRole` | Role id from `assets/ui/ui-theme.json` (`primaryButton`, `title`, …) |
| `colorToken` | Theme token id used when `color` is unset |
| `textColor` / `textColorToken` | Button/toggle label color (never copies fill) |
| `opacity` | 0–1 draw alpha multiplier (default 1) |
| `visible` | Authored show/hide (default true); Lua `hud_set_visible` can still toggle |
| `enabled` | Active/inactive (default true); inactive draws dimmed; alias `active` |
| `text` | Authored default / label string |
| `textAlign` | `left` / `center` / `right` |
| `textVAlign` | `top` / `middle` / `bottom` |
| `fontSize` | Design-space font size |
| `fitText` | When true, shrink the drawn font (down to `minFontSize`) so wrapped copy fits the padded content rect |
| `minFontSize` | Design-space floor for `fitText` (default 11 when omitted) |
| `padding` | Design-space content insets `[left, top, right, bottom]` for text wrap/clip and button labels (keeps copy inside ornate plate chrome) |
| `image` | Project-relative texture path (e.g. `assets/ui/textures/btn.png`) |
| `imageBind` | Optional bind key for a runtime image path (`hud_set_image` / `ui_canvas_set_image`). When set, the bound path overrides authored `image`; empty bind value draws no image |
| `imageMode` | `stretch` (default) or `contain` within widget rect |
| `tooltip` | Optional hover chip text (iron panel + gold rim). Inventory `inventory.select.*` slots prefer live item name/kind **plus catalog stats** (damage range, DPS, modifiers, heal) from `InventoryRuntime` |

**Image draw (TICKET-0164):** when `image` is set, the runtime loads the PNG through `UiTextureCache` (WIC → D3D12 SRV on the ImGui heap) and draws it with `stretch` or `contain`. Missing files fall back to a purple placeholder with the filename stem.

Structural MCP edits: `engine_ui_canvas_mutate`.

Lua value helpers: `engine.hud_set_number` / `hud_set_bool` / `hud_get_bool` / `hud_set_text` / `hud_set_image` / `hud_set_visible` / `hud_set_enabled`. Modal canvases: `engine.ui_canvas_set_text` / `ui_canvas_set_text_typed(canvasId, bind, text, charsPerSec?)` / `ui_canvas_skip_typewriter(canvasId, bind)` / `ui_canvas_set_image(canvasId, bind, path)` / `ui_canvas_set_offset` / `ui_canvas_set_size` / `ui_canvas_set_opacity` (runtime cinematic overrides; cleared on canvas reload).

## Responsive draw

**Letterbox** (`scaleMode: letterbox`):

```
scale = min(view_w / design_w, view_h / design_h)
content rect = centered (design_w*scale × design_h*scale) inside the viewport
```

**Fill edges** (`scaleMode: fill_edges`):

```
scale = max(view_w / design_w, view_h / design_h)
content rect = centered; may extend past viewport (clipped on draw)
```

### Safe margin (play HUD + dialogue)

Author combat HUD and dialogue modal chrome against a shared **40 design-px** inset from the 1920×1080 letterbox edges (corners, hotbar baseline, dialogue plate). Keep nested content padding consistent inside plates (~20–28 px) via widget `padding: [L,T,R,B]` so copy stays inside ornate frames. Prefer `imageMode: contain` for square ring/slot art; use `stretch` only when the widget aspect closely matches the PNG (resource-bar rails, quest plate, prompt strip).

Size text widgets to the plate/card content box, then use `padding` for inner margin — do not rely on overflowing the clip rect. For long runtime copy (dialogue body, card descriptions), set `fitText: true` so the runtime shrinks the font to stay inside the padded box.

## Sample canvases

| Id | Path |
| --- | --- |
| HUD | `assets/ui/player.uicanvas.json` |
| pause | `assets/ui/pause.uicanvas.json` |
| main_menu | `assets/ui/main_menu.uicanvas.json` |
| opening_fade | `assets/ui/opening_fade.uicanvas.json` |
| prologue | `assets/ui/prologue.uicanvas.json` |
| character_creation | `assets/ui/character_creation.uicanvas.json` |
| character_creation_difficulty | `assets/ui/character_creation_difficulty.uicanvas.json` |
| character_creation_appearance | `assets/ui/character_creation_appearance.uicanvas.json` |
| settings | `assets/ui/settings.uicanvas.json` |
| inventory | `assets/ui/inventory.uicanvas.json` |
| dialogue | `assets/ui/dialogue.uicanvas.json` |

## Editing

- Editor UI tab: open any `assets/ui/*.uicanvas.json`, create screens, drag/resize, set `scaleMode`, Add toggle/slider/image
- MCP: `engine_hud_apply` with path ending in `.uicanvas.json` + `source`
- Scene plan: `.uicanvas.json` → `ui_canvas`
- Legacy: `*.hud.json` still validates; runtime loads it as a canvas with default 1920×1080 letterbox

## Related

- Feature: [`../features/ui-canvas.md`](../features/ui-canvas.md)
- Legacy HUD: [`hud-assets.md`](hud-assets.md)
