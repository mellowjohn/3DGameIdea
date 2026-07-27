-- Campfire: Press E to rest / heal. Enter/exit only drive the world interact billboard.

function on_use_campfire(payload_json)
  local payload, err = engine.json_decode(payload_json)
  if not payload then
    engine.log("error", "campfire decode failed: " .. tostring(err))
    return
  end

  local kind = tostring(payload.type or "")
  engine.log("info", "campfire " .. kind .. " id=" .. tostring(payload.interactionId))
  engine.blackboard_set("interaction.lastId", tostring(payload.interactionId or ""))
  engine.blackboard_set("interaction.lastType", kind)

  if kind == "enter" then
    engine.blackboard_set("interaction.campfireActive", true)
    engine.blackboard_set("interact.prompt", true)
    engine.blackboard_set("interact.id", "use_campfire")
    engine.blackboard_set("interact.label", "Press E to rest")
    if payload.contactPoint then
      engine.blackboard_set("interact.x", payload.contactPoint.x or 0)
      engine.blackboard_set("interact.y", (payload.contactPoint.y or 0) + 1.6)
      engine.blackboard_set("interact.z", payload.contactPoint.z or 0)
    end
    return
  end

  if kind == "exit" then
    engine.blackboard_set("interaction.campfireActive", false)
    engine.blackboard_set("interact.prompt", false)
    engine.blackboard_set("interact.id", "")
    return
  end

  if kind ~= "use" and kind ~= "activate" then
    return
  end

  engine.play_sound("assets/audio/campfire_crackle.wav")
  local current, max = engine.get_health()
  if not max or max <= 0 then
    current, max = 100, 100
  end
  local heal = 15
  current = math.min(max, current + heal)
  engine.set_health(current, max)
  engine.blackboard_set("interact.prompt", false)
  engine.log("info", "campfire heal hp=" .. tostring(current) .. "/" .. tostring(max))
end
