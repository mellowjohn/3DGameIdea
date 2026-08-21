function on_body_hit(payload_json)
  local payload, err = engine.json_decode(payload_json)
  if not payload then
    engine.log("error", "combat hurt decode failed: " .. tostring(err))
    return
  end

  -- Own arrows/bolts spawn inside the player's body hurt sphere.
  if tostring(payload.attackerId or "") == "player_attack" and
      tostring(payload.hurtCombatId or "") == "body" then
    return
  end

  if payload.blocked then
    engine.blackboard_set("combat.lastBlocked", true)
    engine.blackboard_set("combat.lastHurtId", tostring(payload.hurtCombatId or ""))
    engine.blackboard_set("combat.lastAttackerId", tostring(payload.attackerId or ""))
    local blocks = engine.blackboard_get("combat.blockCount")
    if type(blocks) ~= "number" then
      blocks = 0
    end
    engine.blackboard_set("combat.blockCount", blocks + 1)
    -- Prefer live feet (above the player), not the hurt-sphere contact point.
    local feet = payload.hurtEntityPosition
    local contact = payload.contactPoint
    local x, y, z = nil, nil, nil
    if type(feet) == "table" then
      x = tonumber(feet.x) or 0
      y = (tonumber(feet.y) or 0) + 2.25 + 0.4
      z = tonumber(feet.z) or 0
    elseif type(contact) == "table" then
      x = tonumber(contact.x) or 0
      y = (tonumber(contact.y) or 0) + 0.4
      z = tonumber(contact.z) or 0
    end
    if x then
      pcall(engine.combat_text, {
        x = x,
        y = y,
        z = z,
        text = "Blocked",
        kind = "hit",
      })
    end
    engine.log("info", "hurt blocked combatId=" .. tostring(payload.hurtCombatId))
    return
  end

  local current, max = engine.get_health()
  if not max or max <= 0 then
    current, max = 100, 100
  end
  local damage = 10
  local armor = 0
  local ok_inv, status = pcall(function()
    return engine.inventory_status()
  end)
  if ok_inv and type(status) == "table" and type(status.playerStats) == "table" then
    armor = tonumber(status.playerStats.armor) or 0
  end
  if armor > 0 then
    damage = damage * 100 / (100 + armor)
  end
  current = math.max(0, current - damage)
  engine.set_health(current, max)
  engine.blackboard_set("combat.lastHurtId", tostring(payload.hurtCombatId or ""))
  engine.blackboard_set("combat.lastAttackerId", tostring(payload.attackerId or ""))
  local hits = engine.blackboard_get("combat.hitCount")
  if type(hits) ~= "number" then
    hits = 0
  end
  engine.blackboard_set("combat.hitCount", hits + 1)
  engine.log("info", "hurt hit combatId=" .. tostring(payload.hurtCombatId) .. " hp=" .. tostring(current) .. "/" .. tostring(max))
end
