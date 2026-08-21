# Marble-bag gameplay RNG

- Status: active (crit + weapon damage bags in `InventoryRuntime`; loot bags still planned)
- Decision: [DEC-0057](../decisions/index.md#dec-0057-marble-bag-rng-for-gameplay-rolls)
- Related: [DEC-0048](../decisions/index.md#dec-0048-terraria-shaped-gearing-with-soft-archetype-affinity) loot tables, [DEC-0003](../decisions/index.md#dec-0003-automation-first-tools) deterministic commands, Act 0 loot Lua `samples/open-world-rpg/assets/scripts/loot_container_interaction.lua` (independent weighted pick — **non-canon** until migrated)

## Product intent

Every **gameplay** random outcome uses a **marble bag**: put integer counts of each outcome in a named bag, draw one marble, do not put it back until the bag is empty, then refill to the authored mix.

That is the contract for loot, crits, item procs, extra-drop coins, random encounter/table picks, and any similar roll. It is **not** a per-roll percentage that forgets history.

**Shipped so far:** `InventoryRuntime` draws **crit** marbles from chance%, and **weapon damage** marbles as one marble per integer in the attribute-scaled `damageMin`…`damageMax` range (`roll_player_attack`).

## How a bag works

1. Author a bag id and marble counts (example crit: `hit` ×4 + `crit` ×1 → 20% long-run, but a crit removes the crit marble until refill).
2. Runtime **shuffles** remaining marbles (seeded) and **pops** the next outcome.
3. **Refill** when empty, or when a requested multi-draw cannot be satisfied from what remains.
4. Persist **remaining** marbles on the save so pity/cycle survives reload.

A 1-in-20 rare loot marble **will** appear once per cycle of 20 draws from that bag (modulo refill rules), instead of clustering or vanishing across independent rolls.

## In vs out

| In (must use bags) | Out (independent / existing seeds) |
| --- | --- |
| Loot tables, chest/pouch grants, combat drops | Particle flipbook / rotation start |
| Crit / glance / proc chance | Foliage scatter positions |
| Weapon hit damage within `damageMin`…`damageMax` | Editor placement yaw randomize |
| Any new gameplay `if random < p` | Camera/VFX noise |

Story grants and guaranteed quest rewards stay **authored grants**, not bag draws.

## Engine vs content

- **C++** owns shuffle, draw, refill, save remainder, Lua/MCP draw commands, and tests (empty bag, refill, seed replay, persist round-trip, reject `math.random` gameplay paths once the API exists).
- **Project data** owns bag id + marble composition (loot entries become counts, not independent weights).
- **Lua** requests `draw(bagId)` (or batch) and applies the result (grant item, apply crit). Do not implement a second RNG in script.

Hot-reload routing follows [DEC-0055](../decisions/index.md#dec-0055-reloadable-native-gameplay-is-the-default-c-iteration-path): the bag runtime is a gameplay capability; put it in `engine_core` if it is a shared save/API contract, or `game_module` only if it can stay behind a POD ABI without owning save I/O.

## v1 UI

Character sheet shows **crit chance %** plus marble mix (`N crit / M hit`). Diagnostics/MCP may still print remaining counts. A dedicated pity widget is not required.

## Current sample debt

`loot_container_interaction.lua` uses `weighted_pick` + a second `math.random() < 0.45` extra grant. When bags ship, that table should be marble counts and two sequential draws from one bag (second marble may still be a dedicated `none` outcome if “sometimes only one item” is desired).

## Acceptance (when implemented)

- Seeded draw sequence is deterministic in tests.
- Empty bag refills; composition matches authored counts.
- Save/load restores remaining marbles.
- Lua/MCP gameplay loot/crit paths have no independent `random` for those outcomes.
- Particle/foliage suites still use their existing noise, not marble bags.
