-- UI button handlers for modal canvases (pause, main menu, settings, inventory, dialogue)

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
  elseif bind == "main_menu.quit" then
    engine.ui_pop()
  elseif bind == "main_menu.settings" then
    engine.ui_push("settings")
  elseif bind == "settings.back" then
    engine.ui_pop()
  elseif bind == "inventory.close" then
    engine.ui_pop()
  elseif bind == "dialogue.continue" then
    engine.ui_pop()
  end
end
