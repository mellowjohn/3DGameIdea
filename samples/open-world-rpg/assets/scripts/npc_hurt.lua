-- Combat-sandbox hostile NPC: drain NPC HP (never player HUD).
-- Death stops the C++ chase brain via combat.npcDead.<id>.
-- Player weapon onHit (bleed/poison/burn/slow) stacks on the NPC the same as dummies.
-- Lightning bolts may arrive with chainHop > 0 (half damage).

local NPC_MAX = 100
local DAMAGE = 10
local HEAD_OFFSET = 2.25
local COMBAT_TEXT_ABOVE_NAME = 0.55

local function combat_text_xyz(payload)
  local feet = payload.hurtEntityPosition
  if type(feet) == "table" then
    return tonumber(feet.x) or 0,
      (tonumber(feet.y) or 0) + HEAD_OFFSET + COMBAT_TEXT_ABOVE_NAME,
      tonumber(feet.z) or 0
  end
  local contact = payload.contactPoint
  if type(contact) == "table" then
    return tonumber(contact.x) or 0,
      (tonumber(contact.y) or 0) + COMBAT_TEXT_ABOVE_NAME,
      tonumber(contact.z) or 0
  end
  return nil
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

function on_npc_body_hit(payload_json)
  local payload, err = engine.json_decode(payload_json)
  if not payload then
    engine.log("error", "npc hurt decode failed: " .. tostring(err))
    return
  end

  local id = tostring(payload.hurtPlacementEntityId or "npc")
  if engine.blackboard_get("combat.npcDead." .. id) == true then
    return
  end
  if payload.blocked then
    engine.blackboard_set("combat.lastBlocked", true)
    engine.blackboard_set("combat.lastHurtId", tostring(payload.hurtCombatId or "npc_body"))
    engine.blackboard_set("combat.lastNpcId", id)
    engine.blackboard_set("combat.lastAttackerId", tostring(payload.attackerId or ""))
    local blocks = engine.blackboard_get("combat.blockCount")
    if type(blocks) ~= "number" then
      blocks = 0
    end
    engine.blackboard_set("combat.blockCount", blocks + 1)
    local x, y, z = combat_text_xyz(payload)
    if x then
      pcall(engine.combat_text, {
        x = x,
        y = y,
        z = z,
        text = "Blocked",
        kind = "hit",
      })
    end
    engine.log("info", "npc hurt blocked id=" .. id)
    return
  end
  local key = "combat.npcHp." .. id
  local hp = engine.blackboard_get(key)
  if type(hp) ~= "number" then
    hp = NPC_MAX
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

  hp = math.max(0, hp - damage)
  engine.blackboard_set(key, hp)
  engine.blackboard_set("combat.lastHurtId", tostring(payload.hurtCombatId or "npc_body"))
  engine.blackboard_set("combat.lastNpcId", id)
  engine.blackboard_set("combat.lastAttackerId", tostring(payload.attackerId or ""))
  local hits = engine.blackboard_get("combat.hitCount")
  if type(hits) ~= "number" then
    hits = 0
  end
  engine.blackboard_set("combat.hitCount", hits + 1)

  if hp <= 0 then
    engine.blackboard_set("combat.npcDead." .. id, true)
    pcall(engine.animator_set_trigger, id, "die")
  else
    pcall(engine.animator_set_trigger, id, "hit")
  end

  local x, y, z = combat_text_xyz(payload)
  if x then
    pcall(engine.status_set_anchor, id, { x = x, y = y, z = z })
    pcall(engine.combat_text, {
      x = x,
      y = y,
      z = z,
      amount = damage,
      crit = crit,
      kind = crit and "crit" or "hit",
    })
    apply_weapon_on_hit(id, x, y, z)
  end

  engine.log("info", "npc hit id=" .. id .. " hp=" .. tostring(hp) .. "/" .. tostring(NPC_MAX)
    .. " dmg=" .. tostring(damage) .. (crit and " crit" or "") .. (hp <= 0 and " dead" or ""))
end
