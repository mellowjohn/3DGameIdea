# TICKET-0259: Armature gizmo + full joint list (subject + held)

- Epic: EPIC-0019
- Status: active
- Agent: cursor-agent
- Priority: P2
- Notion: (mirror when board is updated)

## Goal

Animation Studio shows a selectable armature for the subject skin and any skinned held item, so authors (and MCP) can pick and manipulate bones without hunting a single joint dropdown.

## Context links

- `context/features/animation-studio.md`
- EPIC-0019 (0248–0253, 0258)
- Dual-edit: DEC-0052

## Acceptance criteria

- [ ] Animation viewport can show/hide a skeleton overlay for the studio subject.
- [ ] Skinned held item joints appear on the overlay when gear is equipped in Studio.
- [ ] Clicking a joint sphere (or a row in the bone list) selects that joint and drives the bone gizmo.
- [ ] Diagnostics lists subject joints and held joints in one hierarchy-aware panel (with filter).
- [ ] Selecting a held joint opens the held draw clip for key edits; subject joints target the character clip.
- [ ] MCP: `list_joints`, `set_joint` (+ `source`), `set_skeleton` (`visible`/`labels`).

## Out of scope

- Full retargeter / IK solvers
- Multi-character sandbox cast beyond the single subject
- Replacing weld gizmo authoring path

## Dependencies

Soft: TICKET-0251 bone gizmo, TICKET-0258 skinned held weapons.

## Verification

- Rebuild `engine`.
- Animation tab → subject player → held shortbow → skeleton on → list shows player + bow bones → pick/select/move a bone; save override if dirty.
- `engine_animation_call` `list_joints` / `set_joint` / `set_skeleton`.

## What changed

- Summary: Animation Studio draws a subject + held-item armature, supports click-select and a hierarchy bone list, and routes subject vs held joint selection to the right edit clip for the bone gizmo/keys.
- Files: `src/rendering/render_app.cpp`, `src/automation/mcp_server.cpp` (tools JSON split for MSVC string limit), `context/features/animation-studio.md`, `context/planning/epics.md`, this stub.
- MCP: `list_joints`, `set_skeleton` (`visible`/`labels`), `set_joint` optional `source` (`subject`|`held`).
- Verification: `engine` target rebuild succeeded (Debug). Live MCP host may need Cursor reload for tool schema; lease acquired/released for rebuild.
- Leftover: Owner visual pass (click pick vs free-cam), Notion mirror.

## Agent notes

Editor/MCP process was killed → rebuild → `engine.exe mcp --project samples/open-world-rpg` restarted; build lease released.
