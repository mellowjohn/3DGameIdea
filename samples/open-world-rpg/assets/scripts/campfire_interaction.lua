function on_use_campfire(payload_json)
  local payload, err = engine.json_decode(payload_json)
  if not payload then
    engine.log("error", "campfire decode failed: " .. tostring(err))
    return
  end

  engine.log("info", "campfire " .. tostring(payload.type) .. " id=" .. tostring(payload.interactionId))
  engine.blackboard_set("interaction.lastId", tostring(payload.interactionId or ""))
  engine.blackboard_set("interaction.lastType", tostring(payload.type or ""))
  if payload.type == "enter" then
    engine.blackboard_set("interaction.campfireActive", true)
    local current, max = engine.get_health()
    if not max or max <= 0 then
      current, max = 100, 100
    end
    local heal = 15
    current = math.min(max, current + heal)
    engine.set_health(current, max)
    engine.log("info", "campfire heal hp=" .. tostring(current) .. "/" .. tostring(max))
  elseif payload.type == "exit" then
    engine.blackboard_set("interaction.campfireActive", false)
  end
end
