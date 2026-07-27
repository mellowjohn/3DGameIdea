-- Act 0 talk volume: Press E to start quest-hooked dialogue (or dlg_act0 fallback).
-- Enter/exit only drive the world interact prompt — never auto-start on walk-in.

local QUEST_ID = "mq_act0_calrenoth"
local FALLBACK_TREE = "dlg_act0_meet_arkand"
local GUARD_KEY = "dialogue.talk_act0.started"

function on_talk_act0(payload_json)
  local payload, err = engine.json_decode(payload_json)
  if not payload then
    engine.log("error", "talk_act0 decode failed: " .. tostring(err))
    return
  end

  local kind = tostring(payload.type or "")
  engine.log("info", "talk_act0 " .. kind .. " id=" .. tostring(payload.interactionId))
  engine.blackboard_set("interaction.lastId", tostring(payload.interactionId or ""))
  engine.blackboard_set("interaction.lastType", kind)

  if kind == "enter" then
    engine.blackboard_set("interact.prompt", true)
    engine.blackboard_set("interact.id", "talk_act0")
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

  if engine.blackboard_get(GUARD_KEY) == true then
    engine.log("info", "talk_act0 skipped (already started this session)")
    return
  end

  if engine.dialogue_active and engine.dialogue_active() then
    engine.log("info", "talk_act0 skipped (dialogue already active)")
    return
  end

  local tree_id = FALLBACK_TREE
  local ok, hook = pcall(function()
    return engine.quest_dialogue_hook(QUEST_ID, "start")
  end)
  if ok and type(hook) == "string" and hook ~= "" then
    tree_id = hook
  else
    pcall(function()
      engine.quest_start(QUEST_ID)
    end)
    ok, hook = pcall(function()
      return engine.quest_dialogue_hook(QUEST_ID, "start")
    end)
    if ok and type(hook) == "string" and hook ~= "" then
      tree_id = hook
    end
  end

  local started, start_err = pcall(function()
    engine.dialogue_start(tree_id)
  end)
  if not started then
    engine.log("error", "talk_act0 dialogue_start failed: " .. tostring(start_err))
    return
  end

  engine.blackboard_set(GUARD_KEY, true)
  engine.blackboard_set("dialogue.lastTreeId", tree_id)
  engine.blackboard_set("interact.prompt", false)
  engine.log("info", "talk_act0 started tree=" .. tostring(tree_id))
end
