# Combat Text (Floating Damage Numbers)

Status: active — [DEC-0059](../decisions/index.md#dec-0059-world-anchored-combat-text-floaters)

Ephemeral floating damage numbers when hits land. Separate from persistent NPC name/HP world billboards.

## Ownership

| Layer | Owner |
| --- | --- |
| Juice (impact scale → rise → fade) | C++ `CombatTextRuntime` |
| Spawn on damage | Lua `engine.combat_text({x,y,z,amount,crit})` |
| Anchor | Just **above** the enemy/NPC **name** chip (not contact point) |

## Feel

1. **Impact:** short overshoot scale punch on spawn.
2. **Rise + fade:** floats upward in world space, alpha fades out, then despawns.
3. **Crit:** gold accent color, larger base font, stronger peak scale, slightly longer life / rise.
4. **Block:** `text = "Blocked"` on a parried melee (TICKET-0285); same chrome as a hit, no HP.
5. **DoT ticks:** bleed (red), poison (green), burn (orange) use a minus prefix. Frost Slow spawns `Slowed` on apply (no tick numbers).

Colors follow [ui-chrome-direction.md](../art/ui-chrome-direction.md): chrome text for normal hits, bright gold for crits.

## Lua

```lua
engine.combat_text({
  x = name_x,
  y = name_y + 0.55, -- above name plate
  z = name_z,
  amount = damage,
  crit = is_crit,
})
-- Prefer payload.hurtEntityPosition (live feet) + head offset from C++ combat events.
-- optional: text = "MISS" instead of amount
```

## Sample

Combat sandbox dummies (`assets/scripts/dummy_hurt.lua`) spawn floaters above each dummy name/HP chip on every `combatHurt`.

## Out of scope (v1)

- Dedicated Block / Miss glyphs beyond `text = "Blocked"`
- Screen-space canvas widgets
- Authored timing curves as assets
