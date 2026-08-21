# TICKET-0264: Ease breakdowns + loop_report diagnostics

- Epic: EPIC-0019
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: (mirror when board is updated)

## Goal

Agents can insert LINEAR ease-in/out breakdowns between keys and get numeric loop/foot/hip diagnostics without changing the LINEAR|STEP runtime format.

## Context links

- `context/art/animation-craft.md`
- `context/features/animation-studio.md`
- TICKET-0261

## Acceptance criteria

- [x] `ease_segment` / `auto_breakdowns` inserts N LINEAR keys approximating ease-in / ease-out / ease-in-out between two times on selected joints.
- [x] `loop_report` flags first-vs-last pose/key seams.
- [x] Hip Y range / lowest foot Y series available when skinned world poses exist.
- [x] No CUBICSPLINE/Bezier added to runtime.
- [x] Docs + MCP schema + suite math tests.

## Out of scope

- Full Bezier / CUBICSPLINE (follow-on)
- Crossfade Idle preview (deferred)

## Dependencies

Soft: TICKET-0261, TICKET-0263.

## Verification

- Suite ease math + loop_report shape; live `loop_report` on Attack (seamFlagCount 0).

## What changed

- Summary: LINEAR ease breakdowns + loop/hip/foot report for agent timing polish.
- Files: `anim_studio_agent_ops.cpp` (`insert_ease_breakdowns`, `loop_report_json`), MCP kinds in `render_app.cpp`.
- Leftover: Hip/foot Y needs skinned world sample; channel-only seam still works.

## Agent notes

Shipped with 0261–0263.
