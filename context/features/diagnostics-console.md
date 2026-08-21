# Diagnostics Session Console

Status: active

Owner-facing **cheat / session command line** in the bottom Diagnostics panel (`Diagnostics` → **Console**). Runs against live session `InventoryRuntime` and `FlagRuntime` — the same path as MCP `engine_inventory_call` / `engine_flag_call` — so play-test grants, hotbar edits, and starter loadouts update the in-scene HUD without a parallel implementation.

## UI

- Scrollable history (commands prefixed with `>`, responses below)
- Single-line input; **Enter** executes; **Up/Down** walks command history
- **Clear** / **Help** buttons; `clear` / `help` commands do the same

Works while play-test is **running or paused**. Inventory state exists for the editor session even when play is inactive; play start still resets and re-applies the starter kit as before.

## Commands (v1)

| Command | Behavior |
| --- | --- |
| `help` | List commands |
| `clear` | Clear the history pane |
| `status` | Bag / hotbar / gold / starter / session state |
| `give <itemId> [count]` | `InventoryRuntime::grant` (alias `grant`) |
| `iron_test_set` | Grant and equip the modular iron test helmet, torso, and greaves (alias `iron_test_gear`) |
| `hotbar <slot> <itemId> [count]` | `set_hotbar` — **slot is 0-based** (`0..7`) |
| `select <slot>` | `select_hotbar` (`0..7`) |
| `starter <archetypeId>` | Sets `play_test_starter_archetype_id`, grants starter weapon + bandages on hotbar 0 |
| `gold [amount]` | Show or `set_gold` |
| `flag <id> [true\|false]` | Query / set / clear story flag |
| `flags` | List flags |

Examples: `give outrider_shortbow`, `give guild_rune_focus_fire`, `hotbar 0 ashfell_arming_sword`, `starter outrider`, `iron_test_set`.

## Engineering notes

- Implementation lives in `src/rendering/render_app.cpp` (`draw_diag_console_tab` / `execute_diag_console_command`).
- After inventory mutations, refreshes HUD via `sync_hotbar_equip_hud` + Lua `inventory_refresh_ui` (same as MCP inventory_call).
- MCP exposure of this console is **not** required; agents continue to use `engine_inventory_call`.

## Related

- [gearing-system.md](gearing-system.md) — inventory runtime / starter archetypes
- [runtime-diagnostics.md](runtime-diagnostics.md) — process logging (separate from this UI)
- MCP: `engine_inventory_call` kinds `grant`, `set_hotbar`, `select_hotbar`, `set_starter_archetype`, `apply_starter`
