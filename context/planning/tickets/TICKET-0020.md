# TICKET-0020: Main storyline beat sheet (campaign acts)

- Epic: EPIC-0003
- Status: needs-approval
- Agent: cursor-agent
- Priority: P1
- Notion: https://app.notion.com/p/39ad3efc569581e29cfff151e65996bc

## Goal

Extend story context with a campaign act / beat sheet that reconciles draft chapter-locked flow with the accepted seamless 4×4 km open world (DEC-0001), without inventing new runtime systems.

## Context links

- `context/story/campaign-beat-sheet.md` (deliverable)
- `context/story/sources/wrathful-conquest-act0.twee` (Act 0 Twine source)
- `context/story/story-vision.md`
- `context/story/prologue-and-opening.md`
- `context/story/index.md`
- `context/story/frangitur-the-great-evil.md`
- DEC-0001, DEC-0009, DEC-0021
- Related: TICKET-0021, TICKET-0022, TICKET-0023

## Acceptance criteria

- [x] Beat sheet in `context/story/` covering acts from prologue through late-game with named beats (not full scripts).
- [x] Explicit stance on open-world vs instanced chapters: soft gates + rare optional instances, minimize loads ([DEC-0021](../../decisions/index.md#dec-0021-soft-gates-with-rare-optional-instances)).
- [x] Marks established vs draft beats per story continuity conventions.
- [x] Cross-links factions/companions only where beats depend on them; no duplicate faction essays.
- [x] `story/index.md` updated.

## Out of scope

- Quest runtime schemas (M6) or dialogue graphs (M7).
- Full side-quest catalog (TICKET-0022).
- Engine/code changes / instance streaming implementation.

## Dependencies

- Act 0 detailed from Twine; mid/late acts draft where TICKET-0021 gaps block faction-dependent beats.

## Verification

- Doc review; continuity checklist applied.
- No engine rebuild.

## Agent notes

- 2026-07-15: Owner chose soft gates + rare instances (minimize loads) → DEC-0021. Act 0 from Twine HTML then full `.twee` (Calrenoth → Creotar). Archived `sources/wrathful-conquest-act0.twee`. Twine ends at Tutorial Completion; Acts 1–4 planning-only. Open: Creotar vs Creo, crystal location stubs, wake-up hub, Wild God chronology.
