-- MCP / playtest helpers for evt_act0_timeline_smoke

function on_timeline_smoke_start(payload_json)
  engine.start_event_timeline("evt_act0_timeline_smoke")
  if not engine.event_timeline_control_locked() then
    error("expected control lock immediately after start_event_timeline")
  end
end

function on_timeline_smoke_assert_locked(payload_json)
  if not engine.event_timeline_control_locked() then
    error("expected event_timeline_control_locked during active sequence")
  end
end

function on_timeline_smoke_cancel(payload_json)
  engine.cancel_event_timeline()
  if engine.event_timeline_control_locked() then
    error("expected unlock after cancel_event_timeline")
  end
end
