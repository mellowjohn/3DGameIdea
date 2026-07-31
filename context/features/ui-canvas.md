# UI Canvas Stack

Status: active (toggle/slider, scale modes, samples) — [DEC-0025](../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp) · EPIC-0007 (TICKET-0155–0164)

Format docs: [`../formats/ui-canvas-assets.md`](../formats/ui-canvas-assets.md).

## Stack (TICKET-0156)

- C++ `UiCanvasStack`: HUD layer + modal `push` / `pop` / `show` / `hide`
- MCP: `engine_ui_stack` `{action,id,path?}`
- Lua: `engine.ui_push` / `ui_pop` / `ui_show` / `ui_hide` / `ui_top`
- Sample modals: `pause`, `main_menu`, `settings`, `inventory`, `dialogue`

## Mutate + editor (TICKET-0157 / 0158)

- MCP: `engine_ui_canvas_mutate` — `add` / `remove` / `move` / `resize` / `style` (`color`, `fontSize`, `opacity`, `visible`, `enabled`, `text`, `textAlign`, `textVAlign`, `image`, `imageMode`, `tooltip`)
- Editor: **UI** viewport tab — canvas combo + **New…**, letterbox/fill-edges preview, move/resize, inspector, Save, **Add button / toggle / slider / image**
- **Use as HUD** (checked for `player.uicanvas.json`): Save applies always-on HUD; unchecked registers the canvas id as a modal/screen
- Optional widget fields: `color`, `opacity`, `visible`, `enabled`/`active`, `text`, `textAlign`, `textVAlign`, `fontSize`, `image`, `imageBind`, `imageMode`, `tooltip`
- Lua: `engine.hud_set_visible` / `engine.hud_set_enabled` / `engine.hud_set_bool` / `engine.hud_get_bool` / `engine.hud_set_image` / `engine.ui_canvas_set_image`

## Interactive (TICKET-0159 / 0160)

- Widget types: `button` (Lua dispatch), `toggle` (bool bind), `slider` (number bind + optional `maxBind`)
- Top modal captures keyboard/gamepad/mouse in play test; focus ring on focused control
- Play test: **Esc** opens `pause` when none; **Esc** / gamepad B pops; Up/Down/Tab navigate; Left/Right adjust slider; Enter/Space/gamepad A activates
- Toggle activate flips bool; slider click sets value by track X; arrows nudge by 5% of max
- Pause **Main Menu** → `main_menu` (session stays paused); Settings pushes `settings`; Resume returns to play
- Debug (play test, no modal): **I** opens inventory, **Y** opens dialogue
- `assets/scripts/ui_handlers.lua` dispatches menu / settings / inventory / dialogue binds
- **Inventory drag-drop (2026-07-30):** press+drag between `inventory.select.*` slot buttons past an 8px threshold; release on another slot calls `inventory.drag_drop` → `engine.inventory_move`. Click (no drag) still selects. Ghost outline follows the cursor while dragging.
- **Inventory layout cleanup (2026-07-30):** `inventory.uicanvas.json` aligned to `context/design/inventory-ui.pen` (equip / bag / detail columns, square Bag I–III stubs, footer-contained hotbar, gold section titles). Empty slot hit-targets no longer paint bind ids as labels (`HudRuntime::widget_display_label`).
- **Hover tooltips (2026-07-30):** `UiCanvasStack` draws an iron/gold chip near the cursor for authored `tooltip` and for filled/empty `inventory.select.*` slots (item display name + kind × count).
- **Authoring:** prefer live MCP (`engine_ui_canvas_mutate` / `engine_hud_apply`) over Python canvas generators — `.cursor/rules/ui-canvas-mcp-first.mdc`

## Scale modes (TICKET-0161)

- Per-canvas `scaleMode`: `letterbox` (default) or `fill_edges` (cover; content may extend past viewport, clipped on draw)
- Settings sample: `assets/ui/settings.uicanvas.json`

## Images (TICKET-0164)

- Optional `image` + `imageMode` (`stretch` | `contain`) on widgets; dedicated `image` type (decorative)
- Optional `imageBind` for runtime-swappable icons (inventory slots, hotbar): `engine.hud_set_image(bind, path)` or `engine.ui_canvas_set_image(canvasId, bind, path)`; empty path clears
- Optional runtime color overrides: `engine.hud_set_color(widgetId, r,g,b[,a])` / `engine.hud_clear_color`, and `engine.ui_canvas_set_color(canvasId, widgetId, r,g,b[,a])` / `engine.ui_canvas_clear_color` (0–255 RGBA; used for hotbar selected chrome)
- Runtime: `UiTextureCache` loads PNG via WIC → D3D12 SRV on the ImGui heap (slots 256–511) and draws with `AddImageRounded`. Missing / unloadable paths still show the purple placeholder + filename stem.
- Sample: main menu New Game → `assets/ui/textures/btn_new_game.png`; play HUD / dialogue chrome under `assets/ui/hud/` and `assets/ui/dialogue/`
- Play HUD + dialogue share a **40 design-px** letterbox safe margin; regenerate with `tools/generate_ui_canvases.py`

## Decisions (locked)

| Topic | Choice |
| --- | --- |
| Scope | Full UI canvases (not HUD-only) |
| Asset | `*.uicanvas.json` (migrate `*.hud.json` sample) |
| Design resolution | 1920×1080 default |
| Responsive | Uniform scale: letterbox or fill-edges cover |
| Stack | Engine-owned `push` / `pop` / `show` / `hide`; MCP + Lua equal clients |
| Authoring | Parallel MCP mutate + editor Canvas |
| Interactive | Button + toggle + slider + focus |

## Ticket order

1. **0155** — format + responsive draw + HUD migration  
2. **0156** — stack API  
3. **0157** / **0158** — MCP mutate + editor  
4. **0159** — focus + pause sample  
5. **0160** — toggle + slider  
6. **0161** — fill-edge scale + settings sample  
7. **0164** — image field (placeholder MVP; GPU textures follow-on)  
8. **0162** / **0163** — inventory + dialogue canvas samples  

## Related

- Stepping stone: [`hud-toolkit.md`](hud-toolkit.md) ([DEC-0024](../decisions/index.md#dec-0024-mcp-hud-toolkit-with-lua-value-binds))
- Live editor: [`mcp-live-editor.md`](mcp-live-editor.md)
