# UI Canvas Stack

Status: active (toggle/slider, scale modes, samples) — [DEC-0025](../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp) · EPIC-0007 (TICKET-0155–0164)

Format docs: [`../formats/ui-canvas-assets.md`](../formats/ui-canvas-assets.md).

## Stack (TICKET-0156)

- C++ `UiCanvasStack`: HUD layer + modal `push` / `pop` / `show` / `hide`
- MCP: `engine_ui_stack` `{action,id,path?}`
- Lua: `engine.ui_push` / `ui_pop` / `ui_show` / `ui_hide` / `ui_top`
- Sample modals: `pause`, `main_menu`, `settings`, `inventory`, `dialogue`, `prologue`, `character_creation` (+ difficulty / appearance), `opening_fade`

## Mutate + editor (TICKET-0157 / 0158)

- MCP: `engine_ui_canvas_mutate` — `add` / `remove` / `move` / `resize` / `style` (`color`, `fontSize`, `opacity`, `visible`, `enabled`, `text`, `textAlign`, `textVAlign`, `image`, `imageMode`, `tooltip`, `themeRole`, `colorToken`, `textColorToken`, `textColor`, `clearColor`)
- Editor: **UI** viewport tab — canvas combo + **New…**, letterbox/fill-edges preview, move/resize, inspector, Save, **Add button / toggle / slider / image**
- **Use as HUD** (checked for `player.uicanvas.json`): Save applies always-on HUD; unchecked registers the canvas id as a modal/screen
- Optional widget fields: `color`, `themeRole`, `colorToken`, `textColor`, `textColorToken`, `opacity`, `visible`, `enabled`/`active`, `text`, `textAlign`, `textVAlign`, `fontSize`, `image`, `imageBind`, `imageMode`, `tooltip`
- Lua: `engine.hud_set_visible` / `engine.hud_set_enabled` / `engine.hud_set_bool` / `engine.hud_get_bool` / `engine.hud_set_image` / `engine.ui_canvas_set_image`
- Runtime layout overrides (cinematics): `engine.ui_canvas_set_offset` / `ui_canvas_set_size` / `ui_canvas_set_opacity` (and matching `HudRuntime` APIs). Prologue Ken Burns + 2D FX overlays are driven from C++ `tick_opening_still_pan` while the prologue modal is up.

## Interactive (TICKET-0159 / 0160)

- Widget types: `button` (Lua dispatch), `toggle` (bool bind), `slider` (number bind + optional `maxBind`)
- Top modal captures keyboard/gamepad/mouse in play test; focus ring on focused control
- Near-invisible image-less buttons (opacity×alpha ≤ 0.05, no `themeRole`/`colorToken`/label text) are ghost click catchers: still hittable, but omitted from keyboard focus and focus rings (fixes the gold box around the prologue dialogue plate). Labeled or themed buttons still draw plates.
- Play test: **Esc** opens `pause` when none; **Esc** / gamepad B pops; Up/Down/Tab navigate; Left/Right adjust slider; Enter/Space/gamepad A activates
- Toggle activate flips bool; slider click sets value by track X; arrows nudge by 5% of max
- Pause **Main Menu** → `main_menu` (session stays paused); Settings pushes `settings`; Resume returns to play
- Debug (play test, no modal): **I** opens inventory, **Y** opens dialogue
- `assets/scripts/ui_handlers.lua` dispatches menu / settings / inventory / dialogue binds
- **Inventory drag-drop (2026-07-30):** press+drag between `inventory.select.*` slot buttons past an 8px threshold; release on another slot calls `inventory.drag_drop` → `engine.inventory_move`. Click (no drag) still selects. Ghost outline follows the cursor while dragging. Play-test must feed `mouse_down` / `mouse_held` / `mouse_released` (not click-only) into `handle_modal_input`. World crates (`open_weapon_crate`) add `inventory.select.container.N` slots on the same modal.
- **Inventory layout cleanup (2026-07-30, polish 2026-08-20):** `inventory.uicanvas.json` aligned to `context/design/inventory-ui.pen` (equip / bag / detail columns, square Bag I–III stubs, footer-contained hotbar, gold section titles). Empty slot hit-targets no longer paint bind ids as labels (`HudRuntime::widget_display_label`). Title, Bags, and Ammo sit inside the frame / left column; long names and the properties description use `fitText` + padding.
- **Character stats (2026-08-20):** Inventory has a dedicated right-hand **Character** sheet (`inventory.statsBody`) with Core / Attributes / Offense / Defense (scrollable). Strength / Agility / Intellect, crit % + marble mix, and typed resists live there. **Rune pips** on the HUD are magicka while a magic weapon is held. Crate slots occupy the inspect column (not the Character sheet) when a world container is open.
- **Hover tooltips (2026-07-30, stats 2026-08-20):** `UiCanvasStack` draws an iron/gold chip near the cursor for authored `tooltip` and for filled/empty `inventory.select.*` slots. Filled slots show display name, kind × count, then authored **stats** (damage range, DPS, heal, modifiers) from the item catalog.
- **Authoring:** prefer live MCP (`engine_ui_canvas_mutate` / `engine_hud_apply`) over Python canvas generators — `.cursor/rules/ui-canvas-mcp-first.mdc`
- **Shared UI theme (DEC-0058):** `assets/ui/ui-theme.json` holds named tokens + roles. Inventory chrome buttons (`BAG` / `CRAFT` / `X` / `SORT` / `EQUIP` / `UNEQUIP`) use `themeRole` `primaryButton` / `secondaryButton` so gold plates get **ink** labels. MCP: `engine_asset_apply` `kind: ui_theme`. Lua: `engine.ui_theme_set_token(name, r,g,b[,a])`. Editor UI tab Theme panel + role dropdowns. Format: [`../formats/ui-theme-assets.md`](../formats/ui-theme-assets.md).

## Main menu preview vs sandbox play (2026-08-05)

Front-end screens are tested differently from gameplay. `worlds/main-menu.world.json` is a
**menu-only** world (the project `defaultWorld`), so the editor treats Play there as a menu
preview instead of a player play-test:

| | `main-menu.world.json` | any other world |
| --- | --- | --- |
| Boot | Scene tab, no preview; free authoring camera | Scene tab, no session |
| Play (F5 / toolbar / `engine_editor_session` `start`) | Menu preview: backdrop + menu canvas, **no player spawn, no gameplay sim** (`testSession` stays `inactive`) | Player spawn + locomotion + gameplay HUD |
| Gameplay HUD layer | Hidden (`UiCanvasStack::set_hud_visible(false)`) | Visible |
| Esc | Does not pop `main_menu` (it is the preview root) | Opens `pause` |
| Stop | Shift+F5 / toolbar stop / `engine_editor_session` `end` returns to the empty Game view | Ends play-test |

Menu-camera framing is applied to the Game view during an explicit preview, so it shows
the locked establishing pose rather than an orbit/player camera. Scene remains a free
authoring camera, including while the preview is active.

**Opening boot (2026-08-07):** **New Game** fades to black, loads `opening.prologueWorld` (cinematic instance throne), reveals prologue chrome over 3D with `evt_prologue_throne`, then restores the menu world for class → difficulty. Difficulty Next fades into `opening.appearanceWorld` (pedestal courtyard) under appearance chrome. See [`../story/prologue-and-opening.md`](../story/prologue-and-opening.md) and [`cinematic-instance-worlds.md`](cinematic-instance-worlds.md).

## Scale modes (TICKET-0161)

- Per-canvas `scaleMode`: `letterbox` (default) or `fill_edges` (cover; content may extend past viewport, clipped on draw)
- Settings sample: `assets/ui/settings.uicanvas.json`

## Images (TICKET-0164)

- Optional `image` + `imageMode` (`stretch` | `contain` | `nine_slice`) on widgets; dedicated `image` type (decorative)
- Optional `imageSlice` `[left, top, right, bottom]` in **design pixels** for `nine_slice` (borders scale with the canvas)
- Optional `padding` `[left, top, right, bottom]` in **design pixels** for text wrap/clip and button labels (keeps copy inside ornate plates/cards)
- Optional `fitText` (+ `minFontSize`) shrinks wrapped text to stay inside the padded content rect
- Optional `imageBind` for runtime-swappable icons (inventory slots, hotbar): `engine.hud_set_image(bind, path)` or `engine.ui_canvas_set_image(canvasId, bind, path)`; empty path clears
- Optional runtime color overrides: `engine.hud_set_color(widgetId, r,g,b[,a])` / `engine.hud_clear_color`, and `engine.ui_canvas_set_color(canvasId, widgetId, r,g,b[,a])` / `engine.ui_canvas_clear_color` (0–255 RGBA; used for hotbar selected chrome)
- Runtime: `UiTextureCache` loads PNG via WIC → D3D12 SRV on the ImGui heap (slots 256–511) and draws with `AddImageRounded`. Missing / unloadable paths still show the purple placeholder + filename stem.
- **Pre-filter on load:** ImGui's D3D12 sampler clamps to mip 0, so large chrome PNGs are area-filtered (premultiplied) down toward the widget box before upload. Entry `width`/`height` still report the source PNG size so `contain` / nine-slice UVs stay correct.
- Main menu sample: title logo + stained-glass plates under `assets/ui/menu/` (`glass-menu-*.png`, repaired `wrathful-conquest-title-logo.png`). Prefer opaque glass plates over green-keyed v2 chrome.
- Play HUD / dialogue chrome under `assets/ui/hud/` and `assets/ui/dialogue/`
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
