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
| `image` | — | Decorative; uses `image` path (placeholder until GPU textures) |

Coordinates are in **design pixels**.

Optional style / state / image fields:

| Field | Meaning |
| --- | --- |
| `color` | `[r,g,b,a]` 0–255 fill/text color |
| `opacity` | 0–1 draw alpha multiplier (default 1) |
| `visible` | Authored show/hide (default true); Lua `hud_set_visible` can still toggle |
| `enabled` | Active/inactive (default true); inactive draws dimmed; alias `active` |
| `text` | Authored default / label string |
| `textAlign` | `left` / `center` / `right` |
| `textVAlign` | `top` / `middle` / `bottom` |
| `fontSize` | Design-space font size |
| `image` | Project-relative texture path (e.g. `assets/ui/textures/btn.png`) |
| `imageMode` | `stretch` (default) or `contain` within widget rect |

**Image draw (TICKET-0164 MVP):** when `image` is set, the runtime draws a distinct purple placeholder with the filename stem as a label. GPU texture upload (WIC/stb + D3D12/ImGui) is a documented follow-on.

Structural MCP edits: `engine_ui_canvas_mutate`.

Lua value helpers: `engine.hud_set_number` / `hud_set_bool` / `hud_get_bool` / `hud_set_text` / `hud_set_visible` / `hud_set_enabled`.

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

## Sample canvases

| Id | Path |
| --- | --- |
| HUD | `assets/ui/player.uicanvas.json` |
| pause | `assets/ui/pause.uicanvas.json` |
| main_menu | `assets/ui/main_menu.uicanvas.json` |
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
