# HUD Assets (`*.hud.json`)

Status: active (schemaVersion 1) — [DEC-0024](../decisions/index.md#dec-0024-mcp-hud-toolkit-with-lua-value-binds)

Legacy play-test HUD layouts. **Prefer** [`ui-canvas-assets.md`](ui-canvas-assets.md) (`*.uicanvas.json`, DEC-0025). Sample migrated to `assets/ui/player.uicanvas.json`; `player.hud.json` remains as a load shim.

## Shape

```json
{
  "schemaVersion": 1,
  "id": "player_hud",
  "widgets": [
    {
      "id": "player_health",
      "type": "bar",
      "anchor": "top_left",
      "offset": [24, 24],
      "size": [220, 18],
      "bind": "player.health",
      "maxBind": "player.healthMax",
      "label": "Health"
    },
    {
      "id": "player_health_text",
      "type": "text",
      "anchor": "top_left",
      "offset": [24, 48],
      "size": [220, 16],
      "bind": "player.healthText"
    }
  ]
}
```

## Widget types (v1)

| type | Required fields | Notes |
| --- | --- | --- |
| `bar` | `id`, `bind`, `size` | Fill = `bind / maxBind` (maxBind optional, default 100) |
| `text` | `id`, `bind`, `size` | Shows text bind, or number bind as string |
| `panel` | `id`, `size` | Background rect only |

Anchors: `top_left`, `top_right`, `bottom_left`, `bottom_right`, `center`. Invalid types/missing binds fail validation closed.

## Editing

- MCP: `engine_hud_apply` with `path` + `source` (hot reload allowed during play test)
- Scene plan: `.hud.json` → `hud_asset`
- Lua binds values: see [`../features/hud-toolkit.md`](../features/hud-toolkit.md)
