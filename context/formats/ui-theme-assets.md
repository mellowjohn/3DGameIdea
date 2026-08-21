# UI Theme Assets (`ui-theme.json`)

Status: active (schemaVersion 1) — [DEC-0058](../decisions/index.md#dec-0058-shared-ui-theme-tokens)

Shared chrome tokens and widget roles for playable canvases. Editors and MCP rewrite this object; draw resolves colors on the next frame.

Default sample path: `samples/open-world-rpg/assets/ui/ui-theme.json`.

Token values come from [ui-chrome-direction.md](../art/ui-chrome-direction.md) (iron / gold / parchment / chrome text).

## Shape

```json
{
  "schemaVersion": 1,
  "id": "open_world_rpg_ui",
  "tokens": {
    "goldAccent": [213, 185, 120, 255],
    "ink": [72, 62, 48, 255]
  },
  "roles": {
    "primaryButton": { "fill": "goldAccent", "text": "ink", "border": "goldAccent" }
  }
}
```

| Field | Meaning |
| --- | --- |
| `tokens` | Named RGBA 0–255 arrays (`goldAccent`, `chromeText`, `ironPanel`, …) |
| `roles` | Named `{fill,text,border}` token ids. Missing token ids fail closed (`UITHEME-ROLE-TOKEN`) |

## Widget references

On `*.uicanvas.json` widgets:

| Field | Meaning |
| --- | --- |
| `themeRole` | Role id (`primaryButton`, `secondaryButton`, `title`, `goldLabel`, …) |
| `colorToken` | Token id for fill (or text on labels) when `color` is unset |
| `textColorToken` | Token id for button/toggle labels |
| `textColor` | Literal RGBA label color; wins over tokens |
| `color` | Literal RGBA fill; wins over tokens/roles |

Button labels never reuse fill. Gold plates without an explicit text color resolve to ink; iron plates resolve to chrome text.

## MCP / Lua / editor

- `engine_asset_apply` `kind: ui_theme` writes the JSON and hot-reloads the live stack
- `engine_ui_canvas_mutate` `style` can set `themeRole` / `colorToken` / `textColorToken` / `textColor` / `clearColor`
- Lua: `engine.ui_theme_set_token` / `ui_theme_get_token` / `ui_theme_set_role` / `ui_theme_save` / `ui_theme_reload`
- Editor **UI** tab: Theme panel color-picks tokens; widget inspector combos pick roles/tokens

See [`../features/ui-canvas.md`](../features/ui-canvas.md) and [`ui-canvas-assets.md`](ui-canvas-assets.md).
