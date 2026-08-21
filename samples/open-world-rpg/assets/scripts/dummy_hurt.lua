-- Combat sandbox dummy: drain dummy HP, never the player HUD.
-- Training dummies never die and regen quickly. HitReact plays on contact.
-- Health chips stay locked above each dummy (never follow attack contact points).
-- Melee comboStep 1/2/3 → min/mid/max damage. Weapon onHit applies bleed/poison/burn/slow.
-- Lightning bolts may arrive with chainHop > 0 (half damage).

local DUMMY_MAX = 100
local DAMAGE = 10
local REGEN_PER_SEC = 150
local HEAD_OFFSET = 2.25
local COMBAT_TEXT_ABOVE_NAME = 0.55

local DUMMY_ANCHORS = {
  ["3ad6764b-58e1-4c05-9001-d91e1a31d6e1"] = {
    x = -5.0,
    y = 3.974858283996582,
    z = 4.0,
    label = "Dummy",
  },
  ["6b64531b-076a-4a32-bed6-725dc27e304f"] = {
    x = 0.0,
    y = 3.998626232147217,
    z = 11.0,
    label = "Dummy",
  },
  ["7aaf4fb9-adf5-4e12-8242-4a898c21edb9"] = {
    x = 0.0,
    y = 3.9764726161956787,
    z = 6.0,
    label = "Dummy",
  },
  ["e911e9cf-a478-4591-8a3c-8db3ce7503fd"] = {
    x = 5.0,
    y = 3.982089042663574,
    z = 4.0,
    label = "Dummy",
  },
}

local tracked = {}

local function head_xyz(id, payload)
  -- Prefer live scene feet so chips/text track moved dummies.
  if type(payload) == "table" and type(payload.hurtEntityPosition) == "table" then
    local feet = payload.hurtEntityPosition
    local x = tonumber(feet.x)
    local y = tonumber(feet.y)
    local z = tonumber(feet.z)
    if x and y and z then
      return x, y + HEAD_OFFSET, z
    end
  end
  local anchor = DUMMY_ANCHORS[id]
  if not anchor then
    return nil
  end
  return anchor.x, anchor.y + HEAD_OFFSET, anchor.z
end

local function upsert_chip(id, hp, hit_juice, payload)
  local x, y, z = head_xyz(id, payload)
  if not x then
    return
  end
  local label = "Dummy"
  local anchor = DUMMY_ANCHORS[id]
  if anchor and anchor.label then
    label = anchor.label
  end
  engine.world_ui_upsert("dummy.hp." .. id, {
    x = x,
    y = y,
    z = z,
    text = label .. " " .. tostring(math.floor(hp + 0.5)) .. "/" .. tostring(DUMMY_MAX),
    barCurrent = hp,
    barMax = DUMMY_MAX,
    hitJuice = hit_juice == true,
    visible = true,
  })
end

local function apply_weapon_on_hit(target_id, x, y, z)
  local ok_status, status = pcall(function()
    return engine.inventory_status()
  end)
  if not ok_status or type(status) ~= "table" then
    return
  end
  local slot = tonumber(status.selectedHotbar) or 0
  local stack = nil
  if type(status.hotbar) == "table" then
    stack = status.hotbar[slot + 1]
  end
  if type(stack) ~= "table" or not stack.itemId or stack.itemId == "" then
    return
  end
  local ok_def, def = pcall(function()
    return engine.inventory_def(stack.itemId)
  end)
  if not ok_def or type(def) ~= "table" or type(def.stats) ~= "table" then
    return
  end
  local on_hit = def.stats.onHit
  if type(on_hit) ~= "table" then
    return
  end
  for _, hit in ipairs(on_hit) do
    if type(hit) == "table" and type(hit.status) == "string" then
      local dpt = tonumber(hit.damagePerTick)
      if hit.status == "slow" then
        dpt = 0
      elseif not dpt then
        dpt = 1
      end
      pcall(engine.status_apply, {
        targetId = target_id,
        status = hit.status,
        damagePerTick = dpt,
        duration = tonumber(hit.duration) or 6,
        tickInterval = tonumber(hit.tickInterval) or 1,
        x = x,
        y = y,
        z = z,
      })
      if hit.status == "slow" then
        pcall(engine.combat_text, {
          x = x,
          y = y,
          z = z,
          text = "Slowed",
          kind = "hit",
        })
      end
    end
  end
end

function on_dummy_body_hit(payload_json)
  local payload, err = engine.json_decode(payload_json)
  if not payload then
    engine.log("error", "dummy hurt decode failed: " .. tostring(err))
    return
  end

  local id = tostring(payload.hurtPlacementEntityId or "dummy")
  local key = "combat.dummyHp." .. id
  local hp = engine.blackboard_get(key)
  if type(hp) ~= "number" then
    hp = DUMMY_MAX
  end
  local damage = DAMAGE
  local crit = false
  local combo_step = tonumber(payload.comboStep) or 0
  local chain_hop = tonumber(payload.chainHop) or 0
  local ok_roll, roll = pcall(function()
    return engine.roll_player_attack({ comboStep = combo_step })
  end)
  if ok_roll and type(roll) == "table" then
    local rolled = tonumber(roll.damage) or 0
    if rolled > 0 then
      damage = rolled
    end
    crit = roll.crit == true
  end
  if chain_hop > 0 then
    damage = math.max(1, math.floor(damage * 0.5 + 0.5))
  end
  -- Training dummy: never dies.
  hp = math.max(1, hp - damage)
  engine.blackboard_set(key, hp)
  engine.blackboard_set("combat.lastHurtId", tostring(payload.hurtCombatId or "dummy_body"))
  engine.blackboard_set("combat.lastDummyId", id)
  engine.blackboard_set("combat.lastAttackerId", tostring(payload.attackerId or ""))
  engine.blackboard_set("combat.lastComboStep", combo_step)
  local hits = engine.blackboard_get("combat.hitCount")
  if type(hits) ~= "number" then
    hits = 0
  end
  engine.blackboard_set("combat.hitCount", hits + 1)

  tracked[id] = { hp = hp }
  upsert_chip(id, hp, true, payload)
  pcall(engine.animator_set_trigger, id, "hit")

  local nx, ny, nz = head_xyz(id, payload)
  if nx then
    pcall(engine.status_set_anchor, id, { x = nx, y = ny + COMBAT_TEXT_ABOVE_NAME, z = nz })
    pcall(engine.combat_text, {
      x = nx,
      y = ny + COMBAT_TEXT_ABOVE_NAME,
      z = nz,
      amount = damage,
      crit = crit,
      kind = crit and "crit" or "hit",
    })
    apply_weapon_on_hit(id, nx, ny + COMBAT_TEXT_ABOVE_NAME, nz)
  end

  engine.log("info", "dummy hit id=" .. id .. " hp=" .. tostring(hp) .. "/" .. tostring(DUMMY_MAX)
    .. " dmg=" .. tostring(damage) .. " combo=" .. tostring(combo_step) .. (crit and " crit" or ""))
end

function on_status_tick(payload_json)
  local payload = engine.json_decode(payload_json)
  if type(payload) ~= "table" then
    return
  end
  local id = tostring(payload.targetId or "")
  if id == "" or id == "player" then
    -- Player DoT HP is applied in C++ (HUD). Keep this handler for world targets.
    return
  end
  local amount = tonumber(payload.amount) or 0
  if amount <= 0 then
    return
  end

  -- Hostile NPC: stackable bleed/poison ticks drain combat.npcHp.*
  local npc_key = "combat.npcHp." .. id
  local npc_hp = engine.blackboard_get(npc_key)
  if type(npc_hp) == "number" then
    if engine.blackboard_get("combat.npcDead." .. id) == true then
      return
    end
    npc_hp = math.max(0, npc_hp - amount)
    engine.blackboard_set(npc_key, npc_hp)
    local status = tostring(payload.status or "")
    if status == "bleed" or status == "poison" or status == "burn" or status == "slow" then
      local stacks = tonumber(payload.stacks) or 1
      engine.blackboard_set("combat.npcStatus." .. id, status .. ":" .. tostring(stacks))
    end
    if npc_hp <= 0 then
      engine.blackboard_set("combat.npcDead." .. id, true)
      pcall(engine.animator_set_trigger, id, "die")
    end
    return
  end

  if not DUMMY_ANCHORS[id] and not tracked[id] then
    return
  end
  local key = "combat.dummyHp." .. id
  local hp = engine.blackboard_get(key)
  if type(hp) ~= "number" then
    hp = DUMMY_MAX
  end
  hp = math.max(1, hp - amount)
  engine.blackboard_set(key, hp)
  if not tracked[id] then
    tracked[id] = { hp = hp }
  else
    tracked[id].hp = hp
  end
  local status = tostring(payload.status or "")
  -- Status icons + duration tickers are drawn in C++ from StatusEffectRuntime.
  -- Keep a light blackboard hint for diagnostics only (not shown on the name plate).
  if status == "bleed" or status == "poison" or status == "burn" or status == "slow" then
    local stacks = tonumber(payload.stacks) or 1
    engine.blackboard_set("combat.dummyStatus." .. id, status .. ":" .. tostring(stacks))
  end
  upsert_chip(id, hp, false)
end

function on_update(payload_json)
  local payload = engine.json_decode(payload_json)
  local dt = 0.016
  if type(payload) == "table" and type(payload.dt) == "number" then
    dt = math.min(payload.dt, 0.1)
  end
  if dt <= 0 then
    return
  end
  -- F5 erases combat.* ; drop stale local HP so regen cannot rewrite it.
  local drop = {}
  for id, _ in pairs(tracked) do
    if type(engine.blackboard_get("combat.dummyHp." .. id)) ~= "number" then
      drop[#drop + 1] = id
    end
  end
  for _, id in ipairs(drop) do
    tracked[id] = nil
  end
  for id, state in pairs(tracked) do
    local hp = state.hp
    if type(hp) ~= "number" then
      hp = DUMMY_MAX
    end
    local nx, ny, nz = head_xyz(id)
    if nx then
      pcall(engine.status_set_anchor, id, { x = nx, y = ny + COMBAT_TEXT_ABOVE_NAME, z = nz })
    end
    if hp < DUMMY_MAX then
      hp = math.min(DUMMY_MAX, hp + REGEN_PER_SEC * dt)
      state.hp = hp
      engine.blackboard_set("combat.dummyHp." .. id, hp)
      upsert_chip(id, hp)
    end
  end
end
