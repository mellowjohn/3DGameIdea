# TICKET-0262: Multi-seek contact sheet for Animation Studio

- Epic: EPIC-0019
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: (mirror when board is updated)

## Goal

One MCP call seeks multiple times in the Animation viewport and writes a contact-sheet PNG under project `out/` so agents can judge arcs/overlap without N screenshot round-trips.

## Context links

- `context/features/animation-studio.md`
- `skills/live-editor-mcp/SKILL.md`
- TICKET-0261 inspect kinds

## Acceptance criteria

- [x] `engine_animation_call` `seek_times` accepts `times[]` (capped ~8–12).
- [x] Captures Animation viewport frames and composites a contact sheet to `out/`.
- [x] Response meta includes output path and per-slot times (via MCP wait until done).
- [x] Docs + MCP schema updated.

## Out of scope

- Full onion-skin UI (deferred)
- Replacing single `engine_editor_screenshot`

## Dependencies

Soft: Animation viewport (0248/0249), screenshot path.

## Verification

- Live: Attack → `seek_times` `[0.1,0.3,0.56]` → `out/attack-contact-verify-*.png` status=done.

## What changed

- Summary: Multi-frame contact sheet job advances after each present; MCP stdio waits on `seek_times` until `contact_sheet_status` is done.
- Files: `render_app.cpp` (job + RGBA capture), `mcp_server.cpp` (wait loop), `anim_studio_agent_ops` composite helper.
- Leftover: Onion skin deferred; requires Animation viewport active.

## Agent notes

Verified live during 0261–0264 delivery.
