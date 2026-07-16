# HUD Toolkit

Status: active (v1) — [DEC-0024](../decisions/index.md#dec-0024-mcp-hud-toolkit-with-lua-value-binds) · [TICKET-0153](../planning/tickets/TICKET-0153.md)

Destination UI model: [DEC-0025](../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp) / [`ui-canvas.md`](ui-canvas.md) (`*.uicanvas.json` stack). This toolkit remains the shipped stepping stone until migration (TICKET-0155).

Agents edit HUD **layout** as `*.hud.json` and HUD **values** from Lua, without rebuilding C++ for new bars/text.

## Ownership

| Layer | Owner |
| --- | --- |
| Widget primitives (`bar` / `text` / `panel`) | C++ `HudRuntime` overlay on Game viewport |
| Layout asset | Prefer `assets/ui/*.uicanvas.json` via `engine_hud_apply`; legacy `*.hud.json` shim |
| Values | Lua `engine.hud_*` / `set_health` |

## Live loop

1. Edit `player.uicanvas.json` with `engine_hud_apply` (works during play test).
2. Edit combat/heal rules in Lua with `engine_lua_apply`.
3. Start play test → HUD draws over Game viewport; walk into campfire / take hurt contacts.

## Lua API (adds to host API v1)

| Lua | Behavior |
| --- | --- |
| `engine.hud_set_number(bind, value)` | Set numeric bind used by bars/text/sliders |
| `engine.hud_set_bool(bind, value)` | Set bool bind used by toggles |
| `engine.hud_get_bool(bind)` | Read bool bind (default false) |
| `engine.hud_set_text(bind, text)` | Set string bind |
| `engine.hud_set_visible(widget_id, bool)` | Show/hide widget by id |
| `engine.hud_set_enabled(widget_id, bool)` | Active/inactive (inactive draws dimmed) |
| `engine.set_health(current, max)` | Sugar for `player.health` / `player.healthMax` / `player.healthText` |
| `engine.get_health()` | Returns `current, max` |

## Sample

- Layout: `samples/open-world-rpg/assets/ui/player.uicanvas.json`
- Damage: `assets/scripts/combat_hurt.lua` (−10 HP)
- Heal: `assets/scripts/campfire_interaction.lua` (+15 HP on enter)

## Verification

- `hud` and `scripting` suites
- Format: [`../formats/hud-assets.md`](../formats/hud-assets.md)
