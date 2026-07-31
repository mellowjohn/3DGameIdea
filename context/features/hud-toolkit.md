# HUD Toolkit

Status: active (v1) — [DEC-0024](../decisions/index.md#dec-0024-mcp-hud-toolkit-with-lua-value-binds) · [TICKET-0153](../planning/tickets/TICKET-0153.md)

Destination UI model: [DEC-0025](../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp) / [`ui-canvas.md`](ui-canvas.md) (`*.uicanvas.json` stack). This toolkit remains the shipped stepping stone until migration (TICKET-0155).

Agents edit HUD **layout** as `*.hud.json` / `*.uicanvas.json` and HUD **values** from Lua, without rebuilding C++ for new bars/text.

## Ownership

| Layer | Owner |
| --- | --- |
| Widget primitives (`bar` / `text` / `panel`) | C++ `HudRuntime` overlay on Game viewport |
| World billboards (prompts / floating bars) | C++ `WorldUiBillboardRuntime` + Lua `world_ui_*` |
| Layout asset | Prefer `assets/ui/*.uicanvas.json` via `engine_hud_apply`; legacy `*.hud.json` shim |
| Values | Lua `engine.hud_*` / `set_health` / `set_resource` / `apply_archetype_hud` |

## Play HUD chrome (Act 0)

Shared style: [`../art/ui-chrome-direction.md`](../art/ui-chrome-direction.md) (iron / gold / parchment for all game UI).

Screen-space always-on canvas (`player.uicanvas.json`) — Dragon Age–inspired combat layout (see `context/design/player-hud.pen`):

- **Safe margin:** 40 design-px from the 1920×1080 letterbox edges
- Bottom-left circular face viewport ring (hollow PNG + fill well; placeholder until live face RT)
- `player.name` (Cinzel) beside stacked **Health** then **Stamina/Magic** bars with resource-bar chrome
- Bottom-centered hotbar slots 1–6 (ability-slot chrome; icons on 1–3)
- Top-right circular minimap frame (centered player dot)
- Top-left quest objective chip(s) — up to **3** tracked ([DEC-0051](../decisions/index.md#dec-0051-no-xp-power-progression-and-quest-ux)); kind filter; hidden when empty
- Journal (planned, TICKET-0062): tabs **Main · Side · Faction · Archetype · Completed**; abandon non-main; minimap link from tracked chips — [`../design/quest-ui.pen`](../design/quest-ui.pen)
- World quest markers: floating **`?`** available / **`!`** turn-in (bob animation) — assets [`../design/quest-assets/`](../design/quest-assets/) → `assets/ui/quest/`
- Minimap: kind-tinted objective dots, gold path dashes to focused chip, edge chevron when off-disk; click tracked chip to focus ([`../design/quest-ui.pen`](../design/quest-ui.pen) screen 09)
- Full map (M): centered on focused objective; pins by kind; Set as Tracked (screen 10)
- Design PNGs: `context/design/hud-assets/` → `assets/ui/hud/` (loaded via widget `image` + `UiTextureCache`)
- Dialogue modal uses the same tokens + circular portrait rings (`dialogue.uicanvas.json`, `assets/ui/dialogue/`)

World-space billboards (`WorldUiBillboardRuntime`):

- Interaction prompts (`interact.*` blackboard → parchment “Press E …” chip)
- Optional floating text/bar chips via `engine.world_ui_upsert` (NPC health, etc.)

## Live loop

1. Edit `player.uicanvas.json` with `engine_hud_apply` (works during play test).
2. Edit combat/heal rules in Lua with `engine_lua_apply`.
3. Start play test → HUD draws over Game viewport; approach volumes for billboard prompts; Press E to use.

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
| `engine.set_resource(current, max)` | Secondary class resource binds |
| `engine.get_resource()` | Returns `current, max` |
| `engine.apply_archetype_hud(archetypeId)` | Seeds stamina vs magic label/color (`ashfell_blade`/`outrider` → stamina; `runecaster` → magic) |
| `engine.world_ui_upsert(id, {x,y,z,text,barCurrent,barMax,visible})` | World-anchored billboard chip |
| `engine.world_ui_clear(id?)` | Remove one billboard or clear all |

Play-test dodge spends **20** from `player.resource` and regenerates at **25/s** after a **0.4 s** delay (see [`gearing-system.md`](gearing-system.md)). There is still no general sprint/attack stamina economy (TICKET-0127).

## Interact blackboard (prompt UX)

Scripts set these on enter/exit; the Game viewport syncs them into the `interact_prompt` billboard:

| Key | Type | Meaning |
| --- | --- | --- |
| `interact.prompt` | bool | Show billboard |
| `interact.id` | string | Interaction id for Press E / `dispatch_interaction_use` |
| `interact.label` | string | Chip text (e.g. `Press E to talk`) |
| `interact.x/y/z` | number | World anchor |

## Sample

- Layout: `samples/open-world-rpg/assets/ui/player.uicanvas.json`
- Damage: `assets/scripts/combat_hurt.lua` (−10 HP)
- Rest: `assets/scripts/campfire_interaction.lua` (+15 HP on **use**)
- Talk / investigate: `talk_*_interaction.lua`, `event_zone_sandbox_interaction.lua`

## Verification

- `hud` and `scripting` suites
- Format: [`../formats/hud-assets.md`](../formats/hud-assets.md)
- Billboards: [`interaction-volumes.md`](interaction-volumes.md)
