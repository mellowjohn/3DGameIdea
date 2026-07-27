-- UI button handlers for modal canvases (pause, main menu, settings, inventory, dialogue, co-op lobby)

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
  elseif bind == "inventory.close" then
    engine.ui_pop()
  elseif bind == "dialogue.continue" then
    -- Continue / choice selection is handled in C++ (typewriter skip, choices page, choose).
  elseif bind == "coop_lobby_host.mock_guest" then
    engine.coop_mock_guest_join(engine.coop_invite_code())
    open_ready_room()
  elseif bind == "coop_lobby_host.open_join" then
    engine.ui_pop()
    engine.ui_push("coop_lobby_join")
  elseif bind == "coop_lobby_host.back" then
    leave_coop_lobby()
  elseif bind == "coop_lobby_join.submit" then
    -- Guest path on a single machine: create host lobby if needed, then mock-join.
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
