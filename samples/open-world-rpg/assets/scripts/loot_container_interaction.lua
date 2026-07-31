-- Loot bag / supply chest: Press E to search for icon-only Landfall finds.
-- Thin inventory grant (TICKET-0237) will consume blackboard loot.* keys later.
-- Catalog: assets/items/act0_landfall_items.json

local TABLES = {
  open_loot_bag = {
    id = "landfall_approach_pouch",
    label = "Press E to search pouch",
    entries = {
      { itemId = "field_bandage", count = 1, weight = 3 },
      { itemId = "soldiers_scrap_pouch", count = 1, weight = 2 },
      { itemId = "siege_tonic", count = 1, weight = 2 },
      { itemId = "crude_arrow", count = 5, weight = 2 },
      { itemId = "imperium_footsoldier_badge", count = 1, weight = 1 }
    }
  },
  open_supply_chest = {
    id = "landfall_supply_chest",
    label = "Press E to open chest",
    entries = {
      { itemId = "field_bandage", count = 2, weight = 2 },
      { itemId = "siege_tonic", count = 1, weight = 2 },
      { itemId = "muddied_keep_ring", count = 1, weight = 1 },
      { itemId = "soldiers_scrap_pouch", count = 1, weight = 2 },
      { itemId = "vein_iron_pendant", count = 1, weight = 1 }
    }
  }
}

local function weighted_pick(entries)
  local total = 0
  for _, e in ipairs(entries) do
    total = total + (tonumber(e.weight) or 1)
  end
  local roll = math.random() * total
  local acc = 0
  for _, e in ipairs(entries) do
    acc = acc + (tonumber(e.weight) or 1)
    if roll <= acc then
      return e
    end
  end
  return entries[#entries]
end

local function resolve_table(interaction_id)
  return TABLES[tostring(interaction_id or "")] or TABLES.open_loot_bag
end

local function handle(payload_json)
  local payload, err = engine.json_decode(payload_json)
  if not payload then
    engine.log("error", "loot_container decode failed: " .. tostring(err))
    return
  end

  local kind = tostring(payload.type or "")
  local interaction_id = tostring(payload.interactionId or "open_loot_bag")
  local table_def = resolve_table(interaction_id)
  engine.log("info", "loot_container " .. kind .. " id=" .. interaction_id)
  engine.blackboard_set("interaction.lastId", interaction_id)
  engine.blackboard_set("interaction.lastType", kind)

  if kind == "enter" then
    engine.blackboard_set("interact.prompt", true)
    engine.blackboard_set("interact.id", interaction_id)
    engine.blackboard_set("interact.label", table_def.label)
    if payload.contactPoint then
      engine.blackboard_set("interact.x", payload.contactPoint.x or 0)
      engine.blackboard_set("interact.y", (payload.contactPoint.y or 0) + 1.4)
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

  local entries = table_def.entries
  local grants = {}
  local first = weighted_pick(entries)
  table.insert(grants, first)
  if math.random() < 0.45 then
    local second = weighted_pick(entries)
    if second.itemId ~= first.itemId then
      table.insert(grants, second)
    end
  end

  engine.blackboard_set("loot.lastTableId", table_def.id)
  engine.blackboard_set("loot.lastInteractionId", interaction_id)
  engine.blackboard_set("loot.iconOnly", false)
  engine.blackboard_set("loot.grantCount", tostring(#grants))
  local summary = {}
  for i, g in ipairs(grants) do
    local item_id = tostring(g.itemId)
    local count = tonumber(g.count) or 1
    engine.blackboard_set("loot.grant" .. tostring(i) .. ".itemId", item_id)
    engine.blackboard_set("loot.grant" .. tostring(i) .. ".count", tostring(count))
    local granted = pcall(function()
      engine.inventory_grant(item_id, count)
    end)
    if not granted then
      engine.log("warn", "loot_container inventory_grant failed for " .. item_id)
    end
    table.insert(summary, tostring(count) .. "x " .. item_id)
  end
  engine.blackboard_set("interact.prompt", false)
  engine.log("info", "loot_container grant [" .. table.concat(summary, ", ") .. "]")
end

function on_open_loot_bag(payload_json)
  handle(payload_json)
end

function on_open_supply_chest(payload_json)
  handle(payload_json)
end
