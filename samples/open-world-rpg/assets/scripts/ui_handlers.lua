-- UI button handlers for modal canvases (pause, main menu, settings, inventory, dialogue, co-op lobby)

local inventory_sort_mode = "name"
local inventory_craft_tab = false

local function inventory_stack_icon(entry)
  if not entry or not entry.itemId or entry.itemId == "" then
    return ""
  end
  return tostring(entry.icon or "")
end

local function inventory_stack_count(entry)
  if not entry or not entry.itemId or entry.itemId == "" then
    return ""
  end
  local c = tonumber(entry.count) or 0
  if c <= 1 then
    return ""
  end
  return "x" .. tostring(c)
end

local function parse_inventory_select(bind)
  local bag = string.match(bind or "", "^inventory%.select%.bag%.(%d+)$")
  if bag then
    return "bag", tonumber(bag), ""
  end
  local hot = string.match(bind or "", "^inventory%.select%.hotbar%.(%d+)$")
  if hot then
    return "hotbar", tonumber(hot), ""
  end
  local equip = string.match(bind or "", "^inventory%.select%.equip%.(%w+)$")
  if equip then
    return "equip", -1, equip
  end
  return nil, nil, nil
end

local function sync_inventory_ui()
  local ok, status = pcall(function()
    return engine.inventory_status()
  end)
  if not ok or type(status) ~= "table" then
    return
  end

  engine.ui_canvas_set_text("inventory", "inventory.title", "Inventory")
  engine.ui_canvas_set_text("inventory", "inventory.gold", tostring(status.gold or 0) .. " gold")
  engine.ui_canvas_set_text("inventory", "inventory.sort", "SORT")
  engine.ui_canvas_set_text("inventory", "inventory.craftBody", "Crafting comes later.")
  engine.ui_canvas_set_text("inventory", "inventory.equippedLabel", "Equipped")
  engine.ui_canvas_set_text("inventory", "inventory.trinketLabel", "Trinkets")
  engine.ui_canvas_set_text("inventory", "inventory.bagsLabel", "Bags")
  engine.ui_canvas_set_text("inventory", "inventory.bindLabel", "Hotbar")
  for i = 0, 2 do
    engine.ui_canvas_set_image("inventory", "inventory.bagEquip." .. tostring(i) .. ".icon", "")
  end

  local bag = status.bag or {}
  local filled = 0
  for i = 0, 19 do
    local entry = bag[i + 1]
    local bind = "inventory.bag." .. tostring(i)
    engine.ui_canvas_set_image("inventory", bind .. ".icon", inventory_stack_icon(entry))
    engine.ui_canvas_set_text("inventory", bind .. ".count", inventory_stack_count(entry))
    if entry and entry.itemId and entry.itemId ~= "" then
      filled = filled + 1
    end
  end
  local cap = tonumber(status.bagCapacity) or 20
  engine.ui_canvas_set_text("inventory", "inventory.bagCap", tostring(filled) .. " / " .. tostring(cap))
  engine.ui_canvas_set_text("inventory", "inventory.bagLabel", "Bag")

  local hotbar = status.hotbar or {}
  local selected_hotbar = tonumber(status.selectedHotbar) or 0
  for i = 0, 7 do
    local entry = hotbar[i + 1]
    local bind = "inventory.hotbar." .. tostring(i)
    engine.ui_canvas_set_image("inventory", bind .. ".icon", inventory_stack_icon(entry))
    engine.ui_canvas_set_text("inventory", bind .. ".key", tostring(i + 1))
    engine.hud_set_image("hud.hotbar." .. tostring(i + 1) .. ".icon", inventory_stack_icon(entry))

    local selected = (i == selected_hotbar)
    -- HUD slot chrome: tint ability-slot art + bool drives gold selection ring
    engine.hud_set_bool("hud.hotbar." .. tostring(i + 1) .. ".selected", selected)
    if selected then
      engine.hud_set_color("hud_hotbar_" .. tostring(i + 1), 255, 220, 140, 255)
      engine.hud_set_color("hud_hotbar_" .. tostring(i + 1) .. "_key", 255, 230, 160, 255)
      engine.ui_canvas_set_color("inventory", "inventory_hotbar_" .. tostring(i) .. "_bg", 196, 162, 74, 255)
    else
      engine.hud_set_color("hud_hotbar_" .. tostring(i + 1), 220, 210, 195, 255)
      engine.hud_set_color("hud_hotbar_" .. tostring(i + 1) .. "_key", 241, 238, 232, 255)
      engine.ui_canvas_set_color("inventory", "inventory_hotbar_" .. tostring(i) .. "_bg", 58, 52, 44, 255)
    end
  end

  local equipped = status.equipped or {}
  local function sync_equip(slot, entry)
    engine.ui_canvas_set_image("inventory", "inventory.equip." .. slot .. ".icon", inventory_stack_icon(entry))
  end
  sync_equip("head", equipped.head)
  sync_equip("chest", equipped.chest)
  sync_equip("legs", equipped.legs)
  for t = 0, 3 do
    local key = "trinket" .. tostring(t)
    sync_equip(key, equipped[key])
  end

  local sel = status.selection or {}
  local region = tostring(sel.region or "")
  local index = tonumber(sel.index) or -1
  local equip_slot = tostring(sel.equipSlot or "")
  local selected = nil
  if region == "bag" and index >= 0 then
    selected = bag[index + 1]
  elseif region == "hotbar" and index >= 0 then
    selected = hotbar[index + 1]
  elseif region == "equip" then
    selected = equipped[equip_slot]
  end

  if selected and selected.itemId and selected.itemId ~= "" then
    engine.ui_canvas_set_text("inventory", "inventory.propsName", tostring(selected.displayName or selected.itemId))
    local kind = string.upper(tostring(selected.kind or ""))
    local equipped_here = (region == "hotbar" and index == selected_hotbar)
    if equipped_here then
      kind = kind .. " · EQUIPPED"
    end
    engine.ui_canvas_set_text("inventory", "inventory.propsKind", kind)
    local desc = selected.notes
    if not desc or desc == "" then
      if equipped_here then
        desc = "Active hotbar slot " .. tostring(selected_hotbar + 1) .. " (keys 1–8)."
      else
        desc = ""
      end
    elseif equipped_here then
      desc = tostring(desc) .. "\nActive hotbar slot " .. tostring(selected_hotbar + 1) .. "."
    end
    engine.ui_canvas_set_text("inventory", "inventory.propsDesc", desc)
    engine.ui_canvas_set_image("inventory", "inventory.props.icon", inventory_stack_icon(selected))
  else
    engine.ui_canvas_set_text("inventory", "inventory.propsName", "—")
    engine.ui_canvas_set_text("inventory", "inventory.propsKind", "")
    engine.ui_canvas_set_text("inventory", "inventory.propsDesc", "")
    engine.ui_canvas_set_image("inventory", "inventory.props.icon", "")
  end

  engine.ui_canvas_set_visible("inventory", "inventory_craft_panel", inventory_craft_tab)
  engine.ui_canvas_set_visible("inventory", "inventory_craft_body", inventory_craft_tab)
end

local function handle_inventory_bind(bind, event)
  if bind == "inventory.drag_drop" then
    local from_bind = event and event.fromBind or ""
    local to_bind = event and event.toBind or ""
    local from_region, from_index, from_equip = parse_inventory_select(from_bind)
    local to_region, to_index, to_equip = parse_inventory_select(to_bind)
    if from_region and to_region then
      pcall(function()
        engine.inventory_move(from_region, from_index, from_equip or "", to_region, to_index, to_equip or "")
      end)
      pcall(function()
        if to_region == "hotbar" then
          engine.inventory_select_hotbar(to_index)
        else
          engine.inventory_select(to_region, to_index or -1, to_equip or "")
        end
      end)
      sync_inventory_ui()
    end
    return true
  end
  if bind == "inventory.close" then
    engine.ui_pop()
    return true
  end
  if bind == "inventory.tab_bag" then
    inventory_craft_tab = false
    sync_inventory_ui()
    return true
  end
  if bind == "inventory.tab_craft" then
    inventory_craft_tab = true
    sync_inventory_ui()
    return true
  end
  if bind == "inventory.sort" then
    if inventory_sort_mode == "name" then
      inventory_sort_mode = "kind"
    elseif inventory_sort_mode == "kind" then
      inventory_sort_mode = "newest"
    else
      inventory_sort_mode = "name"
    end
    sync_inventory_ui()
    return true
  end
  if bind == "inventory.equip" then
    pcall(function()
      engine.inventory_equip_selected()
    end)
    sync_inventory_ui()
    return true
  end
  if bind == "inventory.unequip" then
    pcall(function()
      engine.inventory_unequip_selected()
    end)
    sync_inventory_ui()
    return true
  end
  if string.match(bind, "^inventory%.bagEquip%.%d+$") then
    -- Bag-upgrade stubs: square slots only for now.
    return true
  end

  local bag_sel = string.match(bind, "^inventory%.select%.bag%.(%d+)$")
  if bag_sel then
    pcall(function()
      engine.inventory_select("bag", tonumber(bag_sel))
    end)
    sync_inventory_ui()
    return true
  end
  local hot_sel = string.match(bind, "^inventory%.select%.hotbar%.(%d+)$")
  if hot_sel then
    pcall(function()
      engine.inventory_select_hotbar(tonumber(hot_sel))
    end)
    sync_inventory_ui()
    return true
  end
  local equip_sel = string.match(bind, "^inventory%.select%.equip%.(%w+)$")
  if equip_sel then
    pcall(function()
      engine.inventory_select("equip", -1, equip_sel)
    end)
    sync_inventory_ui()
    return true
  end
  return false
end

local function pop_until_main_or_empty()
  while true do
    local top = engine.ui_top()
    if top == nil or top == "main_menu" then
      break
    end
    engine.ui_pop()
  end
end

local function leave_coop_lobby()
  engine.coop_end_session()
  pop_until_main_or_empty()
  if engine.ui_top() ~= "main_menu" then
    engine.ui_push("main_menu")
  end
end

local function open_ready_room()
  local top = engine.ui_top()
  if top == "coop_lobby_host" or top == "coop_lobby_join" then
    engine.ui_pop()
  end
  if engine.ui_top() ~= "coop_ready_room" then
    engine.ui_push("coop_ready_room")
  end
  engine.ui_canvas_set_text("coop_ready_room", "coop_ready_room.host_summary", "Host\nSquire")
  engine.ui_canvas_set_text("coop_ready_room", "coop_ready_room.guest_summary", "Guest\nArcher")
  engine.coop_set_ready(0, false)
  engine.coop_set_ready(1, false)
end

function on_ui_button(payload_json)
  local event = engine.json_decode(payload_json)
  local bind = event.bind

  if handle_inventory_bind(bind, event) then
    return
  end

  if bind == "pause.resume" then
    engine.ui_pop()
  elseif bind == "pause.quit" then
    engine.ui_pop()
    engine.ui_push("main_menu")
  elseif bind == "main_menu.new_game" then
    engine.ui_pop()
  elseif bind == "main_menu.coop" then
    engine.coop_begin_host_lobby()
    engine.ui_push("coop_lobby_host")
    local code = engine.coop_invite_code()
    engine.ui_canvas_set_text("coop_lobby_host", "coop_lobby_host.invite", "Invite code: " .. code)
    engine.ui_canvas_set_text("coop_lobby_host", "coop_lobby_host.status", "Waiting for guest…")
  elseif bind == "main_menu.quit" then
    engine.ui_pop()
  elseif bind == "main_menu.settings" then
    engine.ui_push("settings")
  elseif bind == "settings.back" then
    engine.ui_pop()
  elseif bind == "dialogue.continue" then
  elseif bind == "coop_lobby_host.mock_guest" then
    engine.coop_mock_guest_join(engine.coop_invite_code())
    open_ready_room()
  elseif bind == "coop_lobby_host.open_join" then
    engine.ui_pop()
    engine.ui_push("coop_lobby_join")
  elseif bind == "coop_lobby_host.back" then
    leave_coop_lobby()
  elseif bind == "coop_lobby_join.submit" then
    local top = engine.ui_top()
    engine.coop_begin_host_lobby()
    engine.coop_mock_guest_join("COOP-LOCAL")
    if top == "coop_lobby_join" then
      engine.ui_pop()
    end
    open_ready_room()
  elseif bind == "coop_lobby_join.back" then
    leave_coop_lobby()
  elseif bind == "coop_ready_room.host_ready" then
    engine.coop_toggle_ready(0)
  elseif bind == "coop_ready_room.guest_ready" then
    engine.coop_toggle_ready(1)
  elseif bind == "coop_ready_room.start" then
    engine.coop_host_start()
    pop_until_main_or_empty()
    if engine.ui_top() == "main_menu" then
      engine.ui_pop()
    end
    engine.blackboard_set("coop.request_play_test", true)
  elseif bind == "coop_ready_room.leave" then
    leave_coop_lobby()
  elseif bind == "coop_reconnect.end_session" then
    engine.coop_end_session()
    if engine.ui_top() == "coop_reconnect" then
      engine.ui_pop()
    end
    engine.blackboard_set("coop.request_end_test", true)
    engine.ui_push("main_menu")
  end
end

function inventory_refresh_ui()
  sync_inventory_ui()
end
