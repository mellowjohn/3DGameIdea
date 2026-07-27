-- Sandbox-only talk: starts dlg_sandbox_sample on Press E (use), not Act 0 story.
-- Enter/exit drive the world interact prompt.

local TREE_ID = "dlg_sandbox_sample"

function on_talk_sandbox(payload_json)
  local payload, err = engine.json_decode(payload_json)
  if not payload then
    engine.log("error", "talk_sandbox decode failed: " .. tostring(err))
    return
  end

  local kind = tostring(payload.type or "")
  engine.log("info", "talk_sandbox " .. kind .. " id=" .. tostring(payload.interactionId))

  if kind == "enter" then
    engine.blackboard_set("interact.prompt", true)
    engine.blackboard_set("interact.id", "talk_sandbox")
    engine.blackboard_set("interact.label", "Press E to talk")
    if payload.contactPoint then
      engine.blackboard_set("interact.x", payload.contactPoint.x or 0)
      engine.blackboard_set("interact.y", (payload.contactPoint.y or 0) + 2.2)
      engine.blackboard_set("interact.z", payload.contactPoint.z or 0)
    end
    return
  end

  if kind == "exit" then
    engine.blackboard_set("interact.prompt", false)
    engine.blackboard_set("interact.id", "")
    return
  end

  if kind ~= "use" and kind ~= "activate" then
    return
  end

  if engine.dialogue_active and engine.dialogue_active() then
    engine.log("info", "talk_sandbox skipped (dialogue already active)")
    return
  end

  local started, start_err = pcall(function()
    engine.dialogue_start(TREE_ID)
  end)
  if not started then
    engine.log("error", "talk_sandbox dialogue_start failed: " .. tostring(start_err))
    return
  end

  engine.blackboard_set("dialogue.lastTreeId", TREE_ID)
  engine.blackboard_set("interact.prompt", false)
  engine.log("info", "talk_sandbox started tree=" .. TREE_ID)
end
