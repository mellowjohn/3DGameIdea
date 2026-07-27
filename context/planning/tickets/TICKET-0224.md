# TICKET-0224: Split Act 0 dialogue into per-event trees

- Epic: EPIC-0006
- Status: ready
- Agent: unassigned
- Priority: P2
- Notion: https://app.notion.com/p/3a7d3efc56958174ac2cf838033c2732

## Goal

Replace the single monolithic Twine import `dlg_act0_wrathful_conquest` with **separate World Forge dialogue trees** keyed to Act 0 story beats / event timeline steps, so each encounter (meet Practice Keeper equivalents, Frangitur beats, keep approach, etc.) is authored and started independently instead of one mega-graph.

## Context links

- [`context/formats/world-forge-dialogues.md`](../../formats/world-forge-dialogues.md)
- [`context/formats/world-forge-quests.md`](../../formats/world-forge-quests.md) — DEC-0026 multi-stage dialogue hooks
- [`context/decisions/index.md`](../../decisions/index.md) — DEC-0026, DEC-0028, DEC-0045 (event timelines)
- Twine source: [`context/story/sources/wrathful-conquest-act0.twee`](../../story/sources/wrathful-conquest-act0.twee)
- Sample asset: `samples/open-world-rpg/assets/world-forge/dialogues.worldforge.json`
- Soft: TICKET-0221/0222 event timeline + `start_dialogue` steps; TICKET-0050 quest hooks

## Acceptance criteria

- [ ] Owner-approved beat list maps Twine / Act 0 beats → distinct tree ids (snake_case from display titles via `slugify_id`)
- [ ] `dialogues.worldforge.json` contains **≥2** Act 0 trees (not only `dlg_act0_wrathful_conquest`); each tree has a focused node set and `parentQuestId` / tags as appropriate
- [ ] Quest hooks (`dialogue.startId` and/or objective `dialogueId`) and/or event timeline `start_dialogue` steps point at the **correct** per-beat tree — not one catch-all
- [ ] Legacy mega-tree is either removed, marked deprecated in summary/tags, or reduced to a thin redirect stub documented in format notes
- [ ] Sandbox `dlg_sandbox_sample` remains separate and is **not** merged into Act 0 trees
- [ ] `world_forge` (or assets) suite + project validate pass; headless `DialogueRuntime` can start each new tree by id
- [ ] Docs: update [`world-forge-dialogues.md`](../../formats/world-forge-dialogues.md) seed table + Act 0 notes

## Out of scope

- Full Twine re-author of Act 1+
- Choice UI polish / tone-standing fields (already shipped separately)
- Voice / localization keys (TICKET-0118)
- Reworking the dialogue graph editor itself

## Dependencies

- Soft: TICKET-0221/0222 for timeline `start_dialogue` wiring
- Soft: TICKET-0050/0051 quest stage hooks
- Blocked by: none for a content-first split if hooks are updated in the same change

## Verification

- Rebuild not required for JSON-only content; rebuild `engine` if importer/runtime hook logic changes
- `engine_suite_tests --suite world_forge` (or assets) + `engine project-validate --project samples/open-world-rpg`
- MCP: `engine_dialogue_call` start each new tree id; confirm node/choice metadata matches beat
- Desktop: campaign Act 0 volumes/events start the expected short tree, not the old full Twine dump

## What changed

_(Fill before needs-approval.)_

## Agent notes

Owner ask 2026-07-24: Act 0 dialogue should be broken into separate pieces for multiple events — track as working item, do not leave as one imported mega-tree forever.
