-- Combat sandbox weapon crate: Press E to open a stash and drag starter weapons
-- to/from bag and hotbar. Contents persist for the play-test session.

local STARTER_LOADOUT = {
  { itemId = "ashfell_arming_sword", count = 1 },
  { itemId = "outrider_shortbow", count = 1 },
  { itemId = "guild_rune_focus_fire", count = 1 },
  { itemId = "guild_rune_focus_frost", count = 1 },
  { itemId = "guild_rune_focus_lightning", count = 1 },
  { itemId = "crude_arrow", count = 50 },
  { itemId = "ashfell_bleed_sword", count = 1 },
  { itemId = "outrider_poison_bow", count = 1 }
}

local function container_has_item(status, item_id)
  local slots = status and status.container or {}
  for i = 1, #slots do
    local entry = slots[i]
    if entry and tostring(entry.itemId or "") == item_id then
      return true
    end
  end
  return false
end

local function seed_missing_starters(status)
  for _, entry in ipairs(STARTER_LOADOUT) do
    if not container_has_item(status, entry.itemId) then
      local ok = pcall(function()
        engine.inventory_grant_container(entry.itemId, entry.count)
      end)
      if not ok then
        engine.log("warn", "weapon_crate seed failed for " .. tostring(entry.itemId))
      end
    end
  end
end

function on_open_weapon_crate(payload_json)
  local payload, err = engine.json_decode(payload_json)
  if not payload then
    engine.log("error", "weapon_crate decode failed: " .. tostring(err))
    return
  end

  local kind = tostring(payload.type or "")
  local interaction_id = tostring(payload.interactionId or "open_weapon_crate")
  local crate_id = tostring(payload.placementEntityId or "weapon_crate")

  if kind == "enter" then
    engine.blackboard_set("interact.prompt", true)
    engine.blackboard_set("interact.id", interaction_id)
    engine.blackboard_set("interact.label", "Press E to open crate")
    if payload.contactPoint then
      engine.blackboard_set("interact.x", payload.contactPoint.x or 0)
      engine.blackboard_set("interact.y", (payload.contactPoint.y or 0) + 1.1)
      engine.blackboard_set("interact.z", payload.contactPoint.z or 0)
    end
    return
  end

  if kind == "exit" then
    engine.blackboard_set("interact.prompt", false)
    engine.blackboard_set("interact.id", "")
    local top = ""
    pcall(function()
      top = tostring(engine.ui_top() or "")
    end)
    if top == "inventory" then
      pcall(function()
        engine.ui_pop()
      end)
    else
      pcall(function()
        engine.inventory_close_container()
      end)
    end
    pcall(inventory_refresh_ui)
    return
  end

  if kind ~= "use" and kind ~= "activate" then
    return
  end

  local opened = pcall(function()
    engine.inventory_open_container(crate_id)
  end)
  if not opened then
    engine.log("error", "weapon_crate open_container failed id=" .. crate_id)
    return
  end

  local status = nil
  pcall(function()
    status = engine.inventory_status()
  end)
  seed_missing_starters(status)

  pcall(function()
    engine.ui_push("inventory")
  end)
  pcall(inventory_refresh_ui)
  engine.log("info", "weapon_crate opened id=" .. crate_id)
end
