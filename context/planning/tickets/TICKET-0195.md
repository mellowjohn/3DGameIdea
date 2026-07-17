# TICKET-0195: Safe reload after pull (World Forge + dirty-session rules)

- Epic: EPIC-0014
- Status: proposed
- Agent: unassigned
- Priority: P2
- Notion: (mirror after Notion MCP auth — Tickets DB)

## Goal

After a successful project **pull**, reload World Forge (and document/handle dirty Scene/Sculpt sessions) so the local editor shows remote story changes without corrupting in-memory authority.

## Context links

- [DEC-0037](../../decisions/index.md#dec-0037-git-backed-authoring-sync-in-editor)
- [`../features/authoring-git-sync.md`](../../features/authoring-git-sync.md)
- [`../architecture/content-vs-engine-workflows.md`](../../architecture/content-vs-engine-workflows.md) — live editor owns scene
- World Forge Reload path in editor (`apply_world_forge_operation`)

## Acceptance criteria

- [ ] Successful Pull offers or triggers World Forge reload when World Forge assets changed on disk.
- [ ] If Scene/Sculpt (or other in-memory owners) are dirty, prompt save/discard/cancel before applying disk changes that would clobber them; fail closed on cancel.
- [ ] Pull that only touches World Forge / non-scene files can reload World Forge without forcing a full editor restart when safe.
- [ ] Documented rules in `authoring-git-sync.md` for when a full restart is still required (e.g. bindings.script.json).
- [ ] Regression coverage for “dirty session blocks unsafe reload” where testable.

## Out of scope

- Live multi-user OT/CRDT co-editing
- Auto-merging conflicted JSON
- Player save migration

## Dependencies

- Soft: TICKET-0193/0194 for pull entry points; can land hooks callable from either
- Uses existing World Forge Reload

## Verification

Rebuild `engine`; two-clone manual: A pushes World Forge change; B pulls and sees updated quests/dialogues after reload without restart when applicable.

## What changed

(Fill before `needs-approval`.)

## Agent notes

Align with MCP live-editor rule: do not write open `.world.json` behind the editor’s back — same spirit for pull+reload.
