# TICKET-0022: Side-quest catalog (regions, hooks, rewards)

- Epic: EPIC-0003
- Status: needs-approval
- Agent: cursor-agent
- Priority: P1
- Notion: https://app.notion.com/p/39ad3efc56958158883deeab9f451164

## Goal

Create a durable side-quest inventory (IDs, regions, hooks, reward sketches) that future M6 quest runtime can reference — content planning only.

## Context links

- `context/story/side-quest-catalog.md` (deliverable)
- `context/story/index.md`
- `context/story/story-vision.md`
- `context/story/factions.md`
- `context/story/campaign-beat-sheet.md`
- `context/roadmap.md` (M6)
- Related: TICKET-0020, TICKET-0050 / 0112 (later runtime)

## Acceptance criteria

- [x] New `context/story/` doc with stable quest IDs, region/hook, faction touch if any, reward sketch, draft/established label.
- [x] At least a starter set covering multiple region types from story vision (not empty placeholder).
- [x] Explicit note that runtime schema is out of scope until M6 tickets.
- [x] `story/index.md` updated.

## Out of scope

- Quest asset schema, validation CLI, or editor tools.
- Main campaign beat sheet (TICKET-0020).

## Dependencies

- Soft: richer if TICKET-0021 gaps are known; proceeded with draft faction tags.

## Verification

- Doc review; IDs unique and linkable.
- No engine rebuild.

## Agent notes

- 2026-07-15: Owner chose lean catalog (~12). Consequential side quests may redirect mainline/faction allegiance (proposal). Added Rak Zulla easter egg (SQ-12). Wrote `side-quest-catalog.md`.
- 2026-07-15 (rework): Expanded every SQ with start conditions, ordered objectives, named outcome flags, concrete mainline/faction impact, rewards, and ignore state — no vibe-only stubs. Awaiting owner approval.
