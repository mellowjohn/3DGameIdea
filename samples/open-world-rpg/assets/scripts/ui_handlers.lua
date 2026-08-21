-- UI button handlers for modal canvases (pause, main menu, settings, inventory, dialogue, co-op lobby)



local inventory_sort_mode = "name"

local inventory_craft_tab = false

local inventory_fallback_icon = "assets/ui/icons/items/unknown_item.png"



local function inventory_stack_icon(entry)

  if not entry or not entry.itemId or entry.itemId == "" then

    return ""

  end

  local icon = tostring(entry.icon or "")

  if icon == "" then
    return inventory_fallback_icon
  end

  return icon

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

  local crate = string.match(bind or "", "^inventory%.select%.container%.(%d+)$")

  if crate then

    return "container", tonumber(crate), ""

  end

  local ammo_i = string.match(bind or "", "^inventory%.select%.ammo%.(%d+)$")

  if ammo_i then

    return "ammo", tonumber(ammo_i), ""

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

  engine.apply_player_stats()

  local ps = status.playerStats or {}

  local function fmt_stat(n)

    n = tonumber(n) or 0

    local rounded = math.floor(n * 10 + 0.5) / 10

    if math.abs(rounded - math.floor(rounded + 0.5)) < 0.05 then

      return tostring(math.floor(rounded + 0.5))

    end

    return string.format("%.1f", rounded)

  end

  local hp_cur, hp_max = engine.get_health()

  local stam_cur, stam_max = engine.get_resource()

  engine.ui_canvas_set_text("inventory", "inventory.statsLabel", "Character")

  local armor = tonumber(ps.armor) or 0

  local dr = 0

  if armor > 0 then

    dr = 100 * armor / (100 + armor)

  end

  local function resist_line(label, value)

    value = tonumber(value) or 0

    if value <= 0 then

      return label .. "          —"

    end

    local pct = 100 * value / (100 + value)

    return label .. "          " .. fmt_stat(value) .. "  (" .. fmt_stat(pct) .. "%)"

  end

  local weapon = "—"

  local dmin = tonumber(ps.damageMin) or 0

  local dmax = tonumber(ps.damageMax) or 0

  if dmin > 0 or dmax > 0 then

    weapon = fmt_stat(dmin) .. "-" .. fmt_stat(dmax)

  end

  local aps = tonumber(ps.attacksPerSecond) or 0

  local dps = tonumber(ps.dps) or 0

  local school = tostring(ps.weaponSchool or "none")

  local crit = tonumber(ps.critChance) or 0

  local crit_n = tonumber(ps.critMarbles) or 0

  local hit_n = tonumber(ps.hitMarbles) or 0

  local runes_line = "Runes             —  (hold a magic weapon)"

  if ps.holdingMagicWeapon then

    runes_line = "Runes             HUD pips (spend to cast)"

  end

  local sheet = {

    "CORE",

    "Health            " .. fmt_stat(hp_cur) .. " / " .. fmt_stat(ps.maxHealth or hp_max),

    "Stamina           " .. fmt_stat(stam_cur) .. " / " .. fmt_stat(ps.maxStamina or stam_max),

    runes_line,

    "",

    "ATTRIBUTES",

    "Strength          " .. fmt_stat(ps.strength) .. "  (melee + health)",

    "Agility           " .. fmt_stat(ps.agility) .. "  (ranged + crit %)",

    "Intellect         " .. fmt_stat(ps.intellect) .. "  (magic)",

    "",

    "OFFENSE",

    "School            " .. school,

    "Weapon damage     " .. weapon,

    "Attacks / sec     " .. (aps > 0 and fmt_stat(aps) or "—"),

    "DPS               " .. (dps > 0 and fmt_stat(dps) or "—"),

    "Crit chance       " .. (crit > 0 and (fmt_stat(crit) .. "%") or "—"),

    "Crit bag          " .. (crit_n > 0 and (tostring(crit_n) .. " crit / " .. tostring(hit_n) .. " hit") or "—"),

    "",

    "DEFENSE",

    resist_line("Armor (physical)", armor),

    resist_line("Magic resist    ", ps.magicResist),

    resist_line("Poison resist   ", ps.poisonResist),

    resist_line("Blight resist   ", ps.blightResist),

    resist_line("Holy resist     ", ps.holyResist),

    resist_line("Shadow resist   ", ps.shadowResist),

    "",

    "Held weapon scales from Strength, Agility, or Intellect.",
    "Agility also adds crit chance (2% per point → marble bag).",

    "Stamina is dodge for every lane. Rune pips are magicka while a magic weapon is held.",

    "Crit uses a marble bag (not independent rolls).",

  }

  engine.ui_canvas_set_text("inventory", "inventory.statsBody", table.concat(sheet, "\n"))

  local crate_open = type(status.containerId) == "string" and status.containerId ~= ""

  engine.ui_canvas_set_text("inventory", "inventory.crateLabel", "Crate")

  engine.ui_canvas_set_visible("inventory", "inventory_crate_label", crate_open)

  local crate = status.container or {}

  for i = 0, 7 do

    local entry = crate[i + 1]

    local bind = "inventory.container." .. tostring(i)

    engine.ui_canvas_set_image("inventory", bind .. ".icon", crate_open and inventory_stack_icon(entry) or "")

    engine.ui_canvas_set_text("inventory", bind .. ".count", crate_open and inventory_stack_count(entry) or "")

    engine.ui_canvas_set_visible("inventory", "inventory_crate_" .. tostring(i) .. "_bg", crate_open)

    engine.ui_canvas_set_visible("inventory", "inventory_crate_" .. tostring(i) .. "_icon", crate_open)

    engine.ui_canvas_set_visible("inventory", "inventory_crate_" .. tostring(i) .. "_btn", crate_open)

    engine.ui_canvas_set_visible("inventory", "inventory_crate_" .. tostring(i) .. "_count", crate_open)

  end

  engine.ui_canvas_set_visible("inventory", "inventory_props_kind", not crate_open)

  engine.ui_canvas_set_visible("inventory", "inventory_props_name", not crate_open)

  engine.ui_canvas_set_visible("inventory", "inventory_props_icon_bg", not crate_open)

  engine.ui_canvas_set_visible("inventory", "inventory_props_icon", not crate_open)

  engine.ui_canvas_set_visible("inventory", "inventory_props_desc", not crate_open)

  engine.ui_canvas_set_visible("inventory", "inventory_equip_btn", not crate_open)

  engine.ui_canvas_set_visible("inventory", "inventory_unequip_btn", not crate_open)

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



  -- Ammo lives in its own region (deep stacks), so it never shows up in the bag grid.

  local ammo = status.ammo or {}

  for i = 0, 2 do

    local entry = ammo[i + 1]

    local bind = "inventory.ammo." .. tostring(i)

    engine.ui_canvas_set_image("inventory", bind .. ".icon", inventory_stack_icon(entry))

    engine.ui_canvas_set_text("inventory", bind .. ".count", inventory_stack_count(entry))

  end

  engine.ui_canvas_set_text("inventory", "inventory.ammoLabel", "Ammo")



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

  elseif region == "ammo" and index >= 0 then

    selected = ammo[index + 1]

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

    local lines = {}

    if selected.statsText and tostring(selected.statsText) ~= "" then

      table.insert(lines, tostring(selected.statsText))

    end

    if selected.notes and tostring(selected.notes) ~= "" then

      table.insert(lines, tostring(selected.notes))

    end

    if equipped_here then

      table.insert(lines, "Active hotbar slot " .. tostring(selected_hotbar + 1) .. " (keys 1–8).")

    end

    engine.ui_canvas_set_text("inventory", "inventory.propsDesc", table.concat(lines, "\n"))

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

    pcall(function()

      engine.inventory_close_container()

    end)

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

  local crate_sel = string.match(bind, "^inventory%.select%.container%.(%d+)$")

  if crate_sel then

    pcall(function()

      engine.inventory_select("container", tonumber(crate_sel))

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



-- Opening boot (D-P0-17): New Game → fade → prologue cinematic instance → class → difficulty → appearance courtyard.
-- Calrenoth Landfall cinematic is deferred; Begin Landfall parks on awaiting_landfall.

local prologue_page = 1

local prologue_beats = {

  {

    backdrop = "assets/ui/menu/prologue/prologue-01-throne.png",

    step = "07a · Dramatic pan · Beat 1 of 3",
    stepSub = "Camera pulls back on Luceran · Frangitur gathers",
    speaker = "Frangitur",

    role = "The Great Evil · whisper in the Shroud",
    body = "Luceran... your thirst for control will not save you.\nChaos cannot be chained.\nI gave Tessera its freedom — and those who misuse my gift will drown in it.",
    prompt = "Continue ▸"
  },

  {

    backdrop = "assets/ui/menu/prologue/prologue-02-whisper.png",

    step = "07b · Dramatic pan · Beat 2 of 3",
    stepSub = "Camera settles on Luceran · Frangitur speaks",
    speaker = "Frangitur",

    role = "The Great Evil · whisper in the Shroud",
    body = "Do you understand adventurers?\nThis clown believed he could withstand the Shroud's corruption!\nTo save Tessera, you must rip it apart — you have to!",
    prompt = "Continue ▸"
  },

  {

    backdrop = "assets/ui/menu/prologue/prologue-03-flame.png",

    step = "07c · Dramatic pan · Beat 3 of 3",
    stepSub = "Camera swings to the player · flame · threat",
    speaker = "Frangitur",

    role = "Addressing the adventurer · breaking the fourth wall",
    body = "Please, if you don't then I will!",

    prompt = "Continue ▸  ·  resolves into character creation"
  }

}



local prologue_camera_sequences = {
  "evt_prologue_throne_establish",
  "evt_prologue_throne_luceran",
  "evt_prologue_throne_reveal"
}

local function start_prologue_camera_beat(page)
  local sequence_id = prologue_camera_sequences[page]
  if not sequence_id or sequence_id == "" then
    return
  end
  local started, start_err = pcall(function()
    -- A completed Continue owns the next shot. Cancel makes this safe even if
    -- the player advances before the prior blend has naturally completed.
    if engine.cancel_event_timeline then
      engine.cancel_event_timeline()
    end
    engine.start_event_timeline(sequence_id)
  end)
  if not started then
    engine.log("error", "Opening: start_event_timeline " .. tostring(sequence_id) .. " failed: " .. tostring(start_err))
  else
    engine.log("info", "Opening: started click-gated camera beat " .. tostring(page) .. " (" .. tostring(sequence_id) .. ")")
  end
end

local creation_archetype = "ashfell_blade"

local creation_difficulty = "normal"

local creation_hair_i = 1

local creation_skin_i = 1

local creation_voice_i = 1

local creation_hairs = { "Short Brown", "Cropped Black", "Ash Blonde", "Shaved" }

local creation_skins = { "Warm Tan", "Fair", "Olive", "Deep Brown" }

local creation_voices = { "Steady", "Low", "Sharp", "Quiet" }



local creation_copy = {

  ashfell_blade = {

    name = "Ashfell Blade",

    lane = "Melee · House Ashfell · Fighter / Brawler",
    desc = "Drafted for Calrenoth. House Ashfell steel — duty first.",
    bubble = "House steel under Asher's levy—close combat and duty beneath the Ashfell banner.",
    lore = "You were drafted into House Ashfell's war line, where disciplined fighters and hardened brawlers serve Tessera through blood and obligation. Calrenoth was supposed to hold. You arrive as reinforcement—not yet a hero.",
    abilities = "Examples · Guarded Strike · Banner Push · Close Quarters"
  },

  outrider = {

    name = "Outrider",

    lane = "Ranged · Outrider Lodge · Ranger / Nomad",
    desc = "Scout and skirmish. Soft footfalls, hard arrows.",

    bubble = "A mobile Lodge skirmisher—strike from range and stay ahead of the line.",
    lore = "The Outrider Lodge scouts beyond Tessera's armies, carrying warnings and harrying enemies before battle begins. Called to reinforce Calrenoth, you approach the siege with bow in hand and danger already at your heels.",

    abilities = "Examples · Aimed Shot · Soft Footfalls · Harrier's Mark"
  },

  runecaster = {

    name = "Runecaster",

    lane = "Rune / Sigil · Runecaster Guild",
    desc = "Runes and foci — Guild craft under pressure.",
    bubble = "A Guild combatant who prepares runes and drafts sigils under pressure.",

    lore = "The Runecaster Guild teaches power that must be inscribed, prepared, and triggered through practiced craft. Sent to Calrenoth as arcane support, you carry a small arsenal of runes into a fortress already beginning to fall.",

    abilities = "Examples · Inscribe Rune · Trigger Sigil · Focus Bind"
  }

}



local difficulty_copy = {

  normal = {

    id = "normal",

    name = "Ashen's Levy",

    lane = "Normal · War draft · fair steel",
    bubble = "A fair war draft — deaths sting, but the levy still reforms you.",
    lore = "Tessera answers with discipline, not cruelty. Kit holds, foes press, and free saves keep the campaign moving when steel fails.",

    rules = "Rules · Deaths sting · Levy reforms · Free saves"
  },

  hard = {

    id = "hard",

    name = "Calrenoth Breach",

    lane = "Hard · Siege fire · scarce kit",
    bubble = "Scarce kit and harder foes — the breach does not forgive waste.",
    lore = "Calrenoth's walls already smoke. Supplies thin, enemies hit harder, and every save costs more when the line buckles.",

    rules = "Rules · Scarce kit · Harder foes · Costly saves"
  },

  nightmare = {

    id = "nightmare",

    name = "Frangitur's Claim",

    lane = "Nightmare · Permadeath · hollow throne",
    bubble = "One life. Tessera will not reform you under Frangitur's claim.",

    lore = "The hollow throne does not bargain. Fall once and the campaign ends — no levy, no second draft, only the claim that remains.",
    rules = "Rules · Permadeath · One life · No reform"
  }

}



local function hide_if_present(id)

  pcall(function()

    engine.ui_hide(id)

  end)

end



-- Modal stack draws every layer. Always strip opening canvases before menu so

-- Appearance/prologue chrome cannot ghost through transparent menu plates.

local function show_main_menu_clean()

  hide_if_present("opening_fade")

  hide_if_present("character_creation_appearance")

  hide_if_present("character_creation_difficulty")

  hide_if_present("character_creation")

  hide_if_present("prologue")

  if engine.ui_top() ~= "main_menu" then

    engine.ui_push("main_menu")

  end

  engine.blackboard_set("opening.phase", "main_menu")

end



local function sync_prologue_ui(opts)

  opts = opts or {}

  local total = #prologue_beats

  if prologue_page < 1 then

    prologue_page = 1

  elseif prologue_page > total then

    prologue_page = total

  end

  local beat = prologue_beats[prologue_page]

  engine.ui_canvas_set_image("prologue", "prologue.backdrop", beat.backdrop)

  engine.ui_canvas_set_text("prologue", "prologue.step", beat.step)

  engine.ui_canvas_set_text("prologue", "prologue.stepSub", beat.stepSub)

  engine.ui_canvas_set_text("prologue", "prologue.speaker", beat.speaker)

  engine.ui_canvas_set_text("prologue", "prologue.role", beat.role)

  if opts.defer_typewriter then

    -- Keep body empty/hidden under the crossfade cover; reveal_done starts typing.

    engine.ui_canvas_set_text("prologue", "prologue.body", "")

  else

    engine.ui_canvas_set_text_typed("prologue", "prologue.body", beat.body, 38)

  end

  engine.ui_canvas_set_text("prologue", "prologue.prompt", beat.prompt)

  engine.ui_canvas_set_text("prologue", "prologue.skip", "Skip prologue")

  if prologue_page >= total then

    engine.ui_canvas_set_text("prologue", "prologue.continue", "Choose your lane")

  else

    engine.ui_canvas_set_text("prologue", "prologue.continue", "Continue")

  end

  if not opts.keep_fade_cover then

    engine.ui_canvas_set_visible("prologue", "prologue_fade_cover", false)

  end

end



local function sync_character_creation_ui()

  local copy = creation_copy[creation_archetype] or creation_copy.ashfell_blade

  engine.ui_canvas_set_text("character_creation", "character_creation.step", "03a · Choose your lane")
  engine.ui_canvas_set_text("character_creation", "character_creation.title", "Stained Glass of Calrenoth")

  engine.ui_canvas_set_text(

    "character_creation",

    "character_creation.subtitle",

    "Three windows in the courtyard wall. Choose your starting archetype."

  )

  engine.ui_canvas_set_text("character_creation", "character_creation.detail.title", copy.name)

  engine.ui_canvas_set_text("character_creation", "character_creation.detail.lane", copy.lane)

  engine.ui_canvas_set_text(

    "character_creation",

    "character_creation.detail.body",

    copy.bubble .. " " .. copy.lore

  )

  engine.ui_canvas_set_text("character_creation", "character_creation.detail.abilities", copy.abilities)

  engine.ui_canvas_set_visible("character_creation", "cc_sel_ashfell", creation_archetype == "ashfell_blade")

  engine.ui_canvas_set_visible("character_creation", "cc_sel_outrider", creation_archetype == "outrider")

  engine.ui_canvas_set_visible("character_creation", "cc_sel_runecaster", creation_archetype == "runecaster")

  engine.ui_canvas_set_visible("character_creation", "cc_detail_panel", true)

  engine.ui_canvas_set_visible("character_creation", "cc_detail_title", true)

  engine.ui_canvas_set_visible("character_creation", "cc_detail_lane", true)

  engine.ui_canvas_set_visible("character_creation", "cc_detail_body", true)

  engine.ui_canvas_set_visible("character_creation", "cc_detail_abilities", true)

end



local function sync_difficulty_ui()

  local diff = difficulty_copy[creation_difficulty] or difficulty_copy.normal

  engine.ui_canvas_set_text(

    "character_creation_difficulty",

    "character_creation_difficulty.step",

    "03b · After class · Choose your trial"
  )

  engine.ui_canvas_set_text(

    "character_creation_difficulty",

    "character_creation_difficulty.title",

    "How hard will Tessera answer?"

  )

  engine.ui_canvas_set_text(

    "character_creation_difficulty",

    "character_creation_difficulty.subtitle",

    "Difficulty is set here — not in Settings."
  )

  engine.ui_canvas_set_text(

    "character_creation_difficulty",

    "character_creation_difficulty.detail.title",

    diff.name

  )

  engine.ui_canvas_set_text(

    "character_creation_difficulty",

    "character_creation_difficulty.detail.lane",

    diff.lane

  )

  engine.ui_canvas_set_text(

    "character_creation_difficulty",

    "character_creation_difficulty.detail.body",

    diff.bubble .. " " .. diff.lore

  )

  engine.ui_canvas_set_text(

    "character_creation_difficulty",

    "character_creation_difficulty.detail.rules",

    diff.rules

  )

  engine.ui_canvas_set_visible(

    "character_creation_difficulty",

    "ccd_sel_normal",

    creation_difficulty == "normal"

  )

  engine.ui_canvas_set_visible("character_creation_difficulty", "ccd_sel_hard", creation_difficulty == "hard")

  engine.ui_canvas_set_visible(

    "character_creation_difficulty",

    "ccd_sel_nightmare",

    creation_difficulty == "nightmare"

  )

  engine.ui_canvas_set_visible("character_creation_difficulty", "ccd_detail_panel", true)

  engine.ui_canvas_set_visible("character_creation_difficulty", "ccd_detail_title", true)

  engine.ui_canvas_set_visible("character_creation_difficulty", "ccd_detail_lane", true)

  engine.ui_canvas_set_visible("character_creation_difficulty", "ccd_detail_body", true)

  engine.ui_canvas_set_visible("character_creation_difficulty", "ccd_detail_rules", true)

end



local function sync_appearance_ui()

  local copy = creation_copy[creation_archetype] or creation_copy.ashfell_blade

  local diff = difficulty_copy[creation_difficulty] or difficulty_copy.normal

  local hair = creation_hairs[creation_hair_i] or creation_hairs[1]

  local skin = creation_skins[creation_skin_i] or creation_skins[1]

  local voice = creation_voices[creation_voice_i] or creation_voices[1]

  engine.ui_canvas_set_text("character_creation_appearance", "character_creation_appearance.step", "03c · Appearance")
  engine.ui_canvas_set_text("character_creation_appearance", "character_creation_appearance.title", "Appearance")

  engine.ui_canvas_set_text(

    "character_creation_appearance",

    "character_creation_appearance.summary",

    copy.name .. "  ·  " .. diff.name
  )

  engine.ui_canvas_set_text("character_creation_appearance", "character_creation_appearance.name", "Ashen Recruit")

  engine.ui_canvas_set_text(

    "character_creation_appearance",

    "character_creation_appearance.cycle.hair",

    "Hair · " .. hair
  )

  engine.ui_canvas_set_text(

    "character_creation_appearance",

    "character_creation_appearance.cycle.skin",

    "Skin · " .. skin
  )

  engine.ui_canvas_set_text(

    "character_creation_appearance",

    "character_creation_appearance.cycle.voice",

    "Voice · " .. voice
  )

  engine.blackboard_set("appearance.hair", hair)
  engine.blackboard_set("appearance.skin", skin)
  engine.blackboard_set("appearance.eyes", "Brown")

  engine.ui_canvas_set_text(

    "character_creation_appearance",

    "character_creation_appearance.blurb",

    "Hair, skin, and voice only — lane and trial stay fixed until Landfall."
  )

  -- 3D cinematic courtyard owns the backdrop (same pattern as prologue).

  pcall(function()

    engine.ui_canvas_set_visible("character_creation_appearance", "cca_backdrop", false)

    engine.ui_canvas_set_visible("character_creation_appearance", "cca_underlay", false)

  end)

  engine.blackboard_set("opening.hair", hair)

  engine.blackboard_set("opening.skin", skin)

  engine.blackboard_set("opening.voice", voice)

end



local function open_character_creation()

  -- Restore main-menu world under a fade, then C++ calls on_opening_boot("character_creation").

  pcall(function()

    if engine.cancel_event_timeline then

      engine.cancel_event_timeline()

    end

  end)

  engine.ui_push("opening_fade")

  engine.ui_canvas_set_color("opening_fade", "fade_black", 0, 0, 0, 1)

  engine.blackboard_set("opening.requestRestoreMenuForCreation", true)

end



local function open_character_creation_now()

  engine.blackboard_set("opening.phase", "character_creation")

  hide_if_present("prologue")

  hide_if_present("character_creation_difficulty")

  hide_if_present("character_creation_appearance")

  -- Push creation under the fade; C++ RevealCreation hides opening_fade.

  local ok, err = pcall(function()

    engine.ui_push("character_creation")

    engine.ui_push("opening_fade")

    sync_character_creation_ui()

  end)

  if not ok then

    engine.log("error", "Opening: failed to open character creation: " .. tostring(err))

  end

end



local function open_difficulty()

  engine.blackboard_set("opening.phase", "character_creation_difficulty")

  hide_if_present("character_creation")

  hide_if_present("character_creation_appearance")

  if engine.ui_top() ~= "character_creation_difficulty" then

    engine.ui_push("character_creation_difficulty")

  end

  sync_difficulty_ui()

end



local function open_difficulty_now()

  engine.blackboard_set("opening.phase", "character_creation_difficulty")

  hide_if_present("character_creation")

  hide_if_present("character_creation_appearance")

  local ok, err = pcall(function()

    engine.ui_push("character_creation_difficulty")

    engine.ui_push("opening_fade")

    sync_difficulty_ui()

  end)

  if not ok then

    engine.log("error", "Opening: failed to open difficulty: " .. tostring(err))

  end

end



local function open_appearance()

  -- Fade to appearance cinematic courtyard, then C++ calls on_opening_boot.

  engine.ui_push("opening_fade")

  engine.ui_canvas_set_color("opening_fade", "fade_black", 0, 0, 0, 1)

  engine.blackboard_set("opening.requestFadeToAppearance", true)

end



local function open_appearance_now()

  engine.blackboard_set("opening.phase", "character_creation_appearance")

  hide_if_present("character_creation_difficulty")

  hide_if_present("character_creation")

  local ok, err = pcall(function()

    engine.ui_push("character_creation_appearance")

    engine.ui_push("opening_fade")

    sync_appearance_ui()

  end)

  if not ok then

    engine.log("error", "Opening: failed to open appearance: " .. tostring(err))

  end

end



local function back_to_difficulty_from_appearance()

  engine.ui_push("opening_fade")

  engine.ui_canvas_set_color("opening_fade", "fade_black", 0, 0, 0, 1)

  engine.blackboard_set("opening.requestRestoreMenuForDifficulty", true)

end



local function restore_menu_after_appearance()

  engine.ui_push("opening_fade")

  engine.ui_canvas_set_color("opening_fade", "fade_black", 0, 0, 0, 1)

  engine.blackboard_set("opening.requestRestoreMenuIdle", true)

end



local function start_opening_prologue()

  prologue_page = 1

  creation_archetype = "ashfell_blade"

  creation_difficulty = "normal"

  creation_hair_i = 1

  creation_skin_i = 1

  creation_voice_i = 1

  engine.blackboard_set("opening.phase", "prologue")

  engine.blackboard_set("opening.archetypeId", creation_archetype)

  engine.blackboard_set("opening.difficultyId", creation_difficulty)

  hide_if_present("main_menu")

  hide_if_present("character_creation")

  hide_if_present("character_creation_difficulty")

  hide_if_present("character_creation_appearance")

  if engine.ui_top() ~= "prologue" then

    engine.ui_push("prologue")

  end

  sync_prologue_ui({ defer_typewriter = true, keep_fade_cover = true })

  -- Cover stays black until the engine fade-reveal finishes.

  engine.ui_canvas_set_visible("prologue", "prologue_fade_cover", true)

  engine.ui_canvas_set_color("prologue", "prologue_fade_cover", 0, 0, 0, 255)

end



function on_opening_boot(payload_json)

  local event = engine.json_decode(payload_json or "{}")

  local phase = event.phase or ""

  if phase == "prologue" then

    start_opening_prologue()

  elseif phase == "prologue_advance" then

    prologue_page = prologue_page + 1

    if prologue_page > #prologue_beats then

      prologue_page = #prologue_beats

    end

    sync_prologue_ui({ defer_typewriter = true, keep_fade_cover = true })

    engine.ui_canvas_set_visible("prologue", "prologue_fade_cover", true)

    engine.ui_canvas_set_color("prologue", "prologue_fade_cover", 0, 0, 0, 255)

  elseif phase == "reveal_done" then

    engine.ui_canvas_set_visible("prologue", "prologue_fade_cover", false)

    -- Begin speaking as the plate appears (avoid typing under the black cover).

    local beat = prologue_beats[prologue_page]

    if beat then

      engine.ui_canvas_set_text_typed("prologue", "prologue.body", beat.body, 38)

    end

    -- Each camera blend begins only after its matching dialogue page has
    -- finished fading in. The player therefore owns every shot transition.
    start_prologue_camera_beat(prologue_page)
  elseif phase == "character_creation" then

    open_character_creation_now()

  elseif phase == "character_creation_appearance" then

    open_appearance_now()

  elseif phase == "character_creation_difficulty" then

    open_difficulty_now()

  elseif phase == "awaiting_landfall" then

    hide_if_present("character_creation_appearance")

    hide_if_present("character_creation_difficulty")

    hide_if_present("character_creation")

    hide_if_present("prologue")

    hide_if_present("opening_fade")

    if engine.ui_top() ~= "main_menu" then

      engine.ui_push("main_menu")

    end

    engine.blackboard_set("opening.phase", "awaiting_landfall")

  end

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

    -- Fade to black over the menu, then C++ hands off to on_opening_boot("prologue").

    engine.ui_push("opening_fade")

    engine.ui_canvas_set_color("opening_fade", "fade_black", 0, 0, 0, 1)

    engine.blackboard_set("opening.requestFadeToPrologue", true)

  elseif bind == "prologue.continue" then

    -- First press finishes typewriter; second advances (with 2D crossfade).

    if engine.ui_canvas_skip_typewriter and engine.ui_canvas_skip_typewriter("prologue", "prologue.body") then

      return

    end

    if prologue_page >= #prologue_beats then

      open_character_creation()

    else

      engine.blackboard_set("opening.requestBeatCrossfade", true)

    end

  elseif bind == "prologue.skip" then

    open_character_creation()

  elseif bind == "character_creation.pick.ashfell_blade"

      or bind == "character_creation.pick.outrider"

      or bind == "character_creation.pick.runecaster" then

    creation_archetype = string.match(bind, "character_creation%.pick%.(.+)$") or "ashfell_blade"

    engine.blackboard_set("opening.archetypeId", creation_archetype)

    sync_character_creation_ui()

  elseif bind == "character_creation.next" then

    open_difficulty()

  elseif bind == "character_creation.back" then

    show_main_menu_clean()

  elseif bind == "character_creation_difficulty.pick.normal"

      or bind == "character_creation_difficulty.pick.hard"

      or bind == "character_creation_difficulty.pick.nightmare" then

    creation_difficulty = string.match(bind, "character_creation_difficulty%.pick%.(.+)$") or "normal"

    engine.blackboard_set("opening.difficultyId", creation_difficulty)

    sync_difficulty_ui()

  elseif bind == "character_creation_difficulty.next" then

    open_appearance()

  elseif bind == "character_creation_difficulty.back" then

    open_character_creation()

  elseif bind == "character_creation_appearance.cycle.hair" then

    creation_hair_i = (creation_hair_i % #creation_hairs) + 1

    sync_appearance_ui()

  elseif bind == "character_creation_appearance.cycle.skin" then

    creation_skin_i = (creation_skin_i % #creation_skins) + 1

    sync_appearance_ui()

  elseif bind == "character_creation_appearance.cycle.voice" then

    creation_voice_i = (creation_voice_i % #creation_voices) + 1

    sync_appearance_ui()

  elseif bind == "character_creation_appearance.back" then

    back_to_difficulty_from_appearance()

  elseif bind == "character_creation_appearance.confirm"

      or bind == "character_creation.confirm" then

    engine.blackboard_set("opening.archetypeId", creation_archetype)

    engine.blackboard_set("opening.difficultyId", creation_difficulty)

    engine.blackboard_set("opening.phase", "awaiting_landfall")

    hide_if_present("character_creation_appearance")

    hide_if_present("character_creation_difficulty")

    hide_if_present("character_creation")

    hide_if_present("prologue")

    -- Restore menu world under the courtyard until Landfall cinematic lands.

    restore_menu_after_appearance()

    -- Landfall Calrenoth cinematic is next; keep menu preview until that lands.

    engine.log(

      "info",

      "Opening: archetype="

        .. tostring(creation_archetype)

        .. " difficulty="

        .. tostring(creation_difficulty)

        .. " (awaiting Landfall cinematic)"

    )

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



