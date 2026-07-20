# Story Continuity Checklist

Reusable checklist for authors and agents when adding or revising docs under `context/story/`. Apply before treating new text as ready for review. Do not invent canon to “close” gaps — mark **draft** / **proposal** / **open** and route questions instead.

Related: [story index](index.md) · [faction gap review](factions.md#gaps-blocking-world-forge-schema-ticket-0011-and-mid-campaign-beats) · [open questions](../interviews/open-questions.md) · TICKET-0023

## Before writing

1. Read [index.md](index.md) Continuity Status and any linked docs you will touch.
2. Check [decisions/index.md](../decisions/index.md) for conflicts (especially DEC-0001 open world, DEC-0009 character creation).
3. Prefer extending an existing story file over creating a parallel essay on the same topic.

## Labeling (required)

- Mark new or uncertain claims as **established**, **draft**, **proposal**, or **open**.
- Do not upgrade draft/proposal to established without owner review.
- If a sentence mixes established fact with speculation, split or label the speculative part.

## Naming and identity

- Reuse existing proper names exactly (spelling, titles). Prefer **Nefarium Shroud** over variants.
- If a name is TBD (e.g. great orc leader), say so — do not invent a placeholder that looks final. World geography: **Tessera** is the primary land; do not invent a continent title above it ([DEC-0034](../decisions/index.md#dec-0034-tessera-is-the-worlds-primary-land)).
- Flag naming conflicts with other story docs in the doc’s Open Questions section.

## Decision conflicts

- Engine/product decisions override draft story structure when they conflict (e.g. seamless 4×4 km world vs chapter-locked instanced travel).
- Note the conflict in the story doc; do not silently rewrite the decision or the story to force agreement.

## Cross-doc links

- Link related story files instead of duplicating long lore passages.
- After substantive edits, update [index.md](index.md) if a new doc was added or Continuity Status needs a pointer.
- Faction data gaps belong in [factions.md](factions.md); Shroud/Nefarium conflicts in [nefarium-and-the-shroud.md](nefarium-and-the-shroud.md).

## Open questions routing

- Leave unresolved items in the document’s Open Questions (or equivalent) section.
- Also add blocking or cross-cutting items to [`context/interviews/open-questions.md`](../interviews/open-questions.md) when they affect World Forge, mid-campaign beats, or multiple docs.
- Dom-facing naming / geography / lore answers: update [`context/design/dom-open-questions.md`](../design/dom-open-questions.md) (P0 before P1 before P2); move finished rows to [`context/design/dom-answered-questions.md`](../design/dom-answered-questions.md).
- Never resolve an open question by assumption in the same change that introduces it.

## Smoke example (TICKET-0023)

Applied once to [factions.md](factions.md) after the TICKET-0021 gap review:

- Status table labels each major faction established/draft/open without promoting draft theology or warband names.
- Gaps and open questions point to interviews + related lore docs rather than inventing answers.
- Index Continuity Status links this checklist and the faction gap section.
