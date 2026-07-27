-- Sandbox event zone: Press E to start theatrical timeline (camera pan + dialogue).
-- Enter/exit only drive the world interact billboard.

local SEQUENCE_ID = "evt_sandbox_zone_pan"

function on_event_sandbox(payload_json)
  local payload, err = engine.json_decode(payload_json)
  if not payload then
    engine.log("error", "event_sandbox decode failed: " .. tostring(err))
    return
  end

  local kind = tostring(payload.type or "")
  engine.log("info", "event_sandbox " .. kind .. " id=" .. tostring(payload.interactionId))

  if kind == "enter" then
    engine.blackboard_set("interact.prompt", true)
    engine.blackboard_set("interact.id", "event_sandbox")
    engine.blackboard_set("interact.label", "Press E to investigate")
    if payload.contactPoint then
      engine.blackboard_set("interact.x", payload.contactPoint.x or 0)
      engine.blackboard_set("interact.y", (payload.contactPoint.y or 0) + 2.0)
      engine.blackboard_set("interact.z", payload.contactPoint.z or 0)
    end
    return
  end

  if kind == "exit" then
    engine.blackboard_set("interact.prompt", false)
    engine.blackboard_set("interact.id", "")
    engine.blackboard_set("event.lastZoneExit", "event_sandbox")
    return
  end

  if kind ~= "use" and kind ~= "activate" then
    return
  end

  if engine.event_timeline_control_locked and engine.event_timeline_control_locked() then
    engine.log("info", "event_sandbox skipped (timeline already locking control)")
    return
  end
  if engine.dialogue_active and engine.dialogue_active() then
    engine.log("info", "event_sandbox skipped (dialogue already active)")
    return
  end

  local started, start_err = pcall(function()
    engine.start_event_timeline(SEQUENCE_ID)
  end)
  if not started then
    engine.log("error", "event_sandbox start_event_timeline failed: " .. tostring(start_err))
    return
  end

  engine.blackboard_set("event.lastSequenceId", SEQUENCE_ID)
  engine.blackboard_set("event.lastZone", "event_sandbox")
  engine.blackboard_set("interact.prompt", false)
  engine.log("info", "event_sandbox started sequence=" .. SEQUENCE_ID)
end
