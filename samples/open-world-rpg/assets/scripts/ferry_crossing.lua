-- Scripted ferry crossing stub for SQ-10 island ferry beat.
-- Attach to a pier trigger or run from script bindings during play test.

local ferry = {
    id = "island_ferry",
    speed = 4.5,
    phase = 0.0,
}

local function sample_route_t(progress)
    -- Simple straight crossing between two dock anchors (world XZ meters).
    local from_x, from_z = -12.0, 48.0
    local to_x, to_z = 18.0, 48.0
    local t = math.max(0.0, math.min(1.0, progress))
    return from_x + (to_x - from_x) * t, from_z + (to_z - from_z) * t
end

function ferry.update(dt, entity)
    ferry.phase = ferry.phase + dt * 0.05
    if ferry.phase > 1.0 then ferry.phase = 0.0 end
    local x, z = sample_route_t(ferry.phase)
    local surface = engine.sample_water_surface_y and engine.sample_water_surface_y(x, z)
    local y = surface or 0.0
    if entity and entity.set_position then
        entity.set_position(x, y + 0.35, z)
    end
end

return ferry
