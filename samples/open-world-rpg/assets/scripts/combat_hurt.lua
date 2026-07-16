function on_body_hit(payload_json)
  local payload, err = engine.json_decode(payload_json)
  if not payload then
    engine.log("error", "combat hurt decode failed: " .. tostring(err))
    return
  end

  local current, max = engine.get_health()
  if not max or max <= 0 then
    current, max = 100, 100
  end
  local damage = 10
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
