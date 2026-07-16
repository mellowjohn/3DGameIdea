# TICKET-0103: Blend trees + layered animation state machines

- Epic: EPIC-0008
- Status: proposed
- Agent: unassigned
- Priority: P2
- Notion: (see Notion Tickets DB by Ticket ID)

## Goal

Ship the C++ animator backend and authored `animator` component: controller assets with states, transitions, and parameters (blend trees / layered state machines as needed), plus a small Lua API so movement/combat/interaction scripts can drive animation without owning the graph ([DEC-0022](../../decisions/index.md#dec-0022-c-animator-backend-with-lua-drive-api)).

## Context links

- Design: [`../../features/animator.md`](../../features/animator.md)
- Decision: [DEC-0022](../../decisions/index.md#dec-0022-c-animator-backend-with-lua-drive-api)
- Clips: [`../../formats/animation-clip-assets.md`](../../formats/animation-clip-assets.md) (TICKET-0102)
- Components: DEC-0016 / DEC-0017
- Follow-ons: TICKET-0104 (root motion), 0105 (events → Lua), 0110 (M5 exit)

## Acceptance criteria

- [ ] Documented animator controller asset + `animator` component contract.
- [ ] Runtime evaluates states/transitions/parameters in C++; missing clips/transitions fail closed with entity-aware diagnostics.
- [ ] Lua can set parameters and request/crossfade states from sandbox handlers (movement/combat/interaction).
- [ ] Prefab/entity authoring follows existing component inherit/override paths (or a clearly staged subset).
- [ ] Named tests cover transitions, bad references, and Lua drive smoke; rebuild `engine`; context indexes updated.

## Out of scope

- Root motion (0104), animation events → Lua (0105) beyond a stub hook if required for smoke.
- IK/retarget (0106), audio (0107).
- Lua-authored transition graphs (rejected by DEC-0022).
- Production character art / final Squire rig.

## Dependencies

- TICKET-0102 (clip import + hot reload) done / approved.
- Blocks useful character feedback for combat/movement vertical slice work.

## Verification

Rebuild `engine`; named suites for animator + scripting drive; set Status to `needs-approval` after verification — never `done`.

## Agent notes

- 2026-07-15: Scope expanded from stub using DEC-0022 (C++ backend, Lua drive). Still `proposed` / P2 until 0102 is owner-approved and capacity pulls M5 follow-ons.
